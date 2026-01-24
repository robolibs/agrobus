#pragma once

#include "session.hpp"
#include <agrobus/net/error.hpp>
#include <agrobus/net/event.hpp>
#include <agrobus/net/frame.hpp>
#include <agrobus/net/identifier.hpp>
#include <datapod/datapod.hpp>
#include <echo/echo.hpp>

namespace agrobus::net {

    // ─── ETP Connection Management byte codes ────────────────────────────────────
    namespace etp_cm {
        inline constexpr u8 RTS = 0x14;   // Request To Send
        inline constexpr u8 CTS = 0x15;   // Clear To Send
        inline constexpr u8 DPO = 0x16;   // Data Packet Offset
        inline constexpr u8 EOMA = 0x17;  // End Of Message Acknowledgment
        inline constexpr u8 ABORT = 0xFF; // Connection Abort
    } // namespace etp_cm

    // ─── Extended Transport Protocol (>1785 bytes, up to ~117MB) ─────────────────
    class ExtendedTransportProtocol {
        dp::Vector<TransportSession> sessions_;

      public:
        static constexpr u32 MAX_DATA_LENGTH = ETP_MAX_DATA_LENGTH;
        static constexpr u32 BYTES_PER_FRAME = TP_BYTES_PER_FRAME;

        Result<dp::Vector<Frame>> send(PGN pgn, const dp::Vector<u8> &data, Address source, Address dest, u8 port = 0,
                                       Priority priority = Priority::Lowest) {
            if (data.size() > MAX_DATA_LENGTH) {
                echo::category("isobus.transport.etp")
                    .error("data exceeds ETP max: size=", data.size(), " max=", MAX_DATA_LENGTH);
                return Result<dp::Vector<Frame>>::err(Error(ErrorCode::BufferOverflow, "data exceeds ETP max"));
            }
            if (data.size() <= TP_MAX_DATA_LENGTH) {
                return Result<dp::Vector<Frame>>::err(Error::invalid_state("use TP for <= 1785 bytes"));
            }
            if (dest == BROADCAST_ADDRESS) {
                return Result<dp::Vector<Frame>>::err(Error::invalid_state("ETP does not support broadcast"));
            }

            // Check for existing session by full key
            for (const auto &s : sessions_) {
                if (s.source_address == source && s.destination_address == dest && s.pgn == pgn &&
                    s.direction == TransportDirection::Transmit && s.can_port == port) {
                    echo::category("isobus.transport.etp")
                        .error("session already active: pgn=", pgn, " src=", static_cast<u8>(source),
                               " dst=", static_cast<u8>(dest));
                    return Result<dp::Vector<Frame>>::err(Error(ErrorCode::SessionExists, "session already active"));
                }
            }

            dp::Vector<Frame> frames;
            TransportSession session;
            session.direction = TransportDirection::Transmit;
            session.state = SessionState::WaitingForCTS;
            session.pgn = pgn;
            session.data = data;
            session.total_bytes = static_cast<u32>(data.size());
            session.source_address = source;
            session.destination_address = dest;
            session.can_port = port;
            session.priority = priority;

            frames.push_back(make_rts(session));
            sessions_.push_back(std::move(session));

            echo::category("isobus.transport.etp").debug("ETP RTS sent: pgn=", pgn, " bytes=", data.size());
            return Result<dp::Vector<Frame>>::ok(std::move(frames));
        }

        dp::Vector<Frame> process_frame(const Frame &frame, u8 port = 0) {
            dp::Vector<Frame> responses;
            PGN pgn = frame.pgn();

            if (pgn == PGN_ETP_CM) {
                echo::category("isobus.transport.etp")
                    .trace("ETP CM frame received: src=", static_cast<u8>(frame.source()), " ctrl=", frame.data[0]);
                responses = handle_cm(frame, port);
            } else if (pgn == PGN_ETP_DT) {
                echo::category("isobus.transport.etp")
                    .trace("ETP DT frame received: src=", static_cast<u8>(frame.source()), " seq=", frame.data[0]);
                responses = handle_dt(frame, port);
            }

            return responses;
        }

        dp::Vector<Frame> update(u32 elapsed_ms) {
            dp::Vector<Frame> frames;

            for (auto it = sessions_.begin(); it != sessions_.end();) {
                it->timer_ms += elapsed_ms;

                bool timed_out = false;
                if (it->state == SessionState::WaitingForCTS || it->state == SessionState::WaitingForData ||
                    it->state == SessionState::WaitingForEndOfMsg) {
                    timed_out = it->timer_ms >= ETP_TIMEOUT_T1_MS;
                }

                if (timed_out) {
                    echo::category("isobus.transport.etp").warn("ETP timeout: pgn=", it->pgn);
                    it->state = SessionState::Aborted;
                    on_abort.emit(*it, TransportAbortReason::Timeout);
                    frames.push_back(make_abort(*it, TransportAbortReason::Timeout));
                    it = sessions_.erase(it);
                    continue;
                }

                ++it;
            }

            return frames;
        }

        dp::Vector<Frame> get_pending_data_frames() {
            dp::Vector<Frame> frames;
            for (auto &session : sessions_) {
                if (session.state == SessionState::SendingData && session.direction == TransportDirection::Transmit) {
                    // Send DPO first
                    frames.push_back(make_dpo(session));
                    // Then data packets (sequence resets per DPO group)
                    auto data_frames = generate_data_frames(session, session.packets_to_send);
                    for (auto &f : data_frames)
                        frames.push_back(std::move(f));

                    if (session.bytes_transferred >= session.total_bytes) {
                        // All data sent, wait for EOMA
                        session.state = SessionState::WaitingForEndOfMsg;
                        session.timer_ms = 0;
                    } else {
                        // Window complete, wait for next CTS
                        session.state = SessionState::WaitingForCTS;
                        session.timer_ms = 0;
                    }
                }
            }
            return frames;
        }

        Event<TransportSession &> on_complete;
        Event<TransportSession &, TransportAbortReason> on_abort;

      private:
        Frame make_rts(const TransportSession &s) const noexcept {
            Frame f;
            f.id = Identifier::encode(Priority::Lowest, PGN_ETP_CM, s.source_address, s.destination_address);
            f.data[0] = etp_cm::RTS;
            f.data[1] = static_cast<u8>(s.total_bytes & 0xFF);
            f.data[2] = static_cast<u8>((s.total_bytes >> 8) & 0xFF);
            f.data[3] = static_cast<u8>((s.total_bytes >> 16) & 0xFF);
            f.data[4] = static_cast<u8>((s.total_bytes >> 24) & 0xFF);
            f.data[5] = static_cast<u8>(s.pgn & 0xFF);
            f.data[6] = static_cast<u8>((s.pgn >> 8) & 0xFF);
            f.data[7] = static_cast<u8>((s.pgn >> 16) & 0xFF);
            f.length = 8;
            return f;
        }

        Frame make_dpo(const TransportSession &s) const noexcept {
            u32 packet_offset = s.bytes_transferred / 7;
            Frame f;
            f.id = Identifier::encode(Priority::Lowest, PGN_ETP_CM, s.source_address, s.destination_address);
            f.data[0] = etp_cm::DPO;
            f.data[1] = s.packets_to_send;
            f.data[2] = static_cast<u8>(packet_offset & 0xFF);
            f.data[3] = static_cast<u8>((packet_offset >> 8) & 0xFF);
            f.data[4] = static_cast<u8>((packet_offset >> 16) & 0xFF);
            f.data[5] = static_cast<u8>(s.pgn & 0xFF);
            f.data[6] = static_cast<u8>((s.pgn >> 8) & 0xFF);
            f.data[7] = static_cast<u8>((s.pgn >> 16) & 0xFF);
            f.length = 8;
            return f;
        }

        Frame make_cts(Address src, Address dst, u8 num_packets, u32 next_packet, PGN pgn) const noexcept {
            Frame f;
            f.id = Identifier::encode(Priority::Lowest, PGN_ETP_CM, src, dst);
            f.data[0] = etp_cm::CTS;
            f.data[1] = num_packets;
            f.data[2] = static_cast<u8>(next_packet & 0xFF);
            f.data[3] = static_cast<u8>((next_packet >> 8) & 0xFF);
            f.data[4] = static_cast<u8>((next_packet >> 16) & 0xFF);
            f.data[5] = static_cast<u8>(pgn & 0xFF);
            f.data[6] = static_cast<u8>((pgn >> 8) & 0xFF);
            f.data[7] = static_cast<u8>((pgn >> 16) & 0xFF);
            f.length = 8;
            return f;
        }

        Frame make_eoma(Address src, Address dst, u32 total_bytes, PGN pgn) const noexcept {
            Frame f;
            f.id = Identifier::encode(Priority::Lowest, PGN_ETP_CM, src, dst);
            f.data[0] = etp_cm::EOMA;
            f.data[1] = static_cast<u8>(total_bytes & 0xFF);
            f.data[2] = static_cast<u8>((total_bytes >> 8) & 0xFF);
            f.data[3] = static_cast<u8>((total_bytes >> 16) & 0xFF);
            f.data[4] = static_cast<u8>((total_bytes >> 24) & 0xFF);
            f.data[5] = static_cast<u8>(pgn & 0xFF);
            f.data[6] = static_cast<u8>((pgn >> 8) & 0xFF);
            f.data[7] = static_cast<u8>((pgn >> 16) & 0xFF);
            f.length = 8;
            return f;
        }

        Frame make_abort(const TransportSession &s, TransportAbortReason reason) const noexcept {
            Frame f;
            f.id = Identifier::encode(Priority::Lowest, PGN_ETP_CM, s.source_address, s.destination_address);
            f.data[0] = etp_cm::ABORT;
            f.data[1] = static_cast<u8>(reason);
            f.data[2] = 0xFF;
            f.data[3] = 0xFF;
            f.data[4] = 0xFF;
            f.data[5] = static_cast<u8>(s.pgn & 0xFF);
            f.data[6] = static_cast<u8>((s.pgn >> 8) & 0xFF);
            f.data[7] = static_cast<u8>((s.pgn >> 16) & 0xFF);
            f.length = 8;
            return f;
        }

        dp::Vector<Frame> generate_data_frames(TransportSession &session, u8 count) {
            dp::Vector<Frame> frames;
            for (u8 i = 0; i < count && session.bytes_transferred < session.total_bytes; ++i) {
                Frame f;
                f.id = Identifier::encode(Priority::Lowest, PGN_ETP_DT, session.source_address,
                                          session.destination_address);
                f.data[0] = i + 1; // Sequence number (1-based, resets per DPO group)

                for (u8 j = 0; j < 7; ++j) {
                    u32 idx = session.bytes_transferred + j;
                    f.data[j + 1] = (idx < session.total_bytes) ? session.data[idx] : 0xFF;
                }
                f.length = 8;

                session.bytes_transferred += 7;
                if (session.bytes_transferred > session.total_bytes) {
                    session.bytes_transferred = session.total_bytes;
                }

                frames.push_back(f);
            }
            return frames;
        }

        void erase_session(TransportSession *session) {
            for (auto it = sessions_.begin(); it != sessions_.end(); ++it) {
                if (&(*it) == session) {
                    sessions_.erase(it);
                    return;
                }
            }
        }

        dp::Vector<Frame> handle_cm(const Frame &frame, u8 port) {
            dp::Vector<Frame> responses;
            u8 control_byte = frame.data[0];
            Address src = frame.source();
            Address dst = frame.destination();

            PGN cm_pgn = static_cast<u32>(frame.data[5]) | (static_cast<u32>(frame.data[6]) << 8) |
                         (static_cast<u32>(frame.data[7]) << 16);

            switch (control_byte) {
            case etp_cm::RTS: {
                u32 msg_size = static_cast<u32>(frame.data[1]) | (static_cast<u32>(frame.data[2]) << 8) |
                               (static_cast<u32>(frame.data[3]) << 16) | (static_cast<u32>(frame.data[4]) << 24);

                TransportSession session;
                session.direction = TransportDirection::Receive;
                session.state = SessionState::WaitingForData;
                session.pgn = cm_pgn;
                session.total_bytes = msg_size;
                session.source_address = src;
                session.destination_address = dst;
                session.can_port = port;
                session.priority = frame.priority();
                session.data.resize(msg_size, 0xFF);

                // Send CTS: request first window of packets
                u8 packets = TP_MAX_PACKETS_PER_CTS;
                u32 next_pkt = 1;
                session.cts_window_size = packets;
                responses.push_back(make_cts(dst, src, packets, next_pkt, cm_pgn));

                sessions_.push_back(std::move(session));
                echo::category("isobus.transport.etp").debug("ETP RTS received: pgn=", cm_pgn, " bytes=", msg_size);
                break;
            }
            case etp_cm::CTS: {
                u8 num_packets = frame.data[1];
                u32 next_pkt = static_cast<u32>(frame.data[2]) | (static_cast<u32>(frame.data[3]) << 8) |
                               (static_cast<u32>(frame.data[4]) << 16);

                for (auto &s : sessions_) {
                    if (s.direction == TransportDirection::Transmit && s.source_address == dst &&
                        s.destination_address == src && s.pgn == cm_pgn && s.state == SessionState::WaitingForCTS &&
                        s.can_port == port) {
                        if (num_packets == 0) {
                            // CTS hold
                            s.timer_ms = 0;
                        } else {
                            s.state = SessionState::SendingData;
                            s.packets_to_send = num_packets;
                            // Resume at the packet offset specified by CTS
                            s.bytes_transferred = (next_pkt - 1) * 7;
                            s.timer_ms = 0;
                        }
                        echo::category("isobus.transport.etp")
                            .debug("ETP CTS: packets=", num_packets, " next_pkt=", next_pkt);
                        break;
                    }
                }
                break;
            }
            case etp_cm::DPO: {
                // DPO from sender: update receiver's expected data offset
                u8 num_packets = frame.data[1];
                u32 packet_offset = static_cast<u32>(frame.data[2]) | (static_cast<u32>(frame.data[3]) << 8) |
                                    (static_cast<u32>(frame.data[4]) << 16);

                for (auto &s : sessions_) {
                    if (s.direction == TransportDirection::Receive && s.source_address == src &&
                        s.destination_address == dst && s.pgn == cm_pgn && s.can_port == port) {
                        s.dpo_packet_offset = packet_offset;
                        s.cts_window_size = num_packets;
                        s.last_sequence = 0; // Reset sequence counter for new DPO group
                        s.timer_ms = 0;
                        echo::category("isobus.transport.etp")
                            .debug("ETP DPO: offset=", packet_offset, " packets=", num_packets);
                        break;
                    }
                }
                break;
            }
            case etp_cm::EOMA: {
                for (auto it = sessions_.begin(); it != sessions_.end(); ++it) {
                    if (it->direction == TransportDirection::Transmit && it->source_address == dst &&
                        it->destination_address == src && it->pgn == cm_pgn && it->can_port == port) {
                        it->state = SessionState::Complete;
                        on_complete.emit(*it);
                        sessions_.erase(it);
                        echo::category("isobus.transport.etp").debug("ETP complete");
                        break;
                    }
                }
                break;
            }
            case etp_cm::ABORT: {
                TransportAbortReason reason = static_cast<TransportAbortReason>(frame.data[1]);
                for (auto it = sessions_.begin(); it != sessions_.end(); ++it) {
                    if (it->pgn == cm_pgn && it->can_port == port &&
                        ((it->source_address == dst && it->destination_address == src) ||
                         (it->source_address == src && it->destination_address == dst))) {
                        echo::category("isobus.transport.etp")
                            .warn("ETP abort received: pgn=", cm_pgn, " reason=", static_cast<u8>(reason));
                        it->state = SessionState::Aborted;
                        on_abort.emit(*it, reason);
                        sessions_.erase(it);
                        break;
                    }
                }
                break;
            }
            }
            return responses;
        }

        dp::Vector<Frame> handle_dt(const Frame &frame, u8 port) {
            dp::Vector<Frame> responses;
            Address src = frame.source();
            Address dst = frame.destination();
            u8 seq = frame.data[0]; // Sequence within current DPO group

            // Find RX session
            TransportSession *session = nullptr;
            for (auto &s : sessions_) {
                if (s.direction == TransportDirection::Receive && s.source_address == src &&
                    s.destination_address == dst && s.can_port == port && s.state == SessionState::WaitingForData) {
                    session = &s;
                    break;
                }
            }
            if (!session) {
                return responses;
            }

            // Validate sequence (within DPO group, 1-based)
            u8 expected_seq = session->last_sequence + 1;
            if (seq != expected_seq) {
                echo::category("isobus.transport.etp").warn("ETP bad seq=", seq, " expected=", expected_seq);
                responses.push_back(make_abort(*session, TransportAbortReason::BadSequence));
                session->state = SessionState::Aborted;
                on_abort.emit(*session, TransportAbortReason::BadSequence);
                erase_session(session);
                return responses;
            }

            // Calculate byte offset using DPO packet offset
            u32 byte_offset = (session->dpo_packet_offset + static_cast<u32>(seq) - 1) * 7;
            for (u8 i = 0; i < 7 && (byte_offset + i) < session->total_bytes; ++i) {
                session->data[byte_offset + i] = frame.data[i + 1];
            }
            u32 end = byte_offset + 7;
            if (end > session->total_bytes) {
                end = session->total_bytes;
            }
            session->bytes_transferred = end;
            session->last_sequence = seq;
            session->timer_ms = 0;

            if (session->bytes_transferred >= session->total_bytes) {
                // Complete - send EOMA
                session->state = SessionState::Complete;
                responses.push_back(make_eoma(session->destination_address, session->source_address,
                                              session->total_bytes, session->pgn));
                on_complete.emit(*session);
                erase_session(session);
                echo::category("isobus.transport.etp").debug("ETP RX complete");
            } else if (seq >= session->cts_window_size) {
                // Window exhausted - send next CTS
                u32 next_pkt = session->dpo_packet_offset + seq + 1;
                u32 remaining_packets = (session->total_bytes - session->bytes_transferred + 6) / 7;
                u8 next_count = (remaining_packets < TP_MAX_PACKETS_PER_CTS) ? static_cast<u8>(remaining_packets)
                                                                             : TP_MAX_PACKETS_PER_CTS;
                responses.push_back(make_cts(session->destination_address, session->source_address, next_count,
                                             next_pkt, session->pgn));
                session->cts_window_size = next_count;
            }

            return responses;
        }
    };
} // namespace agrobus::net
