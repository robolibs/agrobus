#pragma once

#include <agrobus/net/types.hpp>
#include <type_traits>

namespace agrobus::net {

    // ─── Bit-level access helpers ────────────────────────────────────────────────
    namespace bitfield {

        // Constrain to unsigned integer types to avoid UB with signed shifts
        template <typename T>
        concept UnsignedInt = std::is_unsigned_v<T>;

        template <UnsignedInt T> constexpr T get_bits(T value, u8 start_bit, u8 length) noexcept {
            constexpr u8 bit_width = sizeof(T) * 8;
            if (length == 0 || start_bit >= bit_width) {
                return 0;
            }
            if (length >= bit_width) {
                return value >> start_bit;
            }
            T mask = (static_cast<T>(1) << length) - 1;
            return (value >> start_bit) & mask;
        }

        template <UnsignedInt T> constexpr T set_bits(T value, u8 start_bit, u8 length, T field_value) noexcept {
            constexpr u8 bit_width = sizeof(T) * 8;
            if (length == 0 || start_bit >= bit_width) {
                return value;
            }
            if (length >= bit_width) {
                return field_value;
            }
            T mask = (static_cast<T>(1) << length) - 1;
            value &= ~(mask << start_bit);
            value |= (field_value & mask) << start_bit;
            return value;
        }

        template <UnsignedInt T> constexpr bool get_bit(T value, u8 bit) noexcept {
            constexpr u8 bit_width = sizeof(T) * 8;
            if (bit >= bit_width) {
                return false;
            }
            return (value >> bit) & 0x01;
        }

        template <UnsignedInt T> constexpr T set_bit(T value, u8 bit, bool on) noexcept {
            constexpr u8 bit_width = sizeof(T) * 8;
            if (bit >= bit_width) {
                return value;
            }
            if (on)
                return value | (static_cast<T>(1) << bit);
            return value & ~(static_cast<T>(1) << bit);
        }

        // Pack/unpack little-endian values from byte arrays
        inline u16 unpack_u16_le(const u8 *data) noexcept {
            return static_cast<u16>(data[0]) | (static_cast<u16>(data[1]) << 8);
        }

        inline u32 unpack_u32_le(const u8 *data) noexcept {
            return static_cast<u32>(data[0]) | (static_cast<u32>(data[1]) << 8) | (static_cast<u32>(data[2]) << 16) |
                   (static_cast<u32>(data[3]) << 24);
        }

        inline u64 unpack_u64_le(const u8 *data) noexcept {
            u64 result = 0;
            for (usize i = 0; i < 8; ++i) {
                result |= static_cast<u64>(data[i]) << (i * 8);
            }
            return result;
        }

        inline void pack_u16_le(u8 *data, u16 value) noexcept {
            data[0] = static_cast<u8>(value & 0xFF);
            data[1] = static_cast<u8>((value >> 8) & 0xFF);
        }

        inline void pack_u32_le(u8 *data, u32 value) noexcept {
            data[0] = static_cast<u8>(value & 0xFF);
            data[1] = static_cast<u8>((value >> 8) & 0xFF);
            data[2] = static_cast<u8>((value >> 16) & 0xFF);
            data[3] = static_cast<u8>((value >> 24) & 0xFF);
        }

        inline void pack_u64_le(u8 *data, u64 value) noexcept {
            for (usize i = 0; i < 8; ++i) {
                data[i] = static_cast<u8>((value >> (i * 8)) & 0xFF);
            }
        }

    } // namespace bitfield
} // namespace agrobus::net
