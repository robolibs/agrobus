#pragma once

#include <agrobus/net/bitfield.hpp>
#include <agrobus/net/types.hpp>
#include <datapod/datapod.hpp>

namespace agrobus::net {

    // ─── Span-like view over message data ────────────────────────────────────────
    class DataSpan {
        const u8 *data_ = nullptr;
        usize size_ = 0;

      public:
        constexpr DataSpan() = default;
        constexpr DataSpan(const u8 *data, usize size) : data_(data), size_(size) {}
        DataSpan(const dp::Vector<u8> &vec) : data_(vec.data()), size_(vec.size()) {}

        template <usize N> constexpr DataSpan(const dp::Array<u8, N> &arr) : data_(arr.data()), size_(N) {}

        constexpr const u8 *data() const noexcept { return data_; }
        constexpr usize size() const noexcept { return size_; }
        constexpr bool empty() const noexcept { return size_ == 0; }

        constexpr u8 operator[](usize idx) const noexcept {
            if (idx >= size_)
                return 0xFF;
            return data_[idx];
        }

        constexpr DataSpan subspan(usize offset, usize count = static_cast<usize>(-1)) const noexcept {
            if (offset >= size_)
                return {};
            usize actual = (count > size_ - offset) ? (size_ - offset) : count;
            return DataSpan(data_ + offset, actual);
        }

        // ─── Typed extraction ────────────────────────────────────────────────────
        u8 get_u8(usize offset) const noexcept { return (*this)[offset]; }

        u16 get_u16_le(usize offset) const noexcept {
            if (offset + 1 >= size_)
                return 0xFFFF;
            return bitfield::unpack_u16_le(data_ + offset);
        }

        u32 get_u32_le(usize offset) const noexcept {
            if (offset + 3 >= size_)
                return 0xFFFFFFFF;
            return bitfield::unpack_u32_le(data_ + offset);
        }

        u64 get_u64_le(usize offset) const noexcept {
            if (offset + 7 >= size_)
                return 0xFFFFFFFFFFFFFFFF;
            return bitfield::unpack_u64_le(data_ + offset);
        }

        bool get_bit(usize byte_offset, u8 bit) const noexcept {
            if (byte_offset >= size_ || bit > 7)
                return false;
            return bitfield::get_bit(data_[byte_offset], bit);
        }

        // Iterator support
        constexpr const u8 *begin() const noexcept { return data_; }
        constexpr const u8 *end() const noexcept { return data_ + size_; }
    };
} // namespace agrobus::net
