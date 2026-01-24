#pragma once

#include "objects.hpp"
#include "server_options.hpp"
#include <agrobus/net/constants.hpp>
#include <agrobus/net/control_function.hpp>
#include <agrobus/net/error.hpp>
#include <agrobus/net/event.hpp>
#include <agrobus/net/internal_cf.hpp>
#include <agrobus/net/message.hpp>
#include <agrobus/net/network_manager.hpp>
#include <agrobus/net/types.hpp>
#include <concord/concord.hpp>
#include <datapod/datapod.hpp>
#include <echo/echo.hpp>

namespace agrobus::isobus::tc {
    using namespace agrobus::net;

    // ─── ISO 11783-10 TC-GEO DDIs ─────────────────────────────────────────────────
    namespace geo_ddi {
        inline constexpr DDI ACTUAL_LATITUDE = 0x0087;  // DDI 135
        inline constexpr DDI ACTUAL_LONGITUDE = 0x0088; // DDI 136
        inline constexpr DDI ACTUAL_ALTITUDE = 0x0089;  // DDI 137
        inline constexpr DDI SETPOINT_LATITUDE = 0x008A;
        inline constexpr DDI SETPOINT_LONGITUDE = 0x008B;
    } // namespace geo_ddi

    // ─── TC-GEO: Position-based Task Control ────────────────────────────────────

    struct GeoPoint {
        concord::earth::WGS position;
        u64 timestamp_us = 0;
    };

    struct PrescriptionZone {
        dp::Vector<concord::earth::WGS> boundary; // Polygon vertices
        i32 application_rate = 0;                 // Rate value (DDI-dependent units)
    };

    struct PrescriptionMap {
        dp::String structure_label;
        dp::Vector<PrescriptionZone> zones;
    };

    // ─── TC-GEO Interface ────────────────────────────────────────────────────────
    // Position data arrives via GNSS PGNs (129025/129027). The TC-GEO interface
    // uses standard TC process data DDIs to communicate position to the TC server,
    // and evaluates prescription maps to determine application rates.
    class TCGEOInterface {
        IsoNet &net_;
        InternalCF *cf_;
        dp::Vector<PrescriptionMap> maps_;
        dp::Optional<GeoPoint> current_position_;
        dp::Optional<i32> last_rate_;

      public:
        TCGEOInterface(IsoNet &net, InternalCF *cf) : net_(net), cf_(cf) {}

        Result<void> initialize() {
            if (!cf_) {
                return Result<void>::err(Error::invalid_state("control function not set"));
            }
            // Listen for GNSS position updates (PGN 129025 - Position Rapid Update)
            net_.register_pgn_callback(PGN_GNSS_POSITION, [this](const Message &msg) { handle_gnss_position(msg); });
            echo::category("isobus.tc.geo").info("TC-GEO interface initialized");
            return {};
        }

        // ─── Position from GNSS ────────────────────────────────────────────────────
        Result<void> set_position(const GeoPoint &position) {
            current_position_ = position;
            echo::category("isobus.tc.geo")
                .trace("position set: lat=", position.position.latitude, " lon=", position.position.longitude);
            on_position_update.emit(position);
            return {};
        }

        // ─── Send position as TC process data (DDI 135/136) ────────────────────────
        Result<void> send_position_process_data(ControlFunction *dest) {
            echo::category("isobus.tc.geo").debug("sending position process data");
            if (!current_position_.has_value())
                return Result<void>::err(Error::invalid_state("no position available"));

            // Encode latitude as 1e-7 degrees (i32)
            i32 lat_fixed = static_cast<i32>(current_position_->position.latitude * 1e7);
            i32 lon_fixed = static_cast<i32>(current_position_->position.longitude * 1e7);

            // Send latitude as process data value
            dp::Vector<u8> lat_data(8, 0xFF);
            lat_data[0] = (static_cast<u8>(ProcessDataCommands::Value) & 0x0F);
            lat_data[2] = static_cast<u8>(geo_ddi::ACTUAL_LATITUDE & 0xFF);
            lat_data[3] = static_cast<u8>((geo_ddi::ACTUAL_LATITUDE >> 8) & 0xFF);
            lat_data[4] = static_cast<u8>(lat_fixed & 0xFF);
            lat_data[5] = static_cast<u8>((lat_fixed >> 8) & 0xFF);
            lat_data[6] = static_cast<u8>((lat_fixed >> 16) & 0xFF);
            lat_data[7] = static_cast<u8>((lat_fixed >> 24) & 0xFF);
            auto r1 = net_.send(PGN_ECU_TO_TC, lat_data, cf_, dest, Priority::Default);
            if (!r1.is_ok())
                return r1;

            // Send longitude as process data value
            dp::Vector<u8> lon_data(8, 0xFF);
            lon_data[0] = (static_cast<u8>(ProcessDataCommands::Value) & 0x0F);
            lon_data[2] = static_cast<u8>(geo_ddi::ACTUAL_LONGITUDE & 0xFF);
            lon_data[3] = static_cast<u8>((geo_ddi::ACTUAL_LONGITUDE >> 8) & 0xFF);
            lon_data[4] = static_cast<u8>(lon_fixed & 0xFF);
            lon_data[5] = static_cast<u8>((lon_fixed >> 8) & 0xFF);
            lon_data[6] = static_cast<u8>((lon_fixed >> 16) & 0xFF);
            lon_data[7] = static_cast<u8>((lon_fixed >> 24) & 0xFF);
            return net_.send(PGN_ECU_TO_TC, lon_data, cf_, dest, Priority::Default);
        }

        // ─── Prescription maps ─────────────────────────────────────────────────────
        Result<void> add_prescription_map(PrescriptionMap map) {
            echo::category("isobus.tc.geo")
                .info("Prescription map added: ", map.structure_label, " zones=", map.zones.size());
            maps_.push_back(std::move(map));
            return {};
        }

        Result<void> clear_prescription_maps() {
            maps_.clear();
            echo::category("isobus.tc.geo").trace("prescription maps cleared");
            return {};
        }

        const dp::Vector<PrescriptionMap> &prescription_maps() const noexcept { return maps_; }

        // Check if a position falls within any prescription zone and return rate
        dp::Optional<i32> get_rate_at_position(const concord::earth::WGS &pos) const {
            for (const auto &map : maps_) {
                for (const auto &zone : map.zones) {
                    if (point_in_polygon(pos, zone.boundary)) {
                        return zone.application_rate;
                    }
                }
            }
            return dp::nullopt;
        }

        dp::Optional<GeoPoint> current_position() const noexcept { return current_position_; }

        // ─── Events ──────────────────────────────────────────────────────────────
        Event<const GeoPoint &> on_position_update;
        Event<i32> on_application_rate_changed; // New rate based on position
        Event<const PrescriptionMap &> on_prescription_map_received;

        void update(u32 /*elapsed_ms*/) {
            // Check current position against prescription maps
            if (current_position_.has_value()) {
                auto rate = get_rate_at_position(current_position_->position);
                if (rate.has_value() && (!last_rate_.has_value() || *last_rate_ != *rate)) {
                    last_rate_ = *rate;
                    on_application_rate_changed.emit(*rate);
                }
            }
        }

      private:
        void handle_gnss_position(const Message &msg) {
            if (msg.data.size() < 8)
                return;

            // PGN 129025: Latitude (i32, 1e-7 deg) at bytes 0-3, Longitude at bytes 4-7
            i32 lat_raw =
                static_cast<i32>(static_cast<u32>(msg.data[0]) | (static_cast<u32>(msg.data[1]) << 8) |
                                 (static_cast<u32>(msg.data[2]) << 16) | (static_cast<u32>(msg.data[3]) << 24));
            i32 lon_raw =
                static_cast<i32>(static_cast<u32>(msg.data[4]) | (static_cast<u32>(msg.data[5]) << 8) |
                                 (static_cast<u32>(msg.data[6]) << 16) | (static_cast<u32>(msg.data[7]) << 24));

            if (lat_raw == static_cast<i32>(0x7FFFFFFF) || lon_raw == static_cast<i32>(0x7FFFFFFF))
                return;

            GeoPoint point;
            point.position = concord::earth::WGS(static_cast<f64>(lat_raw) * 1e-7, static_cast<f64>(lon_raw) * 1e-7);
            point.timestamp_us = msg.timestamp_us;
            set_position(point);
        }

        // Ray-casting point-in-polygon test
        static bool point_in_polygon(const concord::earth::WGS &point, const dp::Vector<concord::earth::WGS> &polygon) {
            if (polygon.size() < 3)
                return false;

            bool inside = false;
            usize n = polygon.size();
            for (usize i = 0, j = n - 1; i < n; j = i++) {
                f64 xi = polygon[i].longitude, yi = polygon[i].latitude;
                f64 xj = polygon[j].longitude, yj = polygon[j].latitude;

                bool intersect = ((yi > point.latitude) != (yj > point.latitude)) &&
                                 (point.longitude < (xj - xi) * (point.latitude - yi) / (yj - yi) + xi);
                if (intersect)
                    inside = !inside;
            }
            return inside;
        }
    };

} // namespace agrobus::isobus::tc
