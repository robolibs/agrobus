#pragma once

#include <datapod/datapod.hpp>

namespace agrobus::net {

    // ─── Numeric type aliases ────────────────────────────────────────────────────
    using dp::f32;
    using dp::f64;
    using dp::i16;
    using dp::i32;
    using dp::i64;
    using dp::i8;
    using dp::isize;
    using dp::u16;
    using dp::u32;
    using dp::u64;
    using dp::u8;
    using dp::usize;

    // ─── Byte and character type aliases ────────────────────────────────────────
    using dp::byte;
    using dp::char16;
    using dp::char32;
    using dp::char8;

    // ─── Domain-specific types ───────────────────────────────────────────────────
    using Address = u8;
    using PGN = u32;

    // ─── Priority (3-bit field in CAN identifier) ────────────────────────────────
    enum class Priority : u8 {
        Highest = 0,
        High = 1,
        AboveNormal = 2,
        Normal = 3,
        BelowNormal = 4,
        Low = 5,
        Default = 6,
        Lowest = 7
    };

} // namespace agrobus::net
