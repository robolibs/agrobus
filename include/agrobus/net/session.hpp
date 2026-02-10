#pragma once

#include <agrobus/net/constants.hpp>
#include <agrobus/net/control_function.hpp>
#include <agrobus/net/types.hpp>
#include <datapod/datapod.hpp>

namespace agrobus::net {

    // ─── Transport direction ─────────────────────────────────────────────────────
    enum class TransportDirection { Transmit, Receive };

    // ─── Transport abort reasons ─────────────────────────────────────────────────
    enum class TransportAbortReason : u8 {
        None = 0,
        Timeout = 1,
        AlreadyInSession = 2,
        ResourcesUnavailable = 3,
        BadSequence = 4,
        UnexpectedDataSize = 5,
        DuplicateSequence = 6,
        MaxRetransmitsExceeded = 7,
        UnexpectedPGN = 8,
        ConnectionModeError = 9
    };

    // ─── Transport session state ─────────────────────────────────────────────────
    enum class SessionState {
        None,
        WaitingForCTS,
        SendingData,
        WaitingForEndOfMsg,
        ReceivingData,
        WaitingForData,
        Complete,
        Aborted
    };

    // ─── Transport session ───────────────────────────────────────────────────────
    struct TransportSession {
        TransportDirection direction = TransportDirection::Receive;
        SessionState state = SessionState::None;
        PGN pgn = 0;
        dp::Vector<u8> data;
        u32 total_bytes = 0;
        u32 bytes_transferred = 0;
        u8 source_address = NULL_ADDRESS;
        u8 destination_address = BROADCAST_ADDRESS;
        u8 can_port = 0;
        Priority priority = Priority::Default;

        // Sequence tracking
        u8 last_sequence = 0;
        u8 packets_to_send = 0;
        u8 next_packet_to_send = 0;
        u8 max_packets_per_cts = TP_MAX_PACKETS_PER_CTS;

        // CTS windowing (receiver-side tracking)
        u8 cts_window_start = 1;
        u8 cts_window_size = 0;

        // ETP: DPO packet offset for current window
        u32 dpo_packet_offset = 0;

        // Timing
        u32 timer_ms = 0;

        f32 progress() const noexcept {
            if (total_bytes == 0)
                return 0.0f;
            return static_cast<f32>(bytes_transferred) / static_cast<f32>(total_bytes);
        }

        u32 total_packets() const noexcept { return (total_bytes + TP_BYTES_PER_FRAME - 1) / TP_BYTES_PER_FRAME; }

        bool is_broadcast() const noexcept { return destination_address == BROADCAST_ADDRESS; }

        bool is_complete() const noexcept { return state == SessionState::Complete; }
    };
} // namespace agrobus::net
