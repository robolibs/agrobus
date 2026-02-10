#pragma once

#include <agrobus/net/constants.hpp>
#include <agrobus/net/event.hpp>
#include <agrobus/net/message.hpp>
#include <agrobus/net/network_manager.hpp>
#include <agrobus/net/types.hpp>
#include <datapod/datapod.hpp>
#include <echo/echo.hpp>

namespace agrobus::isobus {
    using namespace agrobus::net;

    // ─── Guidance data ───────────────────────────────────────────────────────────
    struct GuidanceData {
        dp::Optional<f64> curvature;     // 1/m (positive = right turn)
        dp::Optional<f64> heading_rad;   // radians (0 = north, CW positive)
        dp::Optional<f64> cross_track_m; // meters (positive = right of line)
        dp::Optional<u8> status;         // Guidance status
        u64 timestamp_us = 0;
    };

    // ─── Guidance configuration ─────────────────────────────────────────────────
    struct GuidanceConfig {
        bool listen_machine = true;
        bool listen_system = true;

        GuidanceConfig &machine_info(bool enable) {
            listen_machine = enable;
            return *this;
        }
        GuidanceConfig &system_command(bool enable) {
            listen_system = enable;
            return *this;
        }
    };

    // ─── Guidance interface ──────────────────────────────────────────────────────
    class GuidanceInterface {
        IsoNet &net_;
        InternalCF *cf_;
        GuidanceConfig config_;
        dp::Optional<GuidanceData> latest_machine_;
        dp::Optional<GuidanceData> latest_system_;

      public:
        GuidanceInterface(IsoNet &net, InternalCF *cf, GuidanceConfig config = {})
            : net_(net), cf_(cf), config_(config) {}

        Result<void> initialize() {
            if (!cf_) {
                return Result<void>::err(Error::invalid_state("control function not set"));
            }
            if (config_.listen_machine)
                net_.register_pgn_callback(PGN_GUIDANCE_MACHINE,
                                           [this](const Message &msg) { handle_guidance_machine(msg); });
            if (config_.listen_system)
                net_.register_pgn_callback(PGN_GUIDANCE_SYSTEM,
                                           [this](const Message &msg) { handle_guidance_system(msg); });
            echo::category("isobus.guidance").debug("initialized");
            return {};
        }

        dp::Optional<GuidanceData> latest_machine() const noexcept { return latest_machine_; }
        dp::Optional<GuidanceData> latest_system() const noexcept { return latest_system_; }

        // Send guidance system command (from guidance controller)
        Result<void> send_system_command(const GuidanceData &gd) {
            echo::category("isobus.guidance").debug("sending system command");
            dp::Vector<u8> data(8, 0xFF);
            if (gd.curvature) {
                // Curvature: 0.25 km^-1 per bit, offset -8032
                i16 raw = static_cast<i16>((*gd.curvature * 1000.0) / 0.25 + 8032);
                data[0] = static_cast<u8>(raw & 0xFF);
                data[1] = static_cast<u8>((raw >> 8) & 0xFF);
            }
            if (gd.status) {
                data[2] = *gd.status;
            }
            return net_.send(PGN_GUIDANCE_SYSTEM, data, cf_, nullptr, Priority::AboveNormal);
        }

        // Send guidance machine info (from implement)
        Result<void> send_machine_info(const GuidanceData &gd) {
            echo::category("isobus.guidance").debug("sending machine info");
            dp::Vector<u8> data(8, 0xFF);
            if (gd.curvature) {
                i16 raw = static_cast<i16>((*gd.curvature * 1000.0) / 0.25 + 8032);
                data[0] = static_cast<u8>(raw & 0xFF);
                data[1] = static_cast<u8>((raw >> 8) & 0xFF);
            }
            if (gd.status) {
                data[2] = *gd.status;
            }
            return net_.send(PGN_GUIDANCE_MACHINE, data, cf_, nullptr, Priority::AboveNormal);
        }

        Event<const GuidanceData &> on_guidance_machine;
        Event<const GuidanceData &> on_guidance_system;

      private:
        void handle_guidance_machine(const Message &msg) {
            echo::category("isobus.guidance").trace("guidance data received");
            auto gd = parse_guidance(msg);
            latest_machine_ = gd;
            on_guidance_machine.emit(gd);
        }

        void handle_guidance_system(const Message &msg) {
            echo::category("isobus.guidance").trace("guidance data received");
            auto gd = parse_guidance(msg);
            latest_system_ = gd;
            on_guidance_system.emit(gd);
        }

        GuidanceData parse_guidance(const Message &msg) const {
            GuidanceData gd;
            gd.timestamp_us = msg.timestamp_us;
            if (msg.data.size() < 3)
                return gd;

            u16 curv_raw = msg.get_u16_le(0);
            if (curv_raw != 0xFFFF) {
                // Curvature: 0.25 km^-1 per bit, offset -8032
                gd.curvature = (static_cast<f64>(static_cast<i16>(curv_raw)) - 8032.0) * 0.25 / 1000.0;
            }
            if (msg.data[2] != 0xFF) {
                gd.status = msg.data[2];
            }
            return gd;
        }
    };
} // namespace agrobus::isobus
