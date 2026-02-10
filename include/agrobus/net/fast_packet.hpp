#pragma once

#include "session.hpp"
#include <agrobus/net/error.hpp>
#include <agrobus/net/event.hpp>
#include <agrobus/net/frame.hpp>
#include <agrobus/net/identifier.hpp>
#include <agrobus/net/message.hpp>
#include <datapod/datapod.hpp>
#include <echo/echo.hpp>

namespace agrobus::net {

    // ─── NMEA2000 Fast Packet Protocol ──────────────────────────────────────────
    // Used for messages 9-223 bytes on NMEA2000 networks
    // First frame: [seq_counter:3|frame_counter:5][total_bytes][6 data bytes]
    // Subsequent:  [seq_counter:3|frame_counter:5][7 data bytes]
    class FastPacketProtocol {
        struct FastPacketSession {
            PGN pgn = 0;
            dp::Vector<u8> data;
            u32 total_bytes = 0;
            u32 bytes_received = 0;
            u8 source_address = NULL_ADDRESS;
            u8 sequence_counter = 0;
            u8 expected_frame = 0;
            u32 timer_ms = 0;
        };

        dp::Vector<FastPacketSession> rx_sessions_;
        u8 tx_sequence_counter_ = 0;

      public:
        static constexpr u32 MAX_DATA_LENGTH = FAST_PACKET_MAX_DATA;
        static constexpr u32 FIRST_FRAME_DATA = 6;
        static constexpr u32 SUBSEQUENT_FRAME_DATA = 7;

        // ─── Send a fast packet message ─────────────────────────────────────────
        Result<dp::Vector<Frame>> send(PGN pgn, const dp::Vector<u8> &data, Address source) {
            if (data.size() > MAX_DATA_LENGTH) {
                return Result<dp::Vector<Frame>>::err(Error(ErrorCode::BufferOverflow, "data exceeds fast packet max"));
            }
            if (data.size() <= CAN_DATA_LENGTH) {
                return Result<dp::Vector<Frame>>::err(Error::invalid_state("use single frame for <= 8 bytes"));
            }

            dp::Vector<Frame> frames;
            u8 seq = (tx_sequence_counter_++ & 0x07) << 5;
            u8 total_frames = 1 + static_cast<u8>((data.size() - FIRST_FRAME_DATA + SUBSEQUENT_FRAME_DATA - 1) /
                                                  SUBSEQUENT_FRAME_DATA);

            // First frame
            Frame first;
            first.id = Identifier::encode(Priority::Default, pgn, source, BROADCAST_ADDRESS);
            first.data[0] = seq | 0x00; // frame counter = 0
            first.data[1] = static_cast<u8>(data.size());
            for (u8 i = 0; i < 6 && i < data.size(); ++i) {
                first.data[i + 2] = data[i];
            }
            for (u8 i = static_cast<u8>(data.size()); i < 6; ++i) {
                first.data[i + 2] = 0xFF;
            }
            first.length = 8;
            frames.push_back(first);

            // Subsequent frames
            usize offset = FIRST_FRAME_DATA;
            for (u8 frame_num = 1; frame_num < total_frames; ++frame_num) {
                Frame f;
                f.id = Identifier::encode(Priority::Default, pgn, source, BROADCAST_ADDRESS);
                f.data[0] = seq | frame_num;
                for (u8 i = 0; i < 7; ++i) {
                    f.data[i + 1] = (offset + i < data.size()) ? data[offset + i] : 0xFF;
                }
                f.length = 8;
                frames.push_back(f);
                offset += SUBSEQUENT_FRAME_DATA;
            }

            echo::category("isobus.transport.fp").debug("Fast packet sent: pgn=", pgn, " bytes=", data.size());
            return Result<dp::Vector<Frame>>::ok(std::move(frames));
        }

        // ─── Process incoming frame ──────────────────────────────────────────────
        dp::Optional<Message> process_frame(const Frame &frame) {
            u8 frame_counter = frame.data[0] & 0x1F;
            u8 seq_counter = (frame.data[0] >> 5) & 0x07;
            Address src = frame.source();
            PGN pgn = frame.pgn();

            if (frame_counter == 0) {
                // First frame of a new fast packet
                u8 total_bytes = frame.data[1];

                FastPacketSession session;
                session.pgn = pgn;
                session.total_bytes = total_bytes;
                session.source_address = src;
                session.sequence_counter = seq_counter;
                session.expected_frame = 1;
                session.data.resize(total_bytes, 0xFF);

                u8 copy_len =
                    (total_bytes < FIRST_FRAME_DATA) ? static_cast<u8>(total_bytes) : static_cast<u8>(FIRST_FRAME_DATA);
                for (u8 i = 0; i < copy_len; ++i) {
                    session.data[i] = frame.data[i + 2];
                }
                session.bytes_received = copy_len;

                if (session.bytes_received >= session.total_bytes) {
                    // Single-frame fast packet (unlikely but possible)
                    return make_message(session);
                }

                // Remove any existing session for same source/pgn
                remove_session(src, pgn);
                rx_sessions_.push_back(std::move(session));
                return dp::nullopt;
            }

            // Subsequent frame
            for (auto it = rx_sessions_.begin(); it != rx_sessions_.end(); ++it) {
                if (it->source_address == src && it->pgn == pgn && it->sequence_counter == seq_counter) {
                    if (frame_counter != it->expected_frame) {
                        // Bad sequence - discard
                        echo::category("isobus.transport.fp")
                            .warn("Bad sequence: expected=", it->expected_frame, " got=", frame_counter);
                        rx_sessions_.erase(it);
                        return dp::nullopt;
                    }

                    usize offset = FIRST_FRAME_DATA + (frame_counter - 1) * SUBSEQUENT_FRAME_DATA;
                    for (u8 i = 0; i < 7 && (offset + i) < it->total_bytes; ++i) {
                        it->data[offset + i] = frame.data[i + 1];
                    }
                    it->bytes_received = static_cast<u32>(offset + 7);
                    if (it->bytes_received > it->total_bytes) {
                        it->bytes_received = it->total_bytes;
                    }
                    it->expected_frame++;
                    it->timer_ms = 0;

                    if (it->bytes_received >= it->total_bytes) {
                        auto msg = make_message(*it);
                        rx_sessions_.erase(it);
                        return msg;
                    }
                    return dp::nullopt;
                }
            }

            return dp::nullopt;
        }

        void update(u32 elapsed_ms) {
            for (auto it = rx_sessions_.begin(); it != rx_sessions_.end();) {
                it->timer_ms += elapsed_ms;
                if (it->timer_ms >= TP_TIMEOUT_T1_MS) {
                    echo::category("isobus.transport.fp").warn("Fast packet timeout: pgn=", it->pgn);
                    it = rx_sessions_.erase(it);
                } else {
                    ++it;
                }
            }
        }

        Event<const Message &> on_message;

      private:
        void remove_session(Address src, PGN pgn) {
            for (auto it = rx_sessions_.begin(); it != rx_sessions_.end(); ++it) {
                if (it->source_address == src && it->pgn == pgn) {
                    rx_sessions_.erase(it);
                    return;
                }
            }
        }

        Message make_message(const FastPacketSession &session) const {
            Message msg;
            msg.pgn = session.pgn;
            msg.source = session.source_address;
            msg.destination = BROADCAST_ADDRESS;
            msg.priority = Priority::Default;
            msg.data = session.data;
            return msg;
        }
    };
} // namespace agrobus::net
