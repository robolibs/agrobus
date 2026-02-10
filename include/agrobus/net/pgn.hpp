#pragma once

#include "constants.hpp"
#include "types.hpp"
#include <datapod/datapod.hpp>

namespace agrobus::net {

    // ─── PGN metadata ────────────────────────────────────────────────────────────
    struct PGNInfo {
        PGN pgn;
        const char *name;
        u32 data_length;
        u32 default_priority;
        bool is_broadcast;
    };

    // ─── PGN lookup table ────────────────────────────────────────────────────────
    inline constexpr PGNInfo PGN_TABLE[] = {
        {PGN_REQUEST, "Request", 3, 6, false},
        {PGN_ADDRESS_CLAIMED, "Address Claimed", 8, 6, true},
        {PGN_COMMANDED_ADDRESS, "Commanded Address", 9, 6, false},
        {PGN_TP_CM, "TP.CM", 8, 7, false},
        {PGN_TP_DT, "TP.DT", 8, 7, false},
        {PGN_ETP_CM, "ETP.CM", 8, 7, false},
        {PGN_ETP_DT, "ETP.DT", 8, 7, false},
        {PGN_ACKNOWLEDGMENT, "Acknowledgment", 8, 6, false},
        {PGN_DM1, "DM1", 0, 6, true},
        {PGN_DM2, "DM2", 0, 6, true},
        {PGN_DM3, "DM3", 0, 6, true},
        {PGN_DM11, "DM11", 8, 6, true},
        {PGN_HEARTBEAT, "Heartbeat", 8, 6, true},
        {PGN_TIME_DATE, "Time/Date", 8, 6, true},
        {PGN_VEHICLE_SPEED, "Vehicle Speed", 8, 6, true},
        {PGN_WHEEL_SPEED, "Wheel Speed", 8, 6, true},
        {PGN_GROUND_SPEED, "Ground Speed", 8, 6, true},
        {PGN_MACHINE_SPEED, "Machine Speed", 8, 6, true},
        {PGN_LANGUAGE_COMMAND, "Language Command", 8, 6, true},
        {PGN_MAINTAIN_POWER, "Maintain Power", 8, 6, true},
        {PGN_GUIDANCE_MACHINE, "Guidance Machine", 8, 3, true},
        {PGN_GUIDANCE_SYSTEM, "Guidance System", 8, 3, true},
        {PGN_SHORTCUT_BUTTON, "Shortcut Button", 8, 6, true},
        {PGN_VT_TO_ECU, "VT to ECU", 8, 6, false},
        {PGN_ECU_TO_VT, "ECU to VT", 8, 6, false},
        {PGN_TC_TO_ECU, "TC to ECU", 8, 6, false},
        {PGN_ECU_TO_TC, "ECU to TC", 8, 6, false},
        {PGN_WORKING_SET_MASTER, "Working Set Master", 8, 6, true},
        {PGN_GNSS_POSITION, "GNSS Position Rapid", 8, 2, true},
        {PGN_GNSS_COG_SOG, "GNSS COG/SOG", 8, 2, true},
        {PGN_GNSS_POSITION_DETAIL, "GNSS Position Data", 0, 6, true},
    };

    inline constexpr usize PGN_TABLE_SIZE = sizeof(PGN_TABLE) / sizeof(PGN_TABLE[0]);

    // ─── PGN utility functions ───────────────────────────────────────────────────
    inline dp::Optional<PGNInfo> pgn_lookup(PGN pgn) noexcept {
        for (usize i = 0; i < PGN_TABLE_SIZE; ++i) {
            if (PGN_TABLE[i].pgn == pgn) {
                return PGN_TABLE[i];
            }
        }
        return dp::nullopt;
    }

    // Returns true if PGN uses PDU2 format (PF >= 240), meaning it is broadcast/global
    inline bool pgn_is_pdu2(PGN pgn) noexcept { return (pgn >> 8 & 0xFF) >= 240; }

    // Deprecated alias - use pgn_is_pdu2() instead
    inline bool pgn_is_broadcast(PGN pgn) noexcept { return pgn_is_pdu2(pgn); }

    inline bool pgn_is_valid(PGN pgn) noexcept { return pgn <= 0x3FFFF; }

    inline u8 pgn_pdu_format(PGN pgn) noexcept { return static_cast<u8>((pgn >> 8) & 0xFF); }

} // namespace agrobus::net
