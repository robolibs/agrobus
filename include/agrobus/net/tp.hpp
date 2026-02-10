#pragma once

#include "session.hpp"
#include <agrobus/net/error.hpp>
#include <agrobus/net/event.hpp>
#include <agrobus/net/frame.hpp>
#include <agrobus/net/identifier.hpp>
#include <datapod/datapod.hpp>
#include <echo/echo.hpp>

namespace agrobus::net {

    // ─── TP Abort reason codes (ISO 11783-3) ────────────────────────────────────
    namespace tp_abort {
        inline constexpr u8 ALREADY_IN_PROGRESS = 1;
        inline constexpr u8 NO_RESOURCES = 2;
        inline constexpr u8 TIMEOUT = 3;
        inline constexpr u8 CTS_WHILE_SENDING = 4;
        inline constexpr u8 MAX_RETRANSMITS = 5;
        inline constexpr u8 UNEXPECTED_DT = 6;
        inline constexpr u8 BAD_SEQUENCE = 7;
        inline constexpr u8 DUPLICATE_SEQUENCE = 8;
        inline constexpr u8 TOTAL_SIZE_TOO_BIG = 9;
    } // namespace tp_abort

    // ─── TP Session timer state tracking ────────────────────────────────────────
    enum class TPSessionState : u8 { Idle, WaitForCTS, Sending, WaitForEndOfMsgAck, Complete, Aborted, TimedOut };

    // ─── TP Hold timer constant ─────────────────────────────────────────────────
    inline constexpr u32 TP_T_HOLD_MS = 500; // CTS keepalive interval when paused

    // ─── Per-session timer tracking ─────────────────────────────────────────────
    struct TPTimerSession {
        u32 last_activity_ms = 0;
        TPSessionState timer_state = TPSessionState::Idle;
        u8 abort_reason = 0;
        u32 cts_keepalive_timer_ms = 0;
        Address source = NULL_ADDRESS;
        Address destination = NULL_ADDRESS;
        PGN pgn = 0;
        u8 port = 0;
        bool receiver_paused = false; // True when receiver sent CTS(0)

        bool is_active() const noexcept {
            return timer_state != TPSessionState::Idle && timer_state != TPSessionState::Complete &&
                   timer_state != TPSessionState::Aborted && timer_state != TPSessionState::TimedOut;
        }
    };

    // ─── TP Connection Management byte codes ─────────────────────────────────────
    namespace tp_cm {
        inline constexpr u8 RTS = 0x10;   // Request To Send
        inline constexpr u8 CTS = 0x11;   // Clear To Send
        inline constexpr u8 EOMA = 0x13;  // End Of Message Acknowledgment
        inline constexpr u8 BAM = 0x20;   // Broadcast Announce Message
        inline constexpr u8 ABORT = 0xFF; // Connection Abort
    } // namespace tp_cm

    // ─── Transport Protocol (8-1785 bytes) ───────────────────────────────────────
    class TransportProtocol {
        dp::Vector<TransportSession> sessions_;
        dp::Vector<TPTimerSession> timer_sessions_;

      public:
        static constexpr u32 MAX_DATA_LENGTH = TP_MAX_DATA_LENGTH;
        static constexpr u32 BYTES_PER_FRAME = TP_BYTES_PER_FRAME;

        // ─── Initiate a send ─────────────────────────────────────────────────────
        Result<dp::Vector<Frame>> send(PGN pgn, const dp::Vector<u8> &data, Address source, Address dest, u8 port = 0,
                                       Priority priority = Priority::Lowest) {
            if (data.size() > MAX_DATA_LENGTH) {
                echo::category("isobus.transport.tp")
                    .error("data exceeds TP max: size=", data.size(), " max=", MAX_DATA_LENGTH);
                return Result<dp::Vector<Frame>>::err(Error(ErrorCode::BufferOverflow, "data exceeds TP max"));
            }
            if (data.size() <= CAN_DATA_LENGTH) {
                return Result<dp::Vector<Frame>>::err(Error::invalid_state("use single frame for <= 8 bytes"));
            }

            // Check for existing session - key by (src, dst, pgn, direction, port)
            for (const auto &s : sessions_) {
                if (s.source_address == source && s.destination_address == dest && s.pgn == pgn &&
                    s.direction == TransportDirection::Transmit && s.can_port == port) {
                    echo::category("isobus.transport.tp")
                        .error("session already active: pgn=", pgn, " src=", static_cast<u8>(source),
                               " dst=", static_cast<u8>(dest));
                    return Result<dp::Vector<Frame>>::err(Error(ErrorCode::SessionExists, "session already active"));
                }
            }

            dp::Vector<Frame> frames;
            TransportSession session;
            session.direction = TransportDirection::Transmit;
            session.pgn = pgn;
            session.data = data;
            session.total_bytes = static_cast<u32>(data.size());
            session.source_address = source;
            session.destination_address = dest;
            session.can_port = port;
            session.priority = priority;

            if (dest == BROADCAST_ADDRESS) {
                // BAM mode
                session.state = SessionState::SendingData;
                frames.push_back(make_bam(session));
                echo::category("isobus.transport.tp").debug("BAM started: pgn=", pgn, " bytes=", data.size());
            } else {
                // Connection mode - send RTS
                session.state = SessionState::WaitingForCTS;
                frames.push_back(make_rts(session));
                echo::category("isobus.transport.tp").debug("RTS sent: pgn=", pgn, " bytes=", data.size());
            }

            sessions_.push_back(std::move(session));
            return Result<dp::Vector<Frame>>::ok(std::move(frames));
        }

        // ─── Process incoming frame ──────────────────────────────────────────────
        dp::Vector<Frame> process_frame(const Frame &frame, u8 port = 0) {
            dp::Vector<Frame> responses;
            PGN pgn = frame.pgn();

            if (pgn == PGN_TP_CM) {
                echo::category("isobus.transport.tp")
                    .trace("CM frame received: src=", static_cast<u8>(frame.source()), " ctrl=", frame.data[0]);
                responses = handle_cm(frame, port);
            } else if (pgn == PGN_TP_DT) {
                echo::category("isobus.transport.tp")
                    .trace("DT frame received: src=", static_cast<u8>(frame.source()), " seq=", frame.data[0]);
                responses = handle_dt(frame, port);
            }

            return responses;
        }

        // ─── Update timers ───────────────────────────────────────────────────────
        dp::Vector<Frame> update(u32 elapsed_ms) {
            dp::Vector<Frame> frames;

            for (auto it = sessions_.begin(); it != sessions_.end();) {
                it->timer_ms += elapsed_ms;

                // Generate data frames for BAM (one per update, per J1939 timing)
                if (it->is_broadcast() && it->state == SessionState::SendingData &&
                    it->direction == TransportDirection::Transmit) {
                    // BAM inter-packet delay: 50-200ms per J1939-21
                    if (it->timer_ms >= TP_BAM_INTER_PACKET_MS) {
                        it->timer_ms = 0;
                        auto data_frames = generate_data_frames(*it, 1);
                        for (auto &f : data_frames)
                            frames.push_back(std::move(f));

                        if (it->bytes_transferred >= it->total_bytes) {
                            it->state = SessionState::Complete;
                            on_complete.emit(*it);
                            it = sessions_.erase(it);
                            continue;
                        }
                    }
                }

                // Timeout checking
                bool timed_out = false;
                switch (it->state) {
                case SessionState::WaitingForCTS:
                    timed_out = it->timer_ms >= TP_TIMEOUT_T3_MS;
                    break;
                case SessionState::WaitingForData:
                    timed_out = it->timer_ms >= TP_TIMEOUT_T1_MS;
                    break;
                case SessionState::WaitingForEndOfMsg:
                    timed_out = it->timer_ms >= TP_TIMEOUT_T3_MS;
                    break;
                case SessionState::ReceivingData:
                    // BAM RX timeout
                    timed_out = it->timer_ms >= TP_TIMEOUT_T1_MS;
                    break;
                default:
                    break;
                }

                if (timed_out) {
                    echo::category("isobus.transport.tp").warn("Session timeout: pgn=", it->pgn);
                    it->state = SessionState::Aborted;
                    on_abort.emit(*it, TransportAbortReason::Timeout);
                    if (!it->is_broadcast()) {
                        frames.push_back(make_abort(*it, TransportAbortReason::Timeout));
                    }
                    it = sessions_.erase(it);
                    continue;
                }

                ++it;
            }

            return frames;
        }

        // ─── Get next data frames for CM sessions ────────────────────────────────
        dp::Vector<Frame> get_pending_data_frames() {
            dp::Vector<Frame> frames;
            for (auto &session : sessions_) {
                if (session.state == SessionState::SendingData && session.direction == TransportDirection::Transmit &&
                    !session.is_broadcast()) {
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

        dp::Vector<TransportSession *> active_sessions() {
            dp::Vector<TransportSession *> result;
            for (auto &s : sessions_)
                result.push_back(&s);
            return result;
        }

        // ─── Timer session tracking ────────────────────────────────────────────
        dp::Vector<TPTimerSession> &timer_sessions() { return timer_sessions_; }
        const dp::Vector<TPTimerSession> &timer_sessions() const { return timer_sessions_; }

        // Start tracking a session with explicit timer state
        void track_session(Address src, Address dst, PGN pgn, TPSessionState state, u8 port = 0) {
            TPTimerSession ts;
            ts.source = src;
            ts.destination = dst;
            ts.pgn = pgn;
            ts.timer_state = state;
            ts.port = port;
            ts.last_activity_ms = 0;
            ts.cts_keepalive_timer_ms = 0;
            ts.abort_reason = 0;
            ts.receiver_paused = false;
            timer_sessions_.push_back(ts);
        }

        // Mark receiver as paused (sent CTS with max_packets=0)
        void set_receiver_paused(Address src, Address dst, PGN pgn, u8 port = 0) {
            for (auto &ts : timer_sessions_) {
                if (ts.source == src && ts.destination == dst && ts.pgn == pgn && ts.port == port) {
                    ts.receiver_paused = true;
                    ts.cts_keepalive_timer_ms = 0;
                    break;
                }
            }
        }

        // ─── Update timer sessions with elapsed time ───────────────────────────
        dp::Vector<Frame> update_sessions(u32 elapsed_ms) {
            dp::Vector<Frame> frames;

            for (auto it = timer_sessions_.begin(); it != timer_sessions_.end();) {
                // Skip non-active sessions
                if (!it->is_active()) {
                    ++it;
                    continue;
                }

                it->last_activity_ms += elapsed_ms;

                // Determine timeout threshold based on state
                u32 timeout_threshold = 0;
                switch (it->timer_state) {
                case TPSessionState::WaitForCTS:
                    timeout_threshold = TP_TIMEOUT_T3_MS;
                    break;
                case TPSessionState::Sending:
                    timeout_threshold = TP_TIMEOUT_T4_MS;
                    break;
                case TPSessionState::WaitForEndOfMsgAck:
                    timeout_threshold = TP_TIMEOUT_T3_MS;
                    break;
                default:
                    ++it;
                    continue;
                }

                // Check for timeout
                if (it->last_activity_ms >= timeout_threshold) {
                    echo::category("isobus.transport.tp")
                        .warn("Timer session timeout: pgn=", it->pgn, " state=", static_cast<u8>(it->timer_state));
                    it->timer_state = TPSessionState::TimedOut;
                    it->abort_reason = tp_abort::TIMEOUT;

                    // Emit timeout event
                    on_session_timeout.emit(*it);

                    // Generate abort frame for unicast sessions
                    if (it->destination != BROADCAST_ADDRESS) {
                        Frame f;
                        f.id = Identifier::encode(Priority::Lowest, PGN_TP_CM, it->source, it->destination);
                        f.data[0] = tp_cm::ABORT;
                        f.data[1] = tp_abort::TIMEOUT;
                        f.data[2] = 0xFF;
                        f.data[3] = 0xFF;
                        f.data[4] = 0xFF;
                        f.data[5] = static_cast<u8>(it->pgn & 0xFF);
                        f.data[6] = static_cast<u8>((it->pgn >> 8) & 0xFF);
                        f.data[7] = static_cast<u8>((it->pgn >> 16) & 0xFF);
                        f.length = 8;
                        frames.push_back(f);
                    }

                    ++it;
                    continue;
                }

                // CTS keepalive for paused receivers
                if (it->receiver_paused) {
                    it->cts_keepalive_timer_ms += elapsed_ms;
                    if (it->cts_keepalive_timer_ms >= TP_T_HOLD_MS) {
                        it->cts_keepalive_timer_ms = 0;
                        // Send CTS with num_packets=0 (hold) to keep connection alive
                        frames.push_back(make_cts(it->destination, it->source, 0, 0, it->pgn));
                        echo::category("isobus.transport.tp").trace("CTS keepalive sent: pgn=", it->pgn);
                    }
                }

                ++it;
            }

            return frames;
        }

        // Reset activity timer for a tracked session
        void reset_session_timer(Address src, Address dst, PGN pgn, u8 port = 0) {
            for (auto &ts : timer_sessions_) {
                if (ts.source == src && ts.destination == dst && ts.pgn == pgn && ts.port == port) {
                    ts.last_activity_ms = 0;
                    break;
                }
            }
        }

        // Update the state of a tracked session
        void set_session_state(Address src, Address dst, PGN pgn, TPSessionState state, u8 port = 0) {
            for (auto &ts : timer_sessions_) {
                if (ts.source == src && ts.destination == dst && ts.pgn == pgn && ts.port == port) {
                    ts.timer_state = state;
                    ts.last_activity_ms = 0;
                    break;
                }
            }
        }

        // ─── Events ──────────────────────────────────────────────────────────────
        Event<TransportSession &> on_complete;
        Event<TransportSession &, TransportAbortReason> on_abort;
        Event<TPTimerSession &> on_session_timeout;

      private:
        Frame make_bam(const TransportSession &s) const noexcept {
            Frame f;
            f.id = Identifier::encode(Priority::Lowest, PGN_TP_CM, s.source_address, BROADCAST_ADDRESS);
            f.data[0] = tp_cm::BAM;
            f.data[1] = static_cast<u8>(s.total_bytes & 0xFF);
            f.data[2] = static_cast<u8>((s.total_bytes >> 8) & 0xFF);
            f.data[3] = static_cast<u8>(s.total_packets());
            f.data[4] = 0xFF;
            f.data[5] = static_cast<u8>(s.pgn & 0xFF);
            f.data[6] = static_cast<u8>((s.pgn >> 8) & 0xFF);
            f.data[7] = static_cast<u8>((s.pgn >> 16) & 0xFF);
            f.length = 8;
            return f;
        }

        Frame make_rts(const TransportSession &s) const noexcept {
            Frame f;
            f.id = Identifier::encode(Priority::Lowest, PGN_TP_CM, s.source_address, s.destination_address);
            f.data[0] = tp_cm::RTS;
            f.data[1] = static_cast<u8>(s.total_bytes & 0xFF);
            f.data[2] = static_cast<u8>((s.total_bytes >> 8) & 0xFF);
            f.data[3] = static_cast<u8>(s.total_packets());
            f.data[4] = static_cast<u8>(s.max_packets_per_cts);
            f.data[5] = static_cast<u8>(s.pgn & 0xFF);
            f.data[6] = static_cast<u8>((s.pgn >> 8) & 0xFF);
            f.data[7] = static_cast<u8>((s.pgn >> 16) & 0xFF);
            f.length = 8;
            return f;
        }

        Frame make_cts(Address src, Address dst, u8 num_packets, u8 next_seq, PGN pgn) const noexcept {
            Frame f;
            f.id = Identifier::encode(Priority::Lowest, PGN_TP_CM, src, dst);
            f.data[0] = tp_cm::CTS;
            f.data[1] = num_packets;
            f.data[2] = next_seq;
            f.data[3] = 0xFF;
            f.data[4] = 0xFF;
            f.data[5] = static_cast<u8>(pgn & 0xFF);
            f.data[6] = static_cast<u8>((pgn >> 8) & 0xFF);
            f.data[7] = static_cast<u8>((pgn >> 16) & 0xFF);
            f.length = 8;
            return f;
        }

        Frame make_eoma(Address src, Address dst, u32 total_bytes, u8 total_packets, PGN pgn) const noexcept {
            Frame f;
            f.id = Identifier::encode(Priority::Lowest, PGN_TP_CM, src, dst);
            f.data[0] = tp_cm::EOMA;
            f.data[1] = static_cast<u8>(total_bytes & 0xFF);
            f.data[2] = static_cast<u8>((total_bytes >> 8) & 0xFF);
            f.data[3] = total_packets;
            f.data[4] = 0xFF;
            f.data[5] = static_cast<u8>(pgn & 0xFF);
            f.data[6] = static_cast<u8>((pgn >> 8) & 0xFF);
            f.data[7] = static_cast<u8>((pgn >> 16) & 0xFF);
            f.length = 8;
            return f;
        }

        Frame make_abort(const TransportSession &s, TransportAbortReason reason) const noexcept {
            Frame f;
            f.id = Identifier::encode(Priority::Lowest, PGN_TP_CM, s.source_address, s.destination_address);
            f.data[0] = tp_cm::ABORT;
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
                f.id = Identifier::encode(Priority::Lowest, PGN_TP_DT, session.source_address,
                                          session.destination_address);
                f.data[0] = session.last_sequence + 1;
                session.last_sequence = f.data[0];

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

        // Find a session by full key (src, dst, pgn, direction, port)
        TransportSession *find_session(Address src, Address dst, PGN pgn, TransportDirection dir, u8 port) {
            for (auto &s : sessions_) {
                if (s.source_address == src && s.destination_address == dst && s.pgn == pgn && s.direction == dir &&
                    s.can_port == port) {
                    return &s;
                }
            }
            return nullptr;
        }

        // Find an RX session matching a DT frame source
        TransportSession *find_rx_session(Address src, Address dst, u8 port) {
            for (auto &s : sessions_) {
                if (s.direction == TransportDirection::Receive && s.source_address == src && s.can_port == port &&
                    (s.state == SessionState::WaitingForData || s.state == SessionState::ReceivingData)) {
                    // For connection-mode, also match destination
                    if (!s.is_broadcast() && s.destination_address != dst) {
                        continue;
                    }
                    return &s;
                }
            }
            return nullptr;
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
            case tp_cm::RTS: {
                u16 msg_size = static_cast<u16>(frame.data[1]) | (static_cast<u16>(frame.data[2]) << 8);
                u8 total_packets = frame.data[3];
                u8 max_per_cts = frame.data[4];

                // Check for duplicate session
                if (find_session(src, dst, cm_pgn, TransportDirection::Receive, port)) {
                    // Already have a session for this - abort
                    TransportSession tmp;
                    tmp.source_address = dst;
                    tmp.destination_address = src;
                    tmp.pgn = cm_pgn;
                    responses.push_back(make_abort(tmp, TransportAbortReason::AlreadyInSession));
                    break;
                }

                TransportSession session;
                session.direction = TransportDirection::Receive;
                session.state = SessionState::WaitingForData;
                session.pgn = cm_pgn;
                session.total_bytes = msg_size;
                session.source_address = src;
                session.destination_address = dst;
                session.can_port = port;
                session.priority = frame.priority();
                session.max_packets_per_cts =
                    max_per_cts > TP_MAX_PACKETS_PER_CTS ? TP_MAX_PACKETS_PER_CTS : max_per_cts;
                session.data.resize(msg_size, 0xFF);
                session.cts_window_start = 1; // First packet expected

                u8 cts_count =
                    (total_packets < session.max_packets_per_cts) ? total_packets : session.max_packets_per_cts;
                session.cts_window_size = cts_count;
                responses.push_back(make_cts(dst, src, cts_count, 1, cm_pgn));

                sessions_.push_back(std::move(session));
                echo::category("isobus.transport.tp").debug("RTS received: pgn=", cm_pgn, " bytes=", msg_size);
                break;
            }
            case tp_cm::CTS: {
                u8 num_packets = frame.data[1];
                u8 next_seq = frame.data[2];

                for (auto &s : sessions_) {
                    if (s.direction == TransportDirection::Transmit && s.source_address == dst &&
                        s.destination_address == src && s.pgn == cm_pgn && s.state == SessionState::WaitingForCTS &&
                        s.can_port == port) {
                        if (num_packets == 0) {
                            // CTS hold: receiver is busy, stay in WaitingForCTS
                            s.timer_ms = 0;
                        } else {
                            // Validate CTS parameters per ISO 11783-3
                            u32 remaining_packets = s.total_packets() - (next_seq - 1);
                            if (next_seq == 0 || next_seq > s.total_packets()) {
                                // Invalid next_seq - abort
                                echo::category("isobus.transport.tp")
                                    .warn("CTS invalid next_seq=", next_seq, " total_packets=", s.total_packets());
                                s.state = SessionState::Aborted;
                                on_abort.emit(s, TransportAbortReason::BadSequence);
                                responses.push_back(make_abort(s, TransportAbortReason::BadSequence));
                                erase_session(&s);
                                break;
                            }
                            // Clamp num_packets to remaining data
                            u8 clamped_packets =
                                (num_packets > remaining_packets) ? static_cast<u8>(remaining_packets) : num_packets;
                            s.state = SessionState::SendingData;
                            s.packets_to_send = clamped_packets;
                            // Set bytes_transferred to match the requested next_seq
                            s.bytes_transferred = static_cast<u32>(next_seq - 1) * 7;
                            s.last_sequence = next_seq - 1;
                            s.timer_ms = 0;
                        }
                        echo::category("isobus.transport.tp")
                            .debug("CTS received: packets=", num_packets, " next_seq=", next_seq);
                        break;
                    }
                }
                break;
            }
            case tp_cm::EOMA: {
                for (auto it = sessions_.begin(); it != sessions_.end(); ++it) {
                    if (it->direction == TransportDirection::Transmit && it->source_address == dst &&
                        it->destination_address == src && it->pgn == cm_pgn && it->can_port == port) {
                        it->state = SessionState::Complete;
                        on_complete.emit(*it);
                        sessions_.erase(it);
                        echo::category("isobus.transport.tp").debug("EOMA received - session complete");
                        break;
                    }
                }
                break;
            }
            case tp_cm::BAM: {
                u16 msg_size = static_cast<u16>(frame.data[1]) | (static_cast<u16>(frame.data[2]) << 8);

                TransportSession session;
                session.direction = TransportDirection::Receive;
                session.state = SessionState::ReceivingData;
                session.pgn = cm_pgn;
                session.total_bytes = msg_size;
                session.source_address = src;
                session.destination_address = BROADCAST_ADDRESS;
                session.can_port = port;
                session.priority = frame.priority();
                session.data.resize(msg_size, 0xFF);

                sessions_.push_back(std::move(session));
                echo::category("isobus.transport.tp").debug("BAM received: pgn=", cm_pgn, " bytes=", msg_size);
                break;
            }
            case tp_cm::ABORT: {
                TransportAbortReason reason = static_cast<TransportAbortReason>(frame.data[1]);
                for (auto it = sessions_.begin(); it != sessions_.end(); ++it) {
                    if (it->pgn == cm_pgn && it->can_port == port &&
                        ((it->source_address == dst && it->destination_address == src) ||
                         (it->source_address == src && it->destination_address == dst))) {
                        it->state = SessionState::Aborted;
                        on_abort.emit(*it, reason);
                        sessions_.erase(it);
                        echo::category("isobus.transport.tp").warn("Abort received: reason=", static_cast<u8>(reason));
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
            u8 seq = frame.data[0];

            auto *session = find_rx_session(src, dst, port);
            if (!session) {
                return responses;
            }

            // Validate sequence number
            u8 expected_seq = session->last_sequence + 1;
            if (seq != expected_seq) {
                if (seq <= session->last_sequence && seq != 0) {
                    // Duplicate - abort with reason
                    echo::category("isobus.transport.tp").warn("Duplicate DT seq=", seq, " expected=", expected_seq);
                    if (!session->is_broadcast()) {
                        responses.push_back(make_abort(*session, TransportAbortReason::DuplicateSequence));
                    }
                    session->state = SessionState::Aborted;
                    on_abort.emit(*session, TransportAbortReason::DuplicateSequence);
                    erase_session(session);
                    return responses;
                }
                if (seq > expected_seq) {
                    // Out of order - abort
                    echo::category("isobus.transport.tp").warn("Out-of-order DT seq=", seq, " expected=", expected_seq);
                    if (!session->is_broadcast()) {
                        responses.push_back(make_abort(*session, TransportAbortReason::BadSequence));
                    }
                    session->state = SessionState::Aborted;
                    on_abort.emit(*session, TransportAbortReason::BadSequence);
                    erase_session(session);
                    return responses;
                }
            }

            // Copy data bytes
            u32 offset = (static_cast<u32>(seq) - 1) * 7;
            for (u8 i = 0; i < 7 && (offset + i) < session->total_bytes; ++i) {
                session->data[offset + i] = frame.data[i + 1];
            }
            session->bytes_transferred = offset + 7;
            if (session->bytes_transferred > session->total_bytes) {
                session->bytes_transferred = session->total_bytes;
            }
            session->last_sequence = seq;
            session->timer_ms = 0;

            // Check if complete
            if (session->bytes_transferred >= session->total_bytes) {
                session->state = SessionState::Complete;

                if (!session->is_broadcast()) {
                    responses.push_back(make_eoma(session->destination_address, session->source_address,
                                                  session->total_bytes, static_cast<u8>(session->total_packets()),
                                                  session->pgn));
                }

                on_complete.emit(*session);
                erase_session(session);
                echo::category("isobus.transport.tp").debug("Session complete: pgn=", session->pgn);
            } else if (!session->is_broadcast()) {
                // Check if CTS window is exhausted
                u8 packets_in_window = seq - (session->cts_window_start - 1);
                if (packets_in_window >= session->cts_window_size) {
                    // Send next CTS
                    u32 remaining_packets = session->total_packets() - seq;
                    u8 next_count = (remaining_packets < session->max_packets_per_cts)
                                        ? static_cast<u8>(remaining_packets)
                                        : session->max_packets_per_cts;
                    session->cts_window_start = seq + 1;
                    session->cts_window_size = next_count;
                    responses.push_back(make_cts(session->destination_address, session->source_address, next_count,
                                                 seq + 1, session->pgn));
                }
            }

            return responses;
        }
    };
} // namespace agrobus::net
