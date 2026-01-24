#pragma once

#include "definitions.hpp"
#include <agrobus/net/types.hpp>
#include <concord/concord.hpp>
#include <datapod/datapod.hpp>

namespace agrobus::nmea {
    using namespace agrobus::net;

    // ─── GNSS Position data ─────────────────────────────────────────────────────
    struct GNSSPosition {
        concord::earth::WGS wgs;
        dp::Optional<f64> altitude_m;
        dp::Optional<f64> heading_rad;
        dp::Optional<f64> speed_mps;
        dp::Optional<f64> cog_rad; // Course over ground
        dp::Optional<f64> hdop;
        dp::Optional<f64> pdop;
        dp::Optional<f64> vdop;
        u8 satellites_used = 0;
        GNSSFixType fix_type = GNSSFixType::NoFix;
        GNSSSystem gnss_system = GNSSSystem::GPS;
        dp::Optional<f64> geoidal_separation_m;
        dp::Optional<f64> rate_of_turn_rps;
        dp::Optional<f64> pitch_rad;
        dp::Optional<f64> roll_rad;
        u64 timestamp_us = 0;

        bool has_fix() const noexcept { return fix_type != GNSSFixType::NoFix; }

        bool is_rtk() const noexcept { return fix_type == GNSSFixType::RTKFixed || fix_type == GNSSFixType::RTKFloat; }

        // Convert to local ENU frame
        concord::frame::ENU to_enu(const dp::Geo &reference) const { return concord::frame::to_enu(reference, wgs); }

        // Convert to local NED frame
        concord::frame::NED to_ned(const dp::Geo &reference) const { return concord::frame::to_ned(reference, wgs); }

        // Convert to ECEF
        concord::earth::ECF to_ecf() const { return concord::earth::to_ecf(wgs); }
    };

    // ─── Batch GNSS processing ──────────────────────────────────────────────────
    struct GNSSBatch {
        dp::Vector<GNSSPosition> positions;

        dp::Vector<concord::frame::ENU> to_enu_batch(const dp::Geo &ref) const {
            dp::Vector<concord::frame::ENU> results;
            results.reserve(positions.size());
            for (const auto &p : positions) {
                results.push_back(concord::frame::to_enu(ref, p.wgs));
            }
            return results;
        }

        dp::Vector<concord::frame::NED> to_ned_batch(const dp::Geo &ref) const {
            dp::Vector<concord::frame::NED> results;
            results.reserve(positions.size());
            for (const auto &p : positions) {
                results.push_back(concord::frame::to_ned(ref, p.wgs));
            }
            return results;
        }

        dp::Vector<concord::earth::ECF> to_ecf_batch() const {
            dp::Vector<concord::earth::ECF> results;
            results.reserve(positions.size());
            for (const auto &p : positions) {
                results.push_back(concord::earth::to_ecf(p.wgs));
            }
            return results;
        }
    };

} // namespace agrobus::nmea
