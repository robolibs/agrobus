#pragma once

#include "pgn_defs.hpp"
#include "types.hpp"

namespace agrobus::net {

    // ─── Address constants (ISO 11783-5) ─────────────────────────────────────────
    inline constexpr Address NULL_ADDRESS = 0xFE;
    inline constexpr Address BROADCAST_ADDRESS = 0xFF;
    inline constexpr Address MAX_ADDRESS = 0xFD;

    // ─── Timing constants (ms) ───────────────────────────────────────────────────
    inline constexpr u32 ADDRESS_CLAIM_TIMEOUT_MS = 250;
    inline constexpr u32 ADDRESS_CLAIM_RTXD_MAX_MS = 153; // RTxD max: 0.6ms × 255 (ISO 11783-5 §3.4)
    inline constexpr u32 HEARTBEAT_INTERVAL_MS = 100;
    inline constexpr u32 TP_TIMEOUT_TR_MS = 200; // Response timeout (ISO 11783-3 §5.13.3)
    inline constexpr u32 TP_TIMEOUT_T1_MS = 750;
    inline constexpr u32 TP_TIMEOUT_T2_MS = 1250;
    inline constexpr u32 TP_TIMEOUT_T3_MS = 1250;
    inline constexpr u32 TP_TIMEOUT_T4_MS = 1050;
    inline constexpr u32 ETP_TIMEOUT_T1_MS = 750;
    inline constexpr u32 TP_BAM_INTER_PACKET_MS = 50; // Min 50ms between BAM DT packets (J1939-21)

    // ─── Power management constants (ISO 11783-9 §4.6) ──────────────────────────
    inline constexpr u32 POWER_SHUTDOWN_MIN_MS = 2000;    // Minimum 2s after key-off
    inline constexpr u32 POWER_MAINTAIN_REPEAT_MS = 1000; // Repeat maintain power every 1s
    inline constexpr u32 POWER_MAX_EXTENSION_MS = 180000; // Maximum 3 minutes power extension

    // ─── Protocol limits ─────────────────────────────────────────────────────────
    inline constexpr u32 CAN_DATA_LENGTH = 8;
    inline constexpr u32 TP_MAX_DATA_LENGTH = 1785;
    inline constexpr u32 ETP_MAX_DATA_LENGTH = 117'440'505;
    inline constexpr u32 TP_BYTES_PER_FRAME = 7;
    inline constexpr u32 TP_MAX_PACKETS_PER_CTS = 16;
    inline constexpr u32 FAST_PACKET_MAX_DATA = 223;

} // namespace agrobus::net
