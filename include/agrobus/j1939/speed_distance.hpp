#pragma once

#include <agrobus/net/constants.hpp>
#include <agrobus/net/event.hpp>
#include <agrobus/net/message.hpp>
#include <agrobus/net/network_manager.hpp>
#include <agrobus/net/types.hpp>
#include <datapod/datapod.hpp>
#include <echo/echo.hpp>

namespace agrobus::j1939 {
    using namespace agrobus::net;

    // ─── Speed/distance data ─────────────────────────────────────────────────────
    struct SpeedData {
        dp::Optional<f64> wheel_speed_mps;    // m/s
        dp::Optional<f64> ground_speed_mps;   // m/s
        dp::Optional<f64> machine_speed_mps;  // m/s
        dp::Optional<f64> distance_m;         // meters
        dp::Optional<f64> machine_distance_m; // meters
        u64 timestamp_us = 0;
    };

    // ─── Speed/distance configuration ──────────────────────────────────────────
    struct SpeedDistanceConfig {
        bool listen_wheel = true;
        bool listen_ground = true;
        bool listen_machine = true;

        SpeedDistanceConfig &wheel_speed(bool enable) {
            listen_wheel = enable;
            return *this;
        }
        SpeedDistanceConfig &ground_speed(bool enable) {
            listen_ground = enable;
            return *this;
        }
        SpeedDistanceConfig &machine_speed(bool enable) {
            listen_machine = enable;
            return *this;
        }
    };

    // ─── Speed/distance interface ────────────────────────────────────────────────
    class SpeedDistanceInterface {
        IsoNet &net_;
        InternalCF *cf_;
        SpeedDistanceConfig config_;
        dp::Optional<SpeedData> latest_;

      public:
        SpeedDistanceInterface(IsoNet &net, InternalCF *cf, SpeedDistanceConfig config = {})
            : net_(net), cf_(cf), config_(config) {}

        Result<void> initialize() {
            if (!cf_) {
                return Result<void>::err(Error::invalid_state("control function not set"));
            }
            if (config_.listen_wheel)
                net_.register_pgn_callback(PGN_WHEEL_SPEED, [this](const Message &msg) { handle_wheel_speed(msg); });
            if (config_.listen_ground)
                net_.register_pgn_callback(PGN_GROUND_SPEED, [this](const Message &msg) { handle_ground_speed(msg); });
            if (config_.listen_machine)
                net_.register_pgn_callback(PGN_MACHINE_SPEED,
                                           [this](const Message &msg) { handle_machine_speed(msg); });
            echo::category("isobus.speed").debug("initialized");
            return {};
        }

        dp::Optional<SpeedData> latest() const noexcept { return latest_; }

        Result<void> send_wheel_speed(f64 speed_mps, f64 distance_m) {
            echo::category("isobus.speed").debug("sending wheel speed: ", speed_mps, " m/s");
            dp::Vector<u8> data(8, 0xFF);
            // Speed: 0.001 m/s per bit
            u16 speed_raw = static_cast<u16>(speed_mps / 0.001);
            data[0] = static_cast<u8>(speed_raw & 0xFF);
            data[1] = static_cast<u8>((speed_raw >> 8) & 0xFF);
            // Distance: 0.001 m per bit
            u32 dist_raw = static_cast<u32>(distance_m / 0.001);
            data[2] = static_cast<u8>(dist_raw & 0xFF);
            data[3] = static_cast<u8>((dist_raw >> 8) & 0xFF);
            data[4] = static_cast<u8>((dist_raw >> 16) & 0xFF);
            data[5] = static_cast<u8>((dist_raw >> 24) & 0xFF);
            return net_.send(PGN_WHEEL_SPEED, data, cf_);
        }

        Result<void> send_ground_speed(f64 speed_mps, f64 distance_m) {
            echo::category("isobus.speed").debug("sending ground speed: ", speed_mps, " m/s");
            dp::Vector<u8> data(8, 0xFF);
            u16 speed_raw = static_cast<u16>(speed_mps / 0.001);
            data[0] = static_cast<u8>(speed_raw & 0xFF);
            data[1] = static_cast<u8>((speed_raw >> 8) & 0xFF);
            u32 dist_raw = static_cast<u32>(distance_m / 0.001);
            data[2] = static_cast<u8>(dist_raw & 0xFF);
            data[3] = static_cast<u8>((dist_raw >> 8) & 0xFF);
            data[4] = static_cast<u8>((dist_raw >> 16) & 0xFF);
            data[5] = static_cast<u8>((dist_raw >> 24) & 0xFF);
            return net_.send(PGN_GROUND_SPEED, data, cf_);
        }

        Event<const SpeedData &> on_speed_update;

      private:
        void handle_wheel_speed(const Message &msg) {
            echo::category("isobus.speed").trace("speed update received");
            if (msg.data.size() < 6)
                return;
            SpeedData sd;
            sd.timestamp_us = msg.timestamp_us;
            u16 speed_raw = msg.get_u16_le(0);
            if (speed_raw != 0xFFFF)
                sd.wheel_speed_mps = speed_raw * 0.001;
            u32 dist_raw = msg.get_u32_le(2);
            if (dist_raw != 0xFFFFFFFF)
                sd.distance_m = dist_raw * 0.001;
            latest_ = sd;
            on_speed_update.emit(sd);
        }

        void handle_ground_speed(const Message &msg) {
            echo::category("isobus.speed").trace("speed update received");
            if (msg.data.size() < 6)
                return;
            SpeedData sd;
            sd.timestamp_us = msg.timestamp_us;
            u16 speed_raw = msg.get_u16_le(0);
            if (speed_raw != 0xFFFF)
                sd.ground_speed_mps = speed_raw * 0.001;
            u32 dist_raw = msg.get_u32_le(2);
            if (dist_raw != 0xFFFFFFFF)
                sd.machine_distance_m = dist_raw * 0.001;
            latest_ = sd;
            on_speed_update.emit(sd);
        }

        void handle_machine_speed(const Message &msg) {
            echo::category("isobus.speed").trace("speed update received");
            if (msg.data.size() < 8)
                return;
            SpeedData sd;
            sd.timestamp_us = msg.timestamp_us;
            u16 speed_raw = msg.get_u16_le(0);
            if (speed_raw != 0xFFFF)
                sd.machine_speed_mps = speed_raw * 0.001;
            u32 dist_raw = msg.get_u32_le(2);
            if (dist_raw != 0xFFFFFFFF)
                sd.machine_distance_m = dist_raw * 0.001;
            latest_ = sd;
            on_speed_update.emit(sd);
        }
    };
} // namespace agrobus::j1939
