#pragma once

#include "constants.hpp"
#include "types.hpp"

namespace agrobus::net {

    // ─── CAN Identifier (29-bit extended ID) ────────────────────────────────────
    // Layout: [Priority:3][Reserved:1][DataPage:1][PDU Format:8][PDU Specific:8][Source:8]
    struct Identifier {
        u32 raw = 0;

        constexpr Identifier() = default;
        constexpr explicit Identifier(u32 id) : raw(id & 0x1FFFFFFF) {}

        constexpr Priority priority() const noexcept { return static_cast<Priority>((raw >> 26) & 0x07); }

        constexpr u8 pdu_format() const noexcept { return static_cast<u8>((raw >> 16) & 0xFF); }

        constexpr u8 pdu_specific() const noexcept { return static_cast<u8>((raw >> 8) & 0xFF); }

        constexpr Address source() const noexcept { return static_cast<Address>(raw & 0xFF); }

        constexpr bool is_pdu2() const noexcept { return pdu_format() >= 240; }

        constexpr PGN pgn() const noexcept {
            u8 dp = (raw >> 24) & 0x01;
            u8 edp = (raw >> 25) & 0x01;
            u8 pf = pdu_format();
            if (pf < 240) {
                // PDU1: destination-specific, PGN does not include PS
                return (static_cast<u32>(edp) << 17) | (static_cast<u32>(dp) << 16) | (static_cast<u32>(pf) << 8);
            }
            // PDU2: broadcast, PGN includes PS as group extension
            return (static_cast<u32>(edp) << 17) | (static_cast<u32>(dp) << 16) | (static_cast<u32>(pf) << 8) |
                   pdu_specific();
        }

        constexpr Address destination() const noexcept {
            if (is_pdu2()) {
                return BROADCAST_ADDRESS;
            }
            return pdu_specific();
        }

        constexpr bool is_broadcast() const noexcept { return is_pdu2() || destination() == BROADCAST_ADDRESS; }

        constexpr bool data_page() const noexcept { return (raw >> 24) & 0x01; }

        constexpr bool extended_data_page() const noexcept { return (raw >> 25) & 0x01; }

        static constexpr Identifier encode(Priority prio, PGN pgn, Address src,
                                           Address dst = BROADCAST_ADDRESS) noexcept {
            u32 id = 0;
            id |= (static_cast<u32>(prio) & 0x07) << 26;

            u8 edp = (pgn >> 17) & 0x01;
            u8 dp = (pgn >> 16) & 0x01;
            u8 pf = (pgn >> 8) & 0xFF;
            u8 ps = pgn & 0xFF;

            id |= static_cast<u32>(edp) << 25;
            id |= static_cast<u32>(dp) << 24;
            id |= static_cast<u32>(pf) << 16;

            if (pf < 240) {
                // PDU1: PS is destination
                id |= static_cast<u32>(dst) << 8;
            } else {
                // PDU2: PS is group extension (part of PGN)
                id |= static_cast<u32>(ps) << 8;
            }
            id |= static_cast<u32>(src);

            return Identifier(id);
        }

        constexpr bool operator==(const Identifier &other) const noexcept { return raw == other.raw; }
        constexpr bool operator!=(const Identifier &other) const noexcept { return raw != other.raw; }
    };

} // namespace agrobus::net
