#pragma once

#include "definitions.hpp"
#include "position.hpp"
#include <agrobus/net/constants.hpp>
#include <agrobus/net/event.hpp>
#include <agrobus/net/fast_packet.hpp>
#include <agrobus/net/message.hpp>
#include <agrobus/net/network_manager.hpp>
#include <datapod/datapod.hpp>
#include <echo/echo.hpp>

namespace agrobus::nmea {
    using namespace agrobus::net;

    // ─── NMEA Config ─────────────────────────────────────────────────────────────
    struct NMEAConfig {
        bool listen_rapid_position = true;
        bool listen_cog_sog = true;
        bool listen_attitude = true;
        bool listen_rate_of_turn = true;
        bool listen_position_detail = true;
        bool listen_gnss_dops = false;
        bool listen_magnetic_variation = false;
        bool listen_wind = false;
        bool listen_temperature = false;
        bool listen_humidity = false;
        bool listen_pressure = false;
        bool listen_outside_environmental = false;
        bool listen_engine = false;
        bool listen_fluid_level = false;
        bool listen_battery = false;
        bool listen_depth = false;
        bool listen_speed_water = false;
        bool listen_xte = false;
        bool listen_rudder = false;
        bool listen_system_time = false;
        bool listen_heading = false;

        NMEAConfig &rapid_position(bool enable) {
            listen_rapid_position = enable;
            return *this;
        }
        NMEAConfig &cog_sog(bool enable) {
            listen_cog_sog = enable;
            return *this;
        }
        NMEAConfig &attitude(bool enable) {
            listen_attitude = enable;
            return *this;
        }
        NMEAConfig &rate_of_turn(bool enable) {
            listen_rate_of_turn = enable;
            return *this;
        }
        NMEAConfig &position_detail(bool enable) {
            listen_position_detail = enable;
            return *this;
        }
        NMEAConfig &gnss_dops(bool enable) {
            listen_gnss_dops = enable;
            return *this;
        }
        NMEAConfig &magnetic_variation(bool enable) {
            listen_magnetic_variation = enable;
            return *this;
        }
        NMEAConfig &wind(bool enable) {
            listen_wind = enable;
            return *this;
        }
        NMEAConfig &temperature(bool enable) {
            listen_temperature = enable;
            return *this;
        }
        NMEAConfig &humidity(bool enable) {
            listen_humidity = enable;
            return *this;
        }
        NMEAConfig &pressure(bool enable) {
            listen_pressure = enable;
            return *this;
        }
        NMEAConfig &outside_environmental(bool enable) {
            listen_outside_environmental = enable;
            return *this;
        }
        NMEAConfig &engine(bool enable) {
            listen_engine = enable;
            return *this;
        }
        NMEAConfig &fluid_level(bool enable) {
            listen_fluid_level = enable;
            return *this;
        }
        NMEAConfig &battery(bool enable) {
            listen_battery = enable;
            return *this;
        }
        NMEAConfig &depth(bool enable) {
            listen_depth = enable;
            return *this;
        }
        NMEAConfig &speed_water(bool enable) {
            listen_speed_water = enable;
            return *this;
        }
        NMEAConfig &xte(bool enable) {
            listen_xte = enable;
            return *this;
        }
        NMEAConfig &rudder(bool enable) {
            listen_rudder = enable;
            return *this;
        }
        NMEAConfig &system_time(bool enable) {
            listen_system_time = enable;
            return *this;
        }
        NMEAConfig &heading(bool enable) {
            listen_heading = enable;
            return *this;
        }
        NMEAConfig &all(bool enable = true) {
            listen_rapid_position = listen_cog_sog = listen_attitude = listen_rate_of_turn = listen_position_detail =
                listen_gnss_dops = listen_magnetic_variation = listen_wind = listen_temperature = listen_humidity =
                    listen_pressure = listen_outside_environmental = listen_engine = listen_fluid_level =
                        listen_battery = listen_depth = listen_speed_water = listen_xte = listen_rudder =
                            listen_system_time = listen_heading = enable;
            return *this;
        }
    };

    // ─── NMEA2000 Interface ─────────────────────────────────────────────────────
    class NMEAInterface {
        IsoNet &net_;
        InternalCF *cf_;
        NMEAConfig config_;
        FastPacketProtocol fast_packet_;
        dp::Optional<GNSSPosition> latest_position_;

      public:
        NMEAInterface(IsoNet &net, InternalCF *cf, NMEAConfig config = {}) : net_(net), cf_(cf), config_(config) {}

        Result<void> initialize() {
            if (!cf_) {
                return Result<void>::err(Error::invalid_state("control function not set"));
            }
            // Single-frame PGNs (8 bytes)
            if (config_.listen_rapid_position) {
                net_.register_pgn_callback(PGN_GNSS_POSITION_RAPID,
                                           [this](const Message &msg) { handle_position_rapid(msg); });
            }
            if (config_.listen_cog_sog) {
                net_.register_pgn_callback(PGN_GNSS_COG_SOG_RAPID, [this](const Message &msg) { handle_cog_sog(msg); });
            }
            if (config_.listen_attitude) {
                net_.register_pgn_callback(PGN_ATTITUDE, [this](const Message &msg) { handle_attitude(msg); });
            }
            if (config_.listen_rate_of_turn) {
                net_.register_pgn_callback(PGN_RATE_OF_TURN, [this](const Message &msg) { handle_rate_of_turn(msg); });
            }
            // Fast Packet PGNs (>8 bytes, reassembled by transport)
            if (config_.listen_position_detail) {
                net_.register_pgn_callback(PGN_GNSS_POSITION_DATA,
                                           [this](const Message &msg) { handle_position_detail(msg); });
            }
            if (config_.listen_wind) {
                net_.register_pgn_callback(PGN_WIND_DATA, [this](const Message &msg) { handle_wind(msg); });
            }
            if (config_.listen_temperature) {
                net_.register_pgn_callback(PGN_TEMPERATURE, [this](const Message &msg) { handle_temperature(msg); });
            }
            if (config_.listen_engine) {
                net_.register_pgn_callback(PGN_ENGINE_PARAMS_RAPID, [this](const Message &msg) { handle_engine(msg); });
            }
            if (config_.listen_depth) {
                net_.register_pgn_callback(PGN_WATER_DEPTH, [this](const Message &msg) { handle_depth(msg); });
            }
            if (config_.listen_system_time) {
                net_.register_pgn_callback(PGN_SYSTEM_TIME, [this](const Message &msg) { handle_system_time(msg); });
            }
            if (config_.listen_heading) {
                net_.register_pgn_callback(PGN_HEADING_TRACK, [this](const Message &msg) { handle_heading(msg); });
            }
            if (config_.listen_gnss_dops) {
                net_.register_pgn_callback(PGN_GNSS_DOPs, [this](const Message &msg) { handle_gnss_dops(msg); });
            }
            if (config_.listen_magnetic_variation) {
                net_.register_pgn_callback(PGN_MAGNETIC_VARIATION,
                                           [this](const Message &msg) { handle_magnetic_variation(msg); });
            }
            if (config_.listen_humidity) {
                net_.register_pgn_callback(PGN_HUMIDITY, [this](const Message &msg) { handle_humidity(msg); });
            }
            if (config_.listen_pressure) {
                net_.register_pgn_callback(PGN_PRESSURE, [this](const Message &msg) { handle_pressure(msg); });
            }
            if (config_.listen_outside_environmental) {
                net_.register_pgn_callback(PGN_OUTSIDE_ENVIRONMENTAL,
                                           [this](const Message &msg) { handle_outside_environmental(msg); });
            }
            if (config_.listen_fluid_level) {
                net_.register_pgn_callback(PGN_FLUID_LEVEL, [this](const Message &msg) { handle_fluid_level(msg); });
            }
            if (config_.listen_battery) {
                net_.register_pgn_callback(PGN_BATTERY_STATUS, [this](const Message &msg) { handle_battery(msg); });
            }
            if (config_.listen_speed_water) {
                net_.register_pgn_callback(PGN_SPEED_WATER, [this](const Message &msg) { handle_speed_water(msg); });
            }
            if (config_.listen_xte) {
                net_.register_pgn_callback(PGN_XTE, [this](const Message &msg) { handle_xte(msg); });
            }
            if (config_.listen_rudder) {
                net_.register_pgn_callback(PGN_RUDDER, [this](const Message &msg) { handle_rudder(msg); });
            }
            echo::category("isobus.nmea").debug("initialized");
            return {};
        }

        dp::Optional<GNSSPosition> latest_position() const noexcept { return latest_position_; }

        // Send position (if we are a GNSS source)
        Result<void> send_position(const GNSSPosition &pos) {
            echo::category("isobus.nmea").debug("sending position");
            // PGN 129025 - Position Rapid Update (8 bytes, single frame)
            dp::Vector<u8> data(8, 0xFF);
            i32 lat_raw = static_cast<i32>(pos.wgs.latitude / LAT_LON_RESOLUTION);
            i32 lon_raw = static_cast<i32>(pos.wgs.longitude / LAT_LON_RESOLUTION);
            data[0] = static_cast<u8>(lat_raw & 0xFF);
            data[1] = static_cast<u8>((lat_raw >> 8) & 0xFF);
            data[2] = static_cast<u8>((lat_raw >> 16) & 0xFF);
            data[3] = static_cast<u8>((lat_raw >> 24) & 0xFF);
            data[4] = static_cast<u8>(lon_raw & 0xFF);
            data[5] = static_cast<u8>((lon_raw >> 8) & 0xFF);
            data[6] = static_cast<u8>((lon_raw >> 16) & 0xFF);
            data[7] = static_cast<u8>((lon_raw >> 24) & 0xFF);
            return net_.send(PGN_GNSS_POSITION_RAPID, data, cf_);
        }

        Result<void> send_cog_sog(f64 cog_rad, f64 sog_mps) {
            echo::category("isobus.nmea").debug("sending cog/sog");
            dp::Vector<u8> data(8, 0xFF);
            // SID
            data[0] = 0xFF;
            // COG reference (true = 0)
            data[1] = 0x00;
            // COG
            u16 cog_raw = static_cast<u16>(cog_rad / COG_RESOLUTION);
            data[2] = static_cast<u8>(cog_raw & 0xFF);
            data[3] = static_cast<u8>((cog_raw >> 8) & 0xFF);
            // SOG
            u16 sog_raw = static_cast<u16>(sog_mps / SPEED_RESOLUTION);
            data[4] = static_cast<u8>(sog_raw & 0xFF);
            data[5] = static_cast<u8>((sog_raw >> 8) & 0xFF);
            return net_.send(PGN_GNSS_COG_SOG_RAPID, data, cf_);
        }

        // ─── Additional send methods ──────────────────────────────────────────────
        Result<void> send_wind(const WindData &wind) {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = wind.sid;
            u16 speed_raw = static_cast<u16>(wind.speed_mps / WIND_SPEED_RESOLUTION);
            data[1] = static_cast<u8>(speed_raw & 0xFF);
            data[2] = static_cast<u8>((speed_raw >> 8) & 0xFF);
            u16 dir_raw = static_cast<u16>(wind.direction_rad / WIND_DIR_RESOLUTION);
            data[3] = static_cast<u8>(dir_raw & 0xFF);
            data[4] = static_cast<u8>((dir_raw >> 8) & 0xFF);
            data[5] = static_cast<u8>(wind.reference);
            return net_.send(PGN_WIND_DATA, data, cf_);
        }

        Result<void> send_temperature(const TemperatureData &temp) {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = temp.sid;
            data[1] = temp.instance;
            data[2] = static_cast<u8>(temp.source);
            u16 temp_raw = static_cast<u16>(temp.actual_k / TEMPERATURE_RESOLUTION);
            data[3] = static_cast<u8>(temp_raw & 0xFF);
            data[4] = static_cast<u8>((temp_raw >> 8) & 0xFF);
            return net_.send(PGN_TEMPERATURE, data, cf_);
        }

        Result<void> send_engine_params(const EngineData &engine) {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = engine.instance;
            u16 rpm_raw = static_cast<u16>(engine.rpm / RPM_RESOLUTION);
            data[1] = static_cast<u8>(rpm_raw & 0xFF);
            data[2] = static_cast<u8>((rpm_raw >> 8) & 0xFF);
            u16 boost_raw = static_cast<u16>(engine.boost_pressure_pa / PRESSURE_RESOLUTION);
            data[3] = static_cast<u8>(boost_raw & 0xFF);
            data[4] = static_cast<u8>((boost_raw >> 8) & 0xFF);
            data[5] = static_cast<u8>(engine.tilt_trim);
            return net_.send(PGN_ENGINE_PARAMS_RAPID, data, cf_);
        }

        Result<void> send_depth(const WaterDepthData &depth) {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = depth.sid;
            u32 depth_raw = static_cast<u32>(depth.depth_m / DEPTH_RESOLUTION);
            data[1] = static_cast<u8>(depth_raw & 0xFF);
            data[2] = static_cast<u8>((depth_raw >> 8) & 0xFF);
            data[3] = static_cast<u8>((depth_raw >> 16) & 0xFF);
            data[4] = static_cast<u8>((depth_raw >> 24) & 0xFF);
            i16 offset_raw = static_cast<i16>(depth.offset_m / DEPTH_RESOLUTION);
            data[5] = static_cast<u8>(static_cast<u16>(offset_raw) & 0xFF);
            data[6] = static_cast<u8>((static_cast<u16>(offset_raw) >> 8) & 0xFF);
            return net_.send(PGN_WATER_DEPTH, data, cf_);
        }

        Result<void> send_heading(f64 heading_rad, f64 deviation_rad = 0.0, f64 variation_rad = 0.0) {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = 0xFF; // SID
            u16 heading_raw = static_cast<u16>(heading_rad / HEADING_RESOLUTION);
            data[1] = static_cast<u8>(heading_raw & 0xFF);
            data[2] = static_cast<u8>((heading_raw >> 8) & 0xFF);
            i16 dev_raw = static_cast<i16>(deviation_rad / HEADING_RESOLUTION);
            data[3] = static_cast<u8>(static_cast<u16>(dev_raw) & 0xFF);
            data[4] = static_cast<u8>((static_cast<u16>(dev_raw) >> 8) & 0xFF);
            i16 var_raw = static_cast<i16>(variation_rad / HEADING_RESOLUTION);
            data[5] = static_cast<u8>(static_cast<u16>(var_raw) & 0xFF);
            data[6] = static_cast<u8>((static_cast<u16>(var_raw) >> 8) & 0xFF);
            data[7] = 0x00; // Reference: magnetic
            return net_.send(PGN_HEADING_TRACK, data, cf_);
        }

        Result<void> send_system_time(const SystemTimeData &time) {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = time.sid;
            data[1] = static_cast<u8>(time.source);
            data[2] = static_cast<u8>(time.days_since_epoch & 0xFF);
            data[3] = static_cast<u8>((time.days_since_epoch >> 8) & 0xFF);
            u32 secs_raw = static_cast<u32>(time.seconds_since_midnight * 10000.0);
            data[4] = static_cast<u8>(secs_raw & 0xFF);
            data[5] = static_cast<u8>((secs_raw >> 8) & 0xFF);
            data[6] = static_cast<u8>((secs_raw >> 16) & 0xFF);
            data[7] = static_cast<u8>((secs_raw >> 24) & 0xFF);
            return net_.send(PGN_SYSTEM_TIME, data, cf_);
        }

        // ─── Send methods for new PGNs ─────────────────────────────────────────────

        Result<void> send_rudder(const RudderData &rudder) {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = rudder.instance;
            data[1] = static_cast<u8>(rudder.direction) & 0x07;
            i16 angle_raw = static_cast<i16>(rudder.position_rad / HEADING_RESOLUTION);
            data[2] = static_cast<u8>(static_cast<u16>(angle_raw) & 0xFF);
            data[3] = static_cast<u8>((static_cast<u16>(angle_raw) >> 8) & 0xFF);
            i16 order_raw = static_cast<i16>(rudder.angle_order_rad / HEADING_RESOLUTION);
            data[4] = static_cast<u8>(static_cast<u16>(order_raw) & 0xFF);
            data[5] = static_cast<u8>((static_cast<u16>(order_raw) >> 8) & 0xFF);
            return net_.send(PGN_RUDDER, data, cf_);
        }

        Result<void> send_fluid_level(const FluidLevelData &fluid) {
            dp::Vector<u8> data(8, 0xFF);
            // Instance and type packed: lower 4 bits = instance, upper 4 bits = type
            data[0] = (fluid.instance & 0x0F) | (static_cast<u8>(fluid.type) << 4);
            u16 level_raw = static_cast<u16>(fluid.level_pct / FLUID_LEVEL_RESOLUTION);
            data[1] = static_cast<u8>(level_raw & 0xFF);
            data[2] = static_cast<u8>((level_raw >> 8) & 0xFF);
            u32 capacity_raw = static_cast<u32>(fluid.capacity_l / FLUID_CAPACITY_RESOLUTION);
            data[3] = static_cast<u8>(capacity_raw & 0xFF);
            data[4] = static_cast<u8>((capacity_raw >> 8) & 0xFF);
            data[5] = static_cast<u8>((capacity_raw >> 16) & 0xFF);
            data[6] = static_cast<u8>((capacity_raw >> 24) & 0xFF);
            return net_.send(PGN_FLUID_LEVEL, data, cf_);
        }

        Result<void> send_battery_status(const BatteryStatusData &bat) {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = bat.instance;
            u16 voltage_raw = static_cast<u16>(bat.voltage / VOLTAGE_RESOLUTION);
            data[1] = static_cast<u8>(voltage_raw & 0xFF);
            data[2] = static_cast<u8>((voltage_raw >> 8) & 0xFF);
            i16 current_raw = static_cast<i16>(bat.current_a / CURRENT_RESOLUTION);
            data[3] = static_cast<u8>(static_cast<u16>(current_raw) & 0xFF);
            data[4] = static_cast<u8>((static_cast<u16>(current_raw) >> 8) & 0xFF);
            // Bytes 5-6: temperature (Kelvin), 7: SID - not stored in struct
            return net_.send(PGN_BATTERY_STATUS, data, cf_);
        }

        Result<void> send_speed_water(const SpeedWaterData &spd) {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = spd.sid;
            u16 water_raw = static_cast<u16>(spd.water_speed_mps / SPEED_RESOLUTION);
            data[1] = static_cast<u8>(water_raw & 0xFF);
            data[2] = static_cast<u8>((water_raw >> 8) & 0xFF);
            u16 ground_raw = static_cast<u16>(spd.ground_speed_mps / SPEED_RESOLUTION);
            data[3] = static_cast<u8>(ground_raw & 0xFF);
            data[4] = static_cast<u8>((ground_raw >> 8) & 0xFF);
            data[5] = static_cast<u8>(spd.reference);
            return net_.send(PGN_SPEED_WATER, data, cf_);
        }

        Result<void> send_xte(const XTEData &xte) {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = xte.sid;
            data[1] = static_cast<u8>(xte.mode);
            // Navigation terminated flag in bit 6 of byte 1
            if (xte.navigation_terminated)
                data[1] |= 0x40;
            i32 xte_raw = static_cast<i32>(xte.xte_m / XTE_RESOLUTION);
            data[2] = static_cast<u8>(static_cast<u32>(xte_raw) & 0xFF);
            data[3] = static_cast<u8>((static_cast<u32>(xte_raw) >> 8) & 0xFF);
            data[4] = static_cast<u8>((static_cast<u32>(xte_raw) >> 16) & 0xFF);
            data[5] = static_cast<u8>((static_cast<u32>(xte_raw) >> 24) & 0xFF);
            return net_.send(PGN_XTE, data, cf_);
        }

        Result<void> send_humidity(const HumidityData &hum) {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = hum.sid;
            data[1] = hum.instance;
            data[2] = static_cast<u8>(hum.source);
            u16 actual_raw = static_cast<u16>(hum.actual_pct / HUMIDITY_RESOLUTION);
            data[3] = static_cast<u8>(actual_raw & 0xFF);
            data[4] = static_cast<u8>((actual_raw >> 8) & 0xFF);
            u16 set_raw = static_cast<u16>(hum.set_pct / HUMIDITY_RESOLUTION);
            data[5] = static_cast<u8>(set_raw & 0xFF);
            data[6] = static_cast<u8>((set_raw >> 8) & 0xFF);
            return net_.send(PGN_HUMIDITY, data, cf_);
        }

        Result<void> send_pressure(const PressureData &pres) {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = pres.sid;
            data[1] = pres.instance;
            data[2] = static_cast<u8>(pres.source);
            u32 pressure_raw = static_cast<u32>(pres.pressure_pa / PRESSURE_RESOLUTION);
            data[3] = static_cast<u8>(pressure_raw & 0xFF);
            data[4] = static_cast<u8>((pressure_raw >> 8) & 0xFF);
            data[5] = static_cast<u8>((pressure_raw >> 16) & 0xFF);
            data[6] = static_cast<u8>((pressure_raw >> 24) & 0xFF);
            return net_.send(PGN_PRESSURE, data, cf_);
        }

        Result<void> send_magnetic_variation(f64 variation_rad, u16 age_days = 0xFFFF) {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = 0xFF; // SID
            data[1] = 0x00; // Source: manual
            data[2] = static_cast<u8>(age_days & 0xFF);
            data[3] = static_cast<u8>((age_days >> 8) & 0xFF);
            i16 var_raw = static_cast<i16>(variation_rad / HEADING_RESOLUTION);
            data[4] = static_cast<u8>(static_cast<u16>(var_raw) & 0xFF);
            data[5] = static_cast<u8>((static_cast<u16>(var_raw) >> 8) & 0xFF);
            return net_.send(PGN_MAGNETIC_VARIATION, data, cf_);
        }

        // Events
        Event<const GNSSPosition &> on_position;
        Event<f64> on_cog;                // Course over ground (radians)
        Event<f64> on_sog;                // Speed over ground (m/s)
        Event<f64, f64, f64> on_attitude; // (yaw, pitch, roll) in radians
        Event<const WindData &> on_wind;
        Event<const TemperatureData &> on_temperature;
        Event<const EngineData &> on_engine;
        Event<const WaterDepthData &> on_depth;
        Event<f64> on_heading; // Heading in radians
        Event<const SystemTimeData &> on_system_time;
        Event<const GNSSDOPData &> on_gnss_dops;
        Event<f64> on_magnetic_variation; // Magnetic variation in radians
        Event<const RudderData &> on_rudder;
        Event<const FluidLevelData &> on_fluid_level;
        Event<const BatteryStatusData &> on_battery;
        Event<const SpeedWaterData &> on_speed_water;
        Event<const XTEData &> on_xte;
        Event<const HumidityData &> on_humidity;
        Event<const PressureData &> on_pressure;
        Event<const OutsideEnvironmentalData &> on_outside_environmental;

      private:
        void handle_position_rapid(const Message &msg) {
            if (msg.data.size() < 8)
                return;

            i32 lat_raw = static_cast<i32>(msg.get_u32_le(0));
            i32 lon_raw = static_cast<i32>(msg.get_u32_le(4));

            if (lat_raw == static_cast<i32>(0x7FFFFFFF) || lon_raw == static_cast<i32>(0x7FFFFFFF))
                return;

            GNSSPosition pos;
            pos.wgs = concord::earth::WGS(static_cast<f64>(lat_raw) * LAT_LON_RESOLUTION,
                                          static_cast<f64>(lon_raw) * LAT_LON_RESOLUTION);
            pos.timestamp_us = msg.timestamp_us;
            pos.fix_type = GNSSFixType::GNSSFix; // Assume fix if we got data

            if (latest_position_) {
                // Preserve fields not in this message
                pos.heading_rad = latest_position_->heading_rad;
                pos.speed_mps = latest_position_->speed_mps;
                pos.satellites_used = latest_position_->satellites_used;
                pos.hdop = latest_position_->hdop;
                pos.pdop = latest_position_->pdop;
            }

            latest_position_ = pos;
            on_position.emit(pos);
            echo::category("isobus.nmea").trace("Position: ", pos.wgs.latitude, ", ", pos.wgs.longitude);
        }

        void handle_cog_sog(const Message &msg) {
            if (msg.data.size() < 6)
                return;

            u16 cog_raw = msg.get_u16_le(2);
            u16 sog_raw = msg.get_u16_le(4);

            if (cog_raw != 0xFFFF) {
                f64 cog = static_cast<f64>(cog_raw) * COG_RESOLUTION;
                on_cog.emit(cog);
                if (latest_position_)
                    latest_position_->cog_rad = cog;
            }
            if (sog_raw != 0xFFFF) {
                f64 sog = static_cast<f64>(sog_raw) * SPEED_RESOLUTION;
                on_sog.emit(sog);
                if (latest_position_)
                    latest_position_->speed_mps = sog;
            }
        }

        void handle_attitude(const Message &msg) {
            if (msg.data.size() < 7)
                return;
            // SID at byte 0
            i16 yaw_raw = static_cast<i16>(msg.get_u16_le(1));
            i16 pitch_raw = static_cast<i16>(msg.get_u16_le(3));
            i16 roll_raw = static_cast<i16>(msg.get_u16_le(5));

            f64 yaw = static_cast<f64>(yaw_raw) * HEADING_RESOLUTION;
            f64 pitch = static_cast<f64>(pitch_raw) * HEADING_RESOLUTION;
            f64 roll = static_cast<f64>(roll_raw) * HEADING_RESOLUTION;

            if (latest_position_) {
                latest_position_->heading_rad = yaw;
                latest_position_->pitch_rad = pitch;
                latest_position_->roll_rad = roll;
            }

            on_attitude.emit(yaw, pitch, roll);
        }

        void handle_rate_of_turn(const Message &msg) {
            if (msg.data.size() < 5)
                return;
            i32 rot_raw = static_cast<i32>(msg.get_u32_le(1));
            if (rot_raw != static_cast<i32>(0x7FFFFFFF)) {
                f64 rot = static_cast<f64>(rot_raw) * ROT_RESOLUTION;
                if (latest_position_)
                    latest_position_->rate_of_turn_rps = rot;
            }
        }

        void handle_wind(const Message &msg) {
            if (msg.data.size() < 6)
                return;
            WindData wind;
            wind.sid = msg.data[0];
            u16 speed_raw = msg.get_u16_le(1);
            u16 dir_raw = msg.get_u16_le(3);
            wind.reference = static_cast<WindReference>(msg.data[5]);
            if (speed_raw != 0xFFFF)
                wind.speed_mps = static_cast<f64>(speed_raw) * WIND_SPEED_RESOLUTION;
            if (dir_raw != 0xFFFF)
                wind.direction_rad = static_cast<f64>(dir_raw) * WIND_DIR_RESOLUTION;
            on_wind.emit(wind);
            echo::category("isobus.nmea").trace("wind: ", wind.speed_mps, " m/s dir=", wind.direction_rad);
        }

        void handle_temperature(const Message &msg) {
            if (msg.data.size() < 5)
                return;
            TemperatureData temp;
            temp.sid = msg.data[0];
            temp.instance = msg.data[1];
            temp.source = static_cast<TemperatureSource>(msg.data[2]);
            u16 temp_raw = msg.get_u16_le(3);
            if (temp_raw != 0xFFFF)
                temp.actual_k = static_cast<f64>(temp_raw) * TEMPERATURE_RESOLUTION;
            on_temperature.emit(temp);
        }

        void handle_engine(const Message &msg) {
            if (msg.data.size() < 6)
                return;
            EngineData engine;
            engine.instance = msg.data[0];
            u16 rpm_raw = msg.get_u16_le(1);
            u16 boost_raw = msg.get_u16_le(3);
            if (rpm_raw != 0xFFFF)
                engine.rpm = static_cast<f64>(rpm_raw) * RPM_RESOLUTION;
            if (boost_raw != 0xFFFF)
                engine.boost_pressure_pa = static_cast<f64>(boost_raw) * PRESSURE_RESOLUTION;
            engine.tilt_trim = static_cast<i8>(msg.data[5]);
            on_engine.emit(engine);
            echo::category("isobus.nmea").trace("engine: rpm=", engine.rpm);
        }

        void handle_depth(const Message &msg) {
            if (msg.data.size() < 7)
                return;
            WaterDepthData depth;
            depth.sid = msg.data[0];
            u32 depth_raw = msg.get_u32_le(1);
            i16 offset_raw = static_cast<i16>(msg.get_u16_le(5));
            if (depth_raw != 0xFFFFFFFF)
                depth.depth_m = static_cast<f64>(depth_raw) * DEPTH_RESOLUTION;
            depth.offset_m = static_cast<f64>(offset_raw) * DEPTH_RESOLUTION;
            on_depth.emit(depth);
        }

        void handle_heading(const Message &msg) {
            if (msg.data.size() < 4)
                return;
            u16 heading_raw = msg.get_u16_le(1);
            if (heading_raw != 0xFFFF) {
                f64 heading = static_cast<f64>(heading_raw) * HEADING_RESOLUTION;
                if (latest_position_)
                    latest_position_->heading_rad = heading;
                on_heading.emit(heading);
            }
        }

        void handle_system_time(const Message &msg) {
            if (msg.data.size() < 8)
                return;
            SystemTimeData time;
            time.sid = msg.data[0];
            time.source = static_cast<TimeSource>(msg.data[1]);
            time.days_since_epoch = msg.get_u16_le(2);
            u32 secs_raw = msg.get_u32_le(4);
            if (secs_raw != 0xFFFFFFFF)
                time.seconds_since_midnight = static_cast<f64>(secs_raw) / 10000.0;
            on_system_time.emit(time);
        }

        // PGN 129539 - GNSS DOPs (8 bytes)
        void handle_gnss_dops(const Message &msg) {
            if (msg.data.size() < 8)
                return;
            GNSSDOPData dops;
            dops.sid = msg.data[0];
            dops.desired_mode = static_cast<GNSSDOPMode>((msg.data[1]) & 0x07);
            dops.actual_mode = static_cast<GNSSDOPMode>((msg.data[1] >> 3) & 0x07);
            i16 hdop_raw = static_cast<i16>(msg.get_u16_le(2));
            i16 vdop_raw = static_cast<i16>(msg.get_u16_le(4));
            i16 tdop_raw = static_cast<i16>(msg.get_u16_le(6));
            if (hdop_raw != static_cast<i16>(0x7FFF))
                dops.hdop = static_cast<f64>(hdop_raw) * DOP_RESOLUTION;
            if (vdop_raw != static_cast<i16>(0x7FFF))
                dops.vdop = static_cast<f64>(vdop_raw) * DOP_RESOLUTION;
            if (tdop_raw != static_cast<i16>(0x7FFF))
                dops.tdop = static_cast<f64>(tdop_raw) * DOP_RESOLUTION;
            // Update cached position
            if (latest_position_) {
                if (dops.hdop > 0.0)
                    latest_position_->hdop = dops.hdop;
                if (dops.vdop > 0.0)
                    latest_position_->vdop = dops.vdop;
            }
            on_gnss_dops.emit(dops);
        }

        // PGN 127258 - Magnetic Variation (8 bytes)
        void handle_magnetic_variation(const Message &msg) {
            if (msg.data.size() < 6)
                return;
            // SID at byte 0, source at byte 1
            // Age of service (days) at bytes 2-3
            i16 var_raw = static_cast<i16>(msg.get_u16_le(4));
            if (var_raw != static_cast<i16>(0x7FFF)) {
                f64 variation = static_cast<f64>(var_raw) * HEADING_RESOLUTION;
                on_magnetic_variation.emit(variation);
            }
        }

        // PGN 127245 - Rudder (8 bytes)
        void handle_rudder(const Message &msg) {
            if (msg.data.size() < 6)
                return;
            RudderData rudder;
            rudder.instance = msg.data[0];
            rudder.direction = static_cast<RudderDirection>((msg.data[1]) & 0x07);
            i16 angle_raw = static_cast<i16>(msg.get_u16_le(2));
            i16 order_raw = static_cast<i16>(msg.get_u16_le(4));
            if (angle_raw != static_cast<i16>(0x7FFF))
                rudder.position_rad = static_cast<f64>(angle_raw) * HEADING_RESOLUTION;
            if (order_raw != static_cast<i16>(0x7FFF))
                rudder.angle_order_rad = static_cast<f64>(order_raw) * HEADING_RESOLUTION;
            on_rudder.emit(rudder);
        }

        // PGN 127505 - Fluid Level (8 bytes)
        void handle_fluid_level(const Message &msg) {
            if (msg.data.size() < 7)
                return;
            FluidLevelData fluid;
            fluid.instance = msg.data[0] & 0x0F;
            fluid.type = static_cast<FluidType>(msg.data[0] >> 4);
            u16 level_raw = msg.get_u16_le(1);
            if (level_raw != 0xFFFF)
                fluid.level_pct = static_cast<f64>(level_raw) * FLUID_LEVEL_RESOLUTION;
            u32 capacity_raw = msg.get_u32_le(3);
            if (capacity_raw != 0xFFFFFFFF)
                fluid.capacity_l = static_cast<f64>(capacity_raw) * FLUID_CAPACITY_RESOLUTION;
            on_fluid_level.emit(fluid);
        }

        // PGN 127508 - Battery Status (8 bytes)
        void handle_battery(const Message &msg) {
            if (msg.data.size() < 8)
                return;
            BatteryStatusData bat;
            bat.instance = msg.data[0];
            u16 voltage_raw = msg.get_u16_le(1);
            i16 current_raw = static_cast<i16>(msg.get_u16_le(3));
            if (voltage_raw != 0xFFFF)
                bat.voltage = static_cast<f64>(voltage_raw) * VOLTAGE_RESOLUTION;
            if (current_raw != static_cast<i16>(0x7FFF))
                bat.current_a = static_cast<f64>(current_raw) * CURRENT_RESOLUTION;
            on_battery.emit(bat);
        }

        // PGN 128259 - Speed, Water Referenced (8 bytes)
        void handle_speed_water(const Message &msg) {
            if (msg.data.size() < 6)
                return;
            SpeedWaterData spd;
            spd.sid = msg.data[0];
            u16 water_raw = msg.get_u16_le(1);
            u16 ground_raw = msg.get_u16_le(3);
            spd.reference = static_cast<SpeedWaterRefType>(msg.data[5]);
            if (water_raw != 0xFFFF)
                spd.water_speed_mps = static_cast<f64>(water_raw) * SPEED_RESOLUTION;
            if (ground_raw != 0xFFFF)
                spd.ground_speed_mps = static_cast<f64>(ground_raw) * SPEED_RESOLUTION;
            on_speed_water.emit(spd);
        }

        // PGN 129283 - Cross Track Error (8 bytes)
        void handle_xte(const Message &msg) {
            if (msg.data.size() < 6)
                return;
            XTEData xte;
            xte.sid = msg.data[0];
            xte.mode = static_cast<XTEMode>(msg.data[1] & 0x0F);
            xte.navigation_terminated = (msg.data[1] & 0x40) != 0;
            i32 xte_raw = static_cast<i32>(msg.get_u32_le(2));
            if (xte_raw != static_cast<i32>(0x7FFFFFFF))
                xte.xte_m = static_cast<f64>(xte_raw) * XTE_RESOLUTION;
            on_xte.emit(xte);
        }

        // PGN 130313 - Humidity (8 bytes)
        void handle_humidity(const Message &msg) {
            if (msg.data.size() < 7)
                return;
            HumidityData hum;
            hum.sid = msg.data[0];
            hum.instance = msg.data[1];
            hum.source = static_cast<HumiditySource>(msg.data[2]);
            u16 actual_raw = msg.get_u16_le(3);
            u16 set_raw = msg.get_u16_le(5);
            if (actual_raw != 0xFFFF)
                hum.actual_pct = static_cast<f64>(actual_raw) * HUMIDITY_RESOLUTION;
            if (set_raw != 0xFFFF)
                hum.set_pct = static_cast<f64>(set_raw) * HUMIDITY_RESOLUTION;
            on_humidity.emit(hum);
        }

        // PGN 130314 - Pressure (8 bytes)
        void handle_pressure(const Message &msg) {
            if (msg.data.size() < 7)
                return;
            PressureData pres;
            pres.sid = msg.data[0];
            pres.instance = msg.data[1];
            pres.source = static_cast<PressureSource>(msg.data[2]);
            u32 pressure_raw = msg.get_u32_le(3);
            if (pressure_raw != 0xFFFFFFFF)
                pres.pressure_pa = static_cast<f64>(pressure_raw) * PRESSURE_RESOLUTION;
            on_pressure.emit(pres);
        }

        // PGN 130310 - Outside Environmental (8 bytes)
        void handle_outside_environmental(const Message &msg) {
            if (msg.data.size() < 7)
                return;
            OutsideEnvironmentalData env;
            env.sid = msg.data[0];
            u16 water_temp_raw = msg.get_u16_le(1);
            u16 air_temp_raw = msg.get_u16_le(3);
            u16 pressure_raw = msg.get_u16_le(5);
            if (water_temp_raw != 0xFFFF)
                env.water_temperature_k = static_cast<f64>(water_temp_raw) * TEMPERATURE_RESOLUTION;
            if (air_temp_raw != 0xFFFF)
                env.outside_temperature_k = static_cast<f64>(air_temp_raw) * TEMPERATURE_RESOLUTION;
            if (pressure_raw != 0xFFFF)
                env.atmospheric_pressure_pa = static_cast<f64>(pressure_raw) * PRESSURE_RESOLUTION;
            on_outside_environmental.emit(env);
        }

        // PGN 129029 - GNSS Position Data (Fast Packet, 43+ bytes)
        // Format: SID(1) + Days(2) + Seconds(4) + Lat(8) + Lon(8) + Alt(8) +
        //         Type(1) + Method(1) + Integrity(1) + NumSVs(1) + HDOP(2) + PDOP(2) + ...
        void handle_position_detail(const Message &msg) {
            if (msg.data.size() < 43)
                return;

            GNSSPosition pos;
            pos.timestamp_us = msg.timestamp_us;

            // Latitude at offset 7: i64, 1e-16 degrees
            i64 lat_raw = 0;
            for (u8 i = 0; i < 8; ++i)
                lat_raw |= static_cast<i64>(msg.data[7 + i]) << (i * 8);
            if (lat_raw != static_cast<i64>(0x7FFFFFFFFFFFFFFFLL)) {
                f64 lat = static_cast<f64>(lat_raw) * 1e-16;
                // Longitude at offset 15: i64, 1e-16 degrees
                i64 lon_raw = 0;
                for (u8 i = 0; i < 8; ++i)
                    lon_raw |= static_cast<i64>(msg.data[15 + i]) << (i * 8);
                f64 lon = static_cast<f64>(lon_raw) * 1e-16;
                pos.wgs = concord::earth::WGS(lat, lon);
            }

            // Altitude at offset 23: i64, 1e-6 meters
            i64 alt_raw = 0;
            for (u8 i = 0; i < 8; ++i)
                alt_raw |= static_cast<i64>(msg.data[23 + i]) << (i * 8);
            if (alt_raw != static_cast<i64>(0x7FFFFFFFFFFFFFFFLL)) {
                pos.altitude_m = static_cast<f64>(alt_raw) * 1e-6;
            }

            // GNSS type at offset 31
            u8 type_byte = msg.data[31];
            u8 fix_method = (type_byte >> 4) & 0x0F;
            pos.fix_type = static_cast<GNSSFixType>(fix_method);

            // Number of SVs at offset 33
            pos.satellites_used = msg.data[33];

            // HDOP at offset 34: i16, 0.01 resolution
            i16 hdop_raw = static_cast<i16>(msg.data[34]) | (static_cast<i16>(msg.data[35]) << 8);
            if (hdop_raw != static_cast<i16>(0x7FFF))
                pos.hdop = static_cast<f64>(hdop_raw) * 0.01;

            // PDOP at offset 36: i16, 0.01 resolution
            i16 pdop_raw = static_cast<i16>(msg.data[36]) | (static_cast<i16>(msg.data[37]) << 8);
            if (pdop_raw != static_cast<i16>(0x7FFF))
                pos.pdop = static_cast<f64>(pdop_raw) * 0.01;

            // Preserve heading/speed from other messages
            if (latest_position_) {
                pos.heading_rad = latest_position_->heading_rad;
                pos.speed_mps = latest_position_->speed_mps;
                pos.cog_rad = latest_position_->cog_rad;
            }

            latest_position_ = pos;
            on_position.emit(pos);
            echo::category("isobus.nmea")
                .debug("GNSS detail: fix=", static_cast<u8>(pos.fix_type), " sats=", pos.satellites_used);
        }
    };

} // namespace agrobus::nmea
