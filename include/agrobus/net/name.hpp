#pragma once

#include "types.hpp"
#include <datapod/datapod.hpp>
#include <tuple>

namespace agrobus::net {

    // ─── ISO 11783-5 NAME (64-bit device identity) ──────────────────────────────
    // Bit layout (LSB first):
    //   [0..20]   Identity Number (21 bits)
    //   [21..31]  Manufacturer Code (11 bits)
    //   [32..34]  ECU Instance (3 bits)
    //   [35..39]  Function Instance (5 bits)
    //   [40..47]  Function Code (8 bits)
    //   [48]      Reserved
    //   [49..55]  Device Class (7 bits)
    //   [56..59]  Device Class Instance (4 bits)
    //   [60..62]  Industry Group (3 bits)
    //   [63]      Self-configurable Address (1 bit)
    struct Name {
        u64 raw = 0;

        constexpr Name() = default;
        constexpr explicit Name(u64 value) : raw(value) {}

        // ─── Bitfield accessors ──────────────────────────────────────────────────
        constexpr u32 identity_number() const noexcept { return static_cast<u32>(raw & 0x1FFFFF); }
        constexpr u16 manufacturer_code() const noexcept { return static_cast<u16>((raw >> 21) & 0x7FF); }
        constexpr u8 ecu_instance() const noexcept { return static_cast<u8>((raw >> 32) & 0x07); }
        constexpr u8 function_instance() const noexcept { return static_cast<u8>((raw >> 35) & 0x1F); }
        constexpr u8 function_code() const noexcept { return static_cast<u8>((raw >> 40) & 0xFF); }
        constexpr u8 device_class() const noexcept { return static_cast<u8>((raw >> 49) & 0x7F); }
        constexpr u8 device_class_instance() const noexcept { return static_cast<u8>((raw >> 56) & 0x0F); }
        constexpr u8 industry_group() const noexcept { return static_cast<u8>((raw >> 60) & 0x07); }
        constexpr bool self_configurable() const noexcept { return (raw >> 63) & 0x01; }

        // ─── Bitfield setters ────────────────────────────────────────────────────
        constexpr Name &set_identity_number(u32 val) noexcept {
            raw = (raw & ~0x1FFFFFULL) | (static_cast<u64>(val) & 0x1FFFFF);
            return *this;
        }
        constexpr Name &set_manufacturer_code(u16 val) noexcept {
            raw = (raw & ~(0x7FFULL << 21)) | (static_cast<u64>(val & 0x7FF) << 21);
            return *this;
        }
        constexpr Name &set_ecu_instance(u8 val) noexcept {
            raw = (raw & ~(0x07ULL << 32)) | (static_cast<u64>(val & 0x07) << 32);
            return *this;
        }
        constexpr Name &set_function_instance(u8 val) noexcept {
            raw = (raw & ~(0x1FULL << 35)) | (static_cast<u64>(val & 0x1F) << 35);
            return *this;
        }
        constexpr Name &set_function_code(u8 val) noexcept {
            raw = (raw & ~(0xFFULL << 40)) | (static_cast<u64>(val & 0xFF) << 40);
            return *this;
        }
        constexpr Name &set_device_class(u8 val) noexcept {
            raw = (raw & ~(0x7FULL << 49)) | (static_cast<u64>(val & 0x7F) << 49);
            return *this;
        }
        constexpr Name &set_device_class_instance(u8 val) noexcept {
            raw = (raw & ~(0x0FULL << 56)) | (static_cast<u64>(val & 0x0F) << 56);
            return *this;
        }
        constexpr Name &set_industry_group(u8 val) noexcept {
            raw = (raw & ~(0x07ULL << 60)) | (static_cast<u64>(val & 0x07) << 60);
            return *this;
        }
        constexpr Name &set_self_configurable(bool val) noexcept {
            raw = (raw & ~(0x01ULL << 63)) | (static_cast<u64>(val ? 1 : 0) << 63);
            return *this;
        }

        // ─── Builder pattern ─────────────────────────────────────────────────────
        static constexpr Name build() noexcept { return Name{}; }

        // ─── Comparison (lower NAME wins address arbitration) ────────────────────
        constexpr bool operator<(const Name &other) const noexcept { return raw < other.raw; }
        constexpr bool operator>(const Name &other) const noexcept { return raw > other.raw; }
        constexpr bool operator==(const Name &other) const noexcept { return raw == other.raw; }
        constexpr bool operator!=(const Name &other) const noexcept { return raw != other.raw; }

        // ─── Serialization ───────────────────────────────────────────────────────
        constexpr dp::Array<u8, 8> to_bytes() const noexcept {
            dp::Array<u8, 8> bytes{};
            for (usize i = 0; i < 8; ++i) {
                bytes[i] = static_cast<u8>((raw >> (i * 8)) & 0xFF);
            }
            return bytes;
        }

        static constexpr Name from_bytes(const u8 *data) noexcept {
            u64 val = 0;
            for (usize i = 0; i < 8; ++i) {
                val |= static_cast<u64>(data[i]) << (i * 8);
            }
            return Name(val);
        }

        auto members() const noexcept { return std::tie(raw); }
    };

} // namespace agrobus::net
