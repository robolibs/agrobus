#pragma once

#include "identifier.hpp"
#include "types.hpp"
#include <datapod/datapod.hpp>

namespace agrobus::net {

    // ─── CAN Frame (8-byte physical frame) ──────────────────────────────────────
    struct Frame {
        Identifier id;
        dp::Array<u8, 8> data = {};
        u8 length = 8;
        u64 timestamp_us = 0;

        constexpr Frame() = default;

        constexpr Frame(Identifier identifier, dp::Array<u8, 8> payload, u8 len = 8)
            : id(identifier), data(payload), length(len) {}

        static Frame from_message(Priority prio, PGN pgn, Address src, Address dst, const u8 *payload,
                                  u8 len = 8) noexcept {
            Frame f;
            f.id = Identifier::encode(prio, pgn, src, dst);
            f.length = len > 8 ? 8 : len;
            for (u8 i = 0; i < f.length; ++i) {
                f.data[i] = payload[i];
            }
            for (u8 i = f.length; i < 8; ++i) {
                f.data[i] = 0xFF;
            }
            return f;
        }

        constexpr PGN pgn() const noexcept { return id.pgn(); }
        constexpr Address source() const noexcept { return id.source(); }
        constexpr Address destination() const noexcept { return id.destination(); }
        constexpr Priority priority() const noexcept { return id.priority(); }
        constexpr bool is_broadcast() const noexcept { return id.is_broadcast(); }
    };

} // namespace agrobus::net
