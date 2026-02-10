#pragma once

#include "error.hpp"
#include "types.hpp"
#include <datapod/datapod.hpp>
#include <echo/echo.hpp>

namespace agrobus::net {

    // ─── ISO 11783-2 mandated CAN bus physical layer values ─────────────────────
    inline constexpr u32 ISO_CAN_BITRATE = 250000;        // 250 kbit/s
    inline constexpr f64 ISO_SAMPLE_POINT_MIN = 0.77;     // 80% - 3%
    inline constexpr f64 ISO_SAMPLE_POINT_MAX = 0.83;     // 80% + 3%
    inline constexpr f64 ISO_SAMPLE_POINT_NOMINAL = 0.80; // 80%

    // ─── CAN bus configuration parameters ───────────────────────────────────────
    struct CanBusConfig {
        u32 bitrate = ISO_CAN_BITRATE;
        f64 sample_point = ISO_SAMPLE_POINT_NOMINAL;
        u8 sjw = 1;        // Synchronization Jump Width
        u8 prop_seg = 0;   // Propagation segment
        u8 phase_seg1 = 0; // Phase segment 1
        u8 phase_seg2 = 0; // Phase segment 2
        bool silent_mode = false;
        bool loopback = false;

        // Fluent API
        CanBusConfig &set_bitrate(u32 br) {
            bitrate = br;
            return *this;
        }
        CanBusConfig &set_sample_point(f64 sp) {
            sample_point = sp;
            return *this;
        }
        CanBusConfig &set_sjw(u8 s) {
            sjw = s;
            return *this;
        }
        CanBusConfig &set_silent(bool s) {
            silent_mode = s;
            return *this;
        }
        CanBusConfig &set_loopback(bool l) {
            loopback = l;
            return *this;
        }
    };

    // ─── Validation result ──────────────────────────────────────────────────────
    struct CanBusValidation {
        bool bitrate_ok = false;
        bool sample_point_ok = false;
        bool overall_ok = false;
        dp::String error_message;
    };

    // ─── Validates a CAN bus config against ISO 11783-2 requirements ────────────
    inline CanBusValidation validate_can_bus_config(const CanBusConfig &config) {
        CanBusValidation result;
        result.bitrate_ok = (config.bitrate == ISO_CAN_BITRATE);
        result.sample_point_ok =
            (config.sample_point >= ISO_SAMPLE_POINT_MIN && config.sample_point <= ISO_SAMPLE_POINT_MAX);
        result.overall_ok = result.bitrate_ok && result.sample_point_ok;

        if (!result.bitrate_ok) {
            result.error_message = "bitrate must be 250000";
        } else if (!result.sample_point_ok) {
            result.error_message = "sample point must be 80% +/- 3%";
        }

        if (!result.overall_ok) {
            echo::category("isobus.can_bus").warn("CAN bus config non-compliant: ", result.error_message);
        }
        return result;
    }

    // ─── Helper to enforce compliance before starting stack ─────────────────────
    inline Result<void> enforce_iso_can_config(const CanBusConfig &config) {
        auto validation = validate_can_bus_config(config);
        if (!validation.overall_ok) {
            return Result<void>::err(Error::invalid_state(validation.error_message));
        }
        return {};
    }

} // namespace agrobus::net
