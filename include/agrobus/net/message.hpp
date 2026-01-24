#pragma once

#include "constants.hpp"
#include "types.hpp"
#include <datapod/datapod.hpp>

namespace agrobus::net {

    // ─── CAN Message (arbitrary length, decoded from TP/ETP or single frame) ────
    struct Message {
        PGN pgn = 0;
        dp::Vector<u8> data;
        Address source = NULL_ADDRESS;
        Address destination = BROADCAST_ADDRESS;
        Priority priority = Priority::Default;
        u64 timestamp_us = 0;

        Message() = default;

        Message(PGN p, dp::Vector<u8> d, Address src, Address dst = BROADCAST_ADDRESS,
                Priority prio = Priority::Default)
            : pgn(p), data(std::move(d)), source(src), destination(dst), priority(prio) {}

        // ─── Data extraction helpers ─────────────────────────────────────────────
        u8 get_u8(usize offset) const noexcept {
            if (offset >= data.size())
                return 0xFF;
            return data[offset];
        }

        u16 get_u16_le(usize offset) const noexcept {
            if (offset + 1 >= data.size())
                return 0xFFFF;
            return static_cast<u16>(data[offset]) | (static_cast<u16>(data[offset + 1]) << 8);
        }

        u32 get_u32_le(usize offset) const noexcept {
            if (offset + 3 >= data.size())
                return 0xFFFFFFFF;
            return static_cast<u32>(data[offset]) | (static_cast<u32>(data[offset + 1]) << 8) |
                   (static_cast<u32>(data[offset + 2]) << 16) | (static_cast<u32>(data[offset + 3]) << 24);
        }

        u64 get_u64_le(usize offset) const noexcept {
            if (offset + 7 >= data.size())
                return 0xFFFFFFFFFFFFFFFF;
            u64 result = 0;
            for (usize i = 0; i < 8; ++i) {
                result |= static_cast<u64>(data[offset + i]) << (i * 8);
            }
            return result;
        }

        bool get_bit(usize byte_offset, u8 bit) const noexcept {
            if (byte_offset >= data.size() || bit > 7)
                return false;
            return (data[byte_offset] >> bit) & 0x01;
        }

        // Extract arbitrary bit field (up to 32 bits)
        u32 get_bits(usize start_bit, u8 length) const noexcept {
            if (length == 0 || length > 32)
                return 0;
            u32 result = 0;
            for (u8 i = 0; i < length; ++i) {
                usize bit_pos = start_bit + i;
                usize byte_idx = bit_pos / 8;
                u8 bit_idx = bit_pos % 8;
                if (byte_idx < data.size()) {
                    result |= static_cast<u32>((data[byte_idx] >> bit_idx) & 0x01) << i;
                }
            }
            return result;
        }

        bool is_broadcast() const noexcept { return destination == BROADCAST_ADDRESS; }

        usize size() const noexcept { return data.size(); }

        // ─── Data setter helpers ─────────────────────────────────────────────────
        void set_u8(usize offset, u8 val) {
            if (offset >= data.size())
                data.resize(offset + 1, 0xFF);
            data[offset] = val;
        }

        void set_u16_le(usize offset, u16 val) {
            if (offset + 1 >= data.size())
                data.resize(offset + 2, 0xFF);
            data[offset] = static_cast<u8>(val & 0xFF);
            data[offset + 1] = static_cast<u8>((val >> 8) & 0xFF);
        }

        void set_u32_le(usize offset, u32 val) {
            if (offset + 3 >= data.size())
                data.resize(offset + 4, 0xFF);
            data[offset] = static_cast<u8>(val & 0xFF);
            data[offset + 1] = static_cast<u8>((val >> 8) & 0xFF);
            data[offset + 2] = static_cast<u8>((val >> 16) & 0xFF);
            data[offset + 3] = static_cast<u8>((val >> 24) & 0xFF);
        }
    };

} // namespace agrobus::net
