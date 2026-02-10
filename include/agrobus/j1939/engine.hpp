#pragma once

#include <agrobus/net/constants.hpp>
#include <agrobus/net/error.hpp>
#include <agrobus/net/event.hpp>
#include <agrobus/net/internal_cf.hpp>
#include <agrobus/net/message.hpp>
#include <agrobus/net/network_manager.hpp>
#include <agrobus/net/types.hpp>
#include <datapod/datapod.hpp>
#include <echo/echo.hpp>

namespace agrobus::j1939 {
    using namespace agrobus::net;

    // ─── Electronic Engine Controller 1 (EEC1, PGN 0x0F004) ─────────────────────
    // Broadcast at 10-100ms by engine controller
    struct EEC1 {
        f64 engine_torque_percent = 0.0; // SPN 513: -125 to 125%, 1%/bit offset -125
        f64 driver_demand_percent = 0.0; // SPN 512: -125 to 125%, 1%/bit offset -125
        f64 actual_engine_percent = 0.0; // SPN 514: -125 to 125%, 1%/bit offset -125
        f64 engine_speed_rpm = 0.0;      // SPN 190: 0.125 rpm/bit, 2 bytes
        u8 starter_mode = 0xFF;          // SPN 1675: 4 bits
        u8 source_address = 0xFF;        // SPN 899: source of engine speed

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = static_cast<u8>(engine_torque_percent + 125.0);
            data[1] = static_cast<u8>(driver_demand_percent + 125.0);
            data[2] = static_cast<u8>(actual_engine_percent + 125.0);
            u16 rpm = static_cast<u16>(engine_speed_rpm / 0.125);
            data[3] = static_cast<u8>(rpm & 0xFF);
            data[4] = static_cast<u8>((rpm >> 8) & 0xFF);
            data[5] = source_address;
            data[6] = (starter_mode & 0x0F);
            data[7] = 0xFF; // reserved
            return data;
        }

        static EEC1 decode(const dp::Vector<u8> &data) {
            EEC1 msg;
            if (data.size() >= 8) {
                msg.engine_torque_percent = static_cast<f64>(data[0]) - 125.0;
                msg.driver_demand_percent = static_cast<f64>(data[1]) - 125.0;
                msg.actual_engine_percent = static_cast<f64>(data[2]) - 125.0;
                u16 rpm = static_cast<u16>(data[3]) | (static_cast<u16>(data[4]) << 8);
                msg.engine_speed_rpm = rpm * 0.125;
                msg.source_address = data[5];
                msg.starter_mode = data[6] & 0x0F;
            }
            return msg;
        }
    };

    // ─── Electronic Engine Controller 2 (EEC2, PGN 0x0F003) ─────────────────────
    struct EEC2 {
        u8 accel_pedal_position = 0xFF; // SPN 91: 0.4%/bit
        f64 engine_load_percent = 0.0;  // SPN 92: 1%/bit
        u8 accel_pedal_low_idle = 0x03; // SPN 558: 2 bits
        u8 accel_pedal_kickdown = 0x03; // SPN 559: 2 bits
        u8 road_speed_limit = 0xFF;     // SPN 1437: 1 km/h per bit

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = (accel_pedal_low_idle & 0x03) | ((accel_pedal_kickdown & 0x03) << 2);
            data[1] = accel_pedal_position;
            data[2] = static_cast<u8>(engine_load_percent);
            data[3] = road_speed_limit;
            return data;
        }

        static EEC2 decode(const dp::Vector<u8> &data) {
            EEC2 msg;
            if (data.size() >= 4) {
                msg.accel_pedal_low_idle = data[0] & 0x03;
                msg.accel_pedal_kickdown = (data[0] >> 2) & 0x03;
                msg.accel_pedal_position = data[1];
                msg.engine_load_percent = static_cast<f64>(data[2]);
                msg.road_speed_limit = data[3];
            }
            return msg;
        }
    };

    // ─── Engine Temperature 1 (ET1, PGN 0x0FEEE) ────────────────────────────────
    struct EngineTemp1 {
        f64 coolant_temp_c = -40.0;     // SPN 110: 1 C/bit, offset -40
        f64 fuel_temp_c = -40.0;        // SPN 174: 1 C/bit, offset -40
        f64 oil_temp_c = -40.0;         // SPN 175: 0.03125 C/bit, offset -273, 2 bytes
        f64 turbo_oil_temp_c = -40.0;   // SPN 176: 0.03125 C/bit, offset -273, 2 bytes
        f64 intercooler_temp_c = -40.0; // SPN 52: 1 C/bit, offset -40

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = static_cast<u8>(coolant_temp_c + 40.0);
            data[1] = static_cast<u8>(fuel_temp_c + 40.0);
            u16 oil = static_cast<u16>((oil_temp_c + 273.0) / 0.03125);
            data[2] = static_cast<u8>(oil & 0xFF);
            data[3] = static_cast<u8>((oil >> 8) & 0xFF);
            u16 turbo = static_cast<u16>((turbo_oil_temp_c + 273.0) / 0.03125);
            data[4] = static_cast<u8>(turbo & 0xFF);
            data[5] = static_cast<u8>((turbo >> 8) & 0xFF);
            data[6] = static_cast<u8>(intercooler_temp_c + 40.0);
            return data;
        }

        static EngineTemp1 decode(const dp::Vector<u8> &data) {
            EngineTemp1 msg;
            if (data.size() >= 7) {
                msg.coolant_temp_c = static_cast<f64>(data[0]) - 40.0;
                msg.fuel_temp_c = static_cast<f64>(data[1]) - 40.0;
                u16 oil = static_cast<u16>(data[2]) | (static_cast<u16>(data[3]) << 8);
                msg.oil_temp_c = oil * 0.03125 - 273.0;
                u16 turbo = static_cast<u16>(data[4]) | (static_cast<u16>(data[5]) << 8);
                msg.turbo_oil_temp_c = turbo * 0.03125 - 273.0;
                msg.intercooler_temp_c = static_cast<f64>(data[6]) - 40.0;
            }
            return msg;
        }
    };

    // ─── Engine Temperature 2 (ET2, PGN 0x0FEED) ────────────────────────────────
    struct EngineTemp2 {
        f64 engine_oil_temp_c = -40.0;    // SPN 175: 0.03125 C/bit, offset -273
        f64 turbo_oil_temp_c = -40.0;     // SPN 176: 0.03125 C/bit, offset -273
        f64 engine_intercooler_temp_c = -40.0; // SPN 52: 1 C/bit, offset -40
        f64 turbo_1_temp_c = -40.0;       // SPN 1134: 0.03125 C/bit, offset -273

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            u16 oil = static_cast<u16>((engine_oil_temp_c + 273.0) / 0.03125);
            data[0] = static_cast<u8>(oil & 0xFF);
            data[1] = static_cast<u8>((oil >> 8) & 0xFF);
            u16 turbo_oil = static_cast<u16>((turbo_oil_temp_c + 273.0) / 0.03125);
            data[2] = static_cast<u8>(turbo_oil & 0xFF);
            data[3] = static_cast<u8>((turbo_oil >> 8) & 0xFF);
            data[4] = static_cast<u8>(engine_intercooler_temp_c + 40.0);
            u16 turbo1 = static_cast<u16>((turbo_1_temp_c + 273.0) / 0.03125);
            data[5] = static_cast<u8>(turbo1 & 0xFF);
            data[6] = static_cast<u8>((turbo1 >> 8) & 0xFF);
            return data;
        }

        static EngineTemp2 decode(const dp::Vector<u8> &data) {
            EngineTemp2 msg;
            if (data.size() >= 7) {
                u16 oil = static_cast<u16>(data[0]) | (static_cast<u16>(data[1]) << 8);
                msg.engine_oil_temp_c = oil * 0.03125 - 273.0;
                u16 turbo_oil = static_cast<u16>(data[2]) | (static_cast<u16>(data[3]) << 8);
                msg.turbo_oil_temp_c = turbo_oil * 0.03125 - 273.0;
                msg.engine_intercooler_temp_c = static_cast<f64>(data[4]) - 40.0;
                u16 turbo1 = static_cast<u16>(data[5]) | (static_cast<u16>(data[6]) << 8);
                msg.turbo_1_temp_c = turbo1 * 0.03125 - 273.0;
            }
            return msg;
        }
    };

    // ─── Engine Fluid Level/Pressure (EFLP, PGN 0x0FEEF) ────────────────────────
    struct EngineFluidLP {
        f64 oil_pressure_kpa = 0.0;           // SPN 100: 4 kPa/bit
        f64 coolant_pressure_kpa = 0.0;       // SPN 109: 2 kPa/bit
        u8 oil_level_percent = 0xFF;          // SPN 98: 0.4%/bit
        u8 coolant_level_percent = 0xFF;      // SPN 111: 0.4%/bit
        f64 fuel_delivery_pressure_kpa = 0.0; // SPN 94: 4 kPa/bit
        f64 crankcase_pressure_kpa = 0.0;     // SPN 101: 0.05 kPa/bit, offset -250

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = static_cast<u8>(fuel_delivery_pressure_kpa / 4.0);
            data[1] = static_cast<u8>(oil_pressure_kpa / 4.0);
            data[2] = static_cast<u8>(coolant_pressure_kpa / 2.0);
            data[3] = oil_level_percent;
            data[4] = coolant_level_percent;
            u16 crank = static_cast<u16>((crankcase_pressure_kpa + 250.0) / 0.05);
            data[5] = static_cast<u8>(crank & 0xFF);
            data[6] = static_cast<u8>((crank >> 8) & 0xFF);
            return data;
        }

        static EngineFluidLP decode(const dp::Vector<u8> &data) {
            EngineFluidLP msg;
            if (data.size() >= 7) {
                msg.fuel_delivery_pressure_kpa = static_cast<f64>(data[0]) * 4.0;
                msg.oil_pressure_kpa = static_cast<f64>(data[1]) * 4.0;
                msg.coolant_pressure_kpa = static_cast<f64>(data[2]) * 2.0;
                msg.oil_level_percent = data[3];
                msg.coolant_level_percent = data[4];
                u16 crank = static_cast<u16>(data[5]) | (static_cast<u16>(data[6]) << 8);
                msg.crankcase_pressure_kpa = crank * 0.05 - 250.0;
            }
            return msg;
        }
    };

    // ─── Engine Hours (PGN 0x0FEE5) ─────────────────────────────────────────────
    struct EngineHours {
        f64 total_hours = 0.0;       // SPN 247: 0.05 hr/bit, 4 bytes
        f64 total_revolutions = 0.0; // SPN 249: 1000 rev/bit, 4 bytes (in thousands)

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            u32 hrs = static_cast<u32>(total_hours / 0.05);
            data[0] = static_cast<u8>(hrs & 0xFF);
            data[1] = static_cast<u8>((hrs >> 8) & 0xFF);
            data[2] = static_cast<u8>((hrs >> 16) & 0xFF);
            data[3] = static_cast<u8>((hrs >> 24) & 0xFF);
            u32 revs = static_cast<u32>(total_revolutions / 1000.0);
            data[4] = static_cast<u8>(revs & 0xFF);
            data[5] = static_cast<u8>((revs >> 8) & 0xFF);
            data[6] = static_cast<u8>((revs >> 16) & 0xFF);
            data[7] = static_cast<u8>((revs >> 24) & 0xFF);
            return data;
        }

        static EngineHours decode(const dp::Vector<u8> &data) {
            EngineHours msg;
            if (data.size() >= 8) {
                u32 hrs = static_cast<u32>(data[0]) | (static_cast<u32>(data[1]) << 8) |
                          (static_cast<u32>(data[2]) << 16) | (static_cast<u32>(data[3]) << 24);
                msg.total_hours = hrs * 0.05;
                u32 revs = static_cast<u32>(data[4]) | (static_cast<u32>(data[5]) << 8) |
                           (static_cast<u32>(data[6]) << 16) | (static_cast<u32>(data[7]) << 24);
                msg.total_revolutions = revs * 1000.0;
            }
            return msg;
        }
    };

    // ─── Fuel Economy (PGN 0x0FEF2) ─────────────────────────────────────────────
    struct FuelEconomy {
        f64 fuel_rate_lph = 0.0;     // SPN 183: 0.05 L/h per bit, 2 bytes
        f64 instantaneous_lph = 0.0; // SPN 184: 1/512 km/L per bit, 2 bytes
        f64 throttle_position = 0.0; // SPN 51: 0.4%/bit

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            u16 rate = static_cast<u16>(fuel_rate_lph / 0.05);
            data[0] = static_cast<u8>(rate & 0xFF);
            data[1] = static_cast<u8>((rate >> 8) & 0xFF);
            u16 inst = static_cast<u16>(instantaneous_lph * 512.0);
            data[2] = static_cast<u8>(inst & 0xFF);
            data[3] = static_cast<u8>((inst >> 8) & 0xFF);
            data[4] = static_cast<u8>(throttle_position / 0.4);
            return data;
        }

        static FuelEconomy decode(const dp::Vector<u8> &data) {
            FuelEconomy msg;
            if (data.size() >= 5) {
                u16 rate = static_cast<u16>(data[0]) | (static_cast<u16>(data[1]) << 8);
                msg.fuel_rate_lph = rate * 0.05;
                u16 inst = static_cast<u16>(data[2]) | (static_cast<u16>(data[3]) << 8);
                msg.instantaneous_lph = inst / 512.0;
                msg.throttle_position = static_cast<f64>(data[4]) * 0.4;
            }
            return msg;
        }
    };

    // ─── Electronic Engine Controller 3 (EEC3, PGN 0x0FEC0) ─────────────────────
    struct EEC3 {
        f64 nominal_friction_percent = 0.0;    // SPN 514: 1%/bit, offset -125
        f64 desired_operating_speed_rpm = 0.0; // SPN 515: 0.125 rpm/bit, 2 bytes
        u8 operating_speed_asymmetry = 0xFF;   // SPN 519: 1%/bit

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = static_cast<u8>(nominal_friction_percent + 125.0);
            u16 spd = static_cast<u16>(desired_operating_speed_rpm / 0.125);
            data[1] = static_cast<u8>(spd & 0xFF);
            data[2] = static_cast<u8>((spd >> 8) & 0xFF);
            data[3] = operating_speed_asymmetry;
            return data;
        }

        static EEC3 decode(const dp::Vector<u8> &data) {
            EEC3 msg;
            if (data.size() >= 4) {
                msg.nominal_friction_percent = static_cast<f64>(data[0]) - 125.0;
                u16 spd = static_cast<u16>(data[1]) | (static_cast<u16>(data[2]) << 8);
                msg.desired_operating_speed_rpm = spd * 0.125;
                msg.operating_speed_asymmetry = data[3];
            }
            return msg;
        }
    };

    // ─── Torque/Speed Control 1 (TSC1, PGN 0x0F006) ──────────────────────────────
    // Destination-specific command from controller to engine/retarder
    enum class OverrideControlMode : u8 { NoOverride = 0, SpeedControl = 1, TorqueControl = 2, SpeedTorqueLimit = 3 };

    struct TSC1 {
        OverrideControlMode override_mode = OverrideControlMode::NoOverride;
        f64 requested_speed_rpm = 0.0;      // SPN 898: 0.125 rpm/bit, 2 bytes
        f64 requested_torque_percent = 0.0; // SPN 518: 1%/bit, offset -125

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = static_cast<u8>(override_mode) & 0x03;
            u16 spd = static_cast<u16>(requested_speed_rpm / 0.125);
            data[1] = static_cast<u8>(spd & 0xFF);
            data[2] = static_cast<u8>((spd >> 8) & 0xFF);
            data[3] = static_cast<u8>(requested_torque_percent + 125.0);
            return data;
        }

        static TSC1 decode(const dp::Vector<u8> &data) {
            TSC1 msg;
            if (data.size() >= 4) {
                msg.override_mode = static_cast<OverrideControlMode>(data[0] & 0x03);
                u16 spd = static_cast<u16>(data[1]) | (static_cast<u16>(data[2]) << 8);
                msg.requested_speed_rpm = spd * 0.125;
                msg.requested_torque_percent = static_cast<f64>(data[3]) - 125.0;
            }
            return msg;
        }
    };

    // ─── Vehicle Electrical Power 1 (VEP1, PGN 0x0F009) ──────────────────────────
    struct VEP1 {
        f64 battery_voltage_v = 0.0;         // SPN 168: 0.05 V/bit, 2 bytes
        f64 alternator_current_a = 0.0;      // SPN 167: 1 A/bit offset -125, 1 byte (signed)
        f64 charging_system_voltage_v = 0.0; // SPN 158: 0.05 V/bit, 2 bytes
        f64 key_switch_voltage_v = 0.0;      // SPN 168: 0.05 V/bit, 2 bytes

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            u16 bat = static_cast<u16>(battery_voltage_v / 0.05);
            data[0] = static_cast<u8>(bat & 0xFF);
            data[1] = static_cast<u8>((bat >> 8) & 0xFF);
            u16 chrg = static_cast<u16>(charging_system_voltage_v / 0.05);
            data[2] = static_cast<u8>(chrg & 0xFF);
            data[3] = static_cast<u8>((chrg >> 8) & 0xFF);
            u16 key = static_cast<u16>(key_switch_voltage_v / 0.05);
            data[4] = static_cast<u8>(key & 0xFF);
            data[5] = static_cast<u8>((key >> 8) & 0xFF);
            data[6] = static_cast<u8>(alternator_current_a + 125.0);
            return data;
        }

        static VEP1 decode(const dp::Vector<u8> &data) {
            VEP1 msg;
            if (data.size() >= 7) {
                u16 bat = static_cast<u16>(data[0]) | (static_cast<u16>(data[1]) << 8);
                msg.battery_voltage_v = bat * 0.05;
                u16 chrg = static_cast<u16>(data[2]) | (static_cast<u16>(data[3]) << 8);
                msg.charging_system_voltage_v = chrg * 0.05;
                u16 key = static_cast<u16>(data[4]) | (static_cast<u16>(data[5]) << 8);
                msg.key_switch_voltage_v = key * 0.05;
                msg.alternator_current_a = static_cast<f64>(data[6]) - 125.0;
            }
            return msg;
        }
    };

    // ─── Ambient Conditions (PGN 0x0FEF5) ────────────────────────────────────────
    struct AmbientConditions {
        f64 barometric_pressure_kpa = 0.0; // SPN 108: 0.5 kPa/bit
        f64 ambient_air_temp_c = -40.0;    // SPN 171: 0.03125 C/bit, offset -273, 2 bytes
        f64 intake_air_temp_c = -40.0;     // SPN 172: 1 C/bit, offset -40
        f64 road_surface_temp_c = -40.0;   // SPN 79: 0.03125 C/bit, offset -273, 2 bytes

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = static_cast<u8>(barometric_pressure_kpa / 0.5);
            u16 amb = static_cast<u16>((ambient_air_temp_c + 273.0) / 0.03125);
            data[1] = static_cast<u8>(amb & 0xFF);
            data[2] = static_cast<u8>((amb >> 8) & 0xFF);
            data[3] = static_cast<u8>(intake_air_temp_c + 40.0);
            u16 road = static_cast<u16>((road_surface_temp_c + 273.0) / 0.03125);
            data[4] = static_cast<u8>(road & 0xFF);
            data[5] = static_cast<u8>((road >> 8) & 0xFF);
            return data;
        }

        static AmbientConditions decode(const dp::Vector<u8> &data) {
            AmbientConditions msg;
            if (data.size() >= 6) {
                msg.barometric_pressure_kpa = static_cast<f64>(data[0]) * 0.5;
                u16 amb = static_cast<u16>(data[1]) | (static_cast<u16>(data[2]) << 8);
                msg.ambient_air_temp_c = amb * 0.03125 - 273.0;
                msg.intake_air_temp_c = static_cast<f64>(data[3]) - 40.0;
                u16 road = static_cast<u16>(data[4]) | (static_cast<u16>(data[5]) << 8);
                msg.road_surface_temp_c = road * 0.03125 - 273.0;
            }
            return msg;
        }
    };

    // ─── Dash Display (PGN 0x0FEFC) ──────────────────────────────────────────────
    struct DashDisplay {
        u8 fuel_level_percent = 0xFF;     // SPN 96: 0.4%/bit
        u8 washer_fluid_level = 0xFF;     // SPN 80: 0.4%/bit
        f64 fuel_filter_diff_kpa = 0.0;   // SPN 95: 2 kPa/bit
        f64 oil_filter_diff_kpa = 0.0;    // SPN 99: 0.5 kPa/bit
        f64 cargo_ambient_temp_c = -40.0; // SPN 169: 0.03125 C/bit, offset -273, 2 bytes

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = washer_fluid_level;
            data[1] = fuel_level_percent;
            data[2] = static_cast<u8>(fuel_filter_diff_kpa / 2.0);
            data[3] = static_cast<u8>(oil_filter_diff_kpa / 0.5);
            u16 temp = static_cast<u16>((cargo_ambient_temp_c + 273.0) / 0.03125);
            data[4] = static_cast<u8>(temp & 0xFF);
            data[5] = static_cast<u8>((temp >> 8) & 0xFF);
            return data;
        }

        static DashDisplay decode(const dp::Vector<u8> &data) {
            DashDisplay msg;
            if (data.size() >= 6) {
                msg.washer_fluid_level = data[0];
                msg.fuel_level_percent = data[1];
                msg.fuel_filter_diff_kpa = static_cast<f64>(data[2]) * 2.0;
                msg.oil_filter_diff_kpa = static_cast<f64>(data[3]) * 0.5;
                u16 temp = static_cast<u16>(data[4]) | (static_cast<u16>(data[5]) << 8);
                msg.cargo_ambient_temp_c = temp * 0.03125 - 273.0;
            }
            return msg;
        }
    };

    // ─── Vehicle Position (PGN 0x0FEF7) ──────────────────────────────────────────
    struct VehiclePosition {
        f64 latitude_deg = 0.0;  // SPN 584: 1e-7 deg/bit, offset -210, 4 bytes
        f64 longitude_deg = 0.0; // SPN 585: 1e-7 deg/bit, offset -210, 4 bytes

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            u32 lat = static_cast<u32>((latitude_deg + 210.0) / 1e-7);
            data[0] = static_cast<u8>(lat & 0xFF);
            data[1] = static_cast<u8>((lat >> 8) & 0xFF);
            data[2] = static_cast<u8>((lat >> 16) & 0xFF);
            data[3] = static_cast<u8>((lat >> 24) & 0xFF);
            u32 lon = static_cast<u32>((longitude_deg + 210.0) / 1e-7);
            data[4] = static_cast<u8>(lon & 0xFF);
            data[5] = static_cast<u8>((lon >> 8) & 0xFF);
            data[6] = static_cast<u8>((lon >> 16) & 0xFF);
            data[7] = static_cast<u8>((lon >> 24) & 0xFF);
            return data;
        }

        static VehiclePosition decode(const dp::Vector<u8> &data) {
            VehiclePosition msg;
            if (data.size() >= 8) {
                u32 lat = static_cast<u32>(data[0]) | (static_cast<u32>(data[1]) << 8) |
                          (static_cast<u32>(data[2]) << 16) | (static_cast<u32>(data[3]) << 24);
                msg.latitude_deg = lat * 1e-7 - 210.0;
                u32 lon = static_cast<u32>(data[4]) | (static_cast<u32>(data[5]) << 8) |
                          (static_cast<u32>(data[6]) << 16) | (static_cast<u32>(data[7]) << 24);
                msg.longitude_deg = lon * 1e-7 - 210.0;
            }
            return msg;
        }
    };

    // ─── Fuel Consumption (PGN 0x0FEE9) ────────────────────────────────────────
    struct FuelConsumption {
        f64 trip_fuel_l = 0.0;  // SPN 182: 0.5 L/bit, 4 bytes (total trip fuel)
        f64 total_fuel_l = 0.0; // SPN 250: 0.5 L/bit, 4 bytes (lifetime fuel)

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            u32 trip = static_cast<u32>(trip_fuel_l / 0.5);
            data[0] = static_cast<u8>(trip & 0xFF);
            data[1] = static_cast<u8>((trip >> 8) & 0xFF);
            data[2] = static_cast<u8>((trip >> 16) & 0xFF);
            data[3] = static_cast<u8>((trip >> 24) & 0xFF);
            u32 total = static_cast<u32>(total_fuel_l / 0.5);
            data[4] = static_cast<u8>(total & 0xFF);
            data[5] = static_cast<u8>((total >> 8) & 0xFF);
            data[6] = static_cast<u8>((total >> 16) & 0xFF);
            data[7] = static_cast<u8>((total >> 24) & 0xFF);
            return data;
        }

        static FuelConsumption decode(const dp::Vector<u8> &data) {
            FuelConsumption msg;
            if (data.size() >= 8) {
                u32 trip = static_cast<u32>(data[0]) | (static_cast<u32>(data[1]) << 8) |
                           (static_cast<u32>(data[2]) << 16) | (static_cast<u32>(data[3]) << 24);
                msg.trip_fuel_l = trip * 0.5;
                u32 total = static_cast<u32>(data[4]) | (static_cast<u32>(data[5]) << 8) |
                            (static_cast<u32>(data[6]) << 16) | (static_cast<u32>(data[7]) << 24);
                msg.total_fuel_l = total * 0.5;
            }
            return msg;
        }
    };

    // ─── Aftertreatment 1 (AT1, PGN 65269 / 0x0FEF5) ────────────────────────────
    struct Aftertreatment1 {
        f64 diesel_exhaust_fluid_tank_level = 0.0; // SPN 1761: 0.4% per bit
        f64 intake_nox_ppm = 0.0;                  // SPN 3216: 0.05 ppm per bit, 2 bytes
        f64 outlet_nox_ppm = 0.0;                  // SPN 3226: 0.05 ppm per bit, 2 bytes
        f64 intake_nox_reading_status = 0xFF;      // SPN 3241: status
        f64 outlet_nox_reading_status = 0xFF;      // SPN 3242: status

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = static_cast<u8>(diesel_exhaust_fluid_tank_level / 0.4);
            u16 intake = static_cast<u16>(intake_nox_ppm / 0.05);
            data[1] = static_cast<u8>(intake & 0xFF);
            data[2] = static_cast<u8>((intake >> 8) & 0xFF);
            u16 outlet = static_cast<u16>(outlet_nox_ppm / 0.05);
            data[3] = static_cast<u8>(outlet & 0xFF);
            data[4] = static_cast<u8>((outlet >> 8) & 0xFF);
            data[5] = static_cast<u8>(intake_nox_reading_status);
            data[6] = static_cast<u8>(outlet_nox_reading_status);
            return data;
        }

        static Aftertreatment1 decode(const dp::Vector<u8> &data) {
            Aftertreatment1 msg;
            if (data.size() >= 7) {
                msg.diesel_exhaust_fluid_tank_level = static_cast<f64>(data[0]) * 0.4;
                u16 intake = static_cast<u16>(data[1]) | (static_cast<u16>(data[2]) << 8);
                msg.intake_nox_ppm = intake * 0.05;
                u16 outlet = static_cast<u16>(data[3]) | (static_cast<u16>(data[4]) << 8);
                msg.outlet_nox_ppm = outlet * 0.05;
                msg.intake_nox_reading_status = data[5];
                msg.outlet_nox_reading_status = data[6];
            }
            return msg;
        }
    };

    // ─── Aftertreatment 2 (AT2, PGN 65110 / 0x0FE46) ────────────────────────────
    struct Aftertreatment2 {
        f64 dpf_differential_pressure_kpa = 0.0;      // SPN 3719: 0.1 kPa per bit, 2 bytes
        f64 diesel_exhaust_fluid_concentration = 0.0; // SPN 4331: 0.4% per bit
        f64 dpf_soot_load_percent = 0.0;              // SPN 3720: 0.4% per bit
        u8 dpf_active_regeneration_status = 0xFF;     // SPN 3700: status
        u8 dpf_passive_regeneration_status = 0xFF;    // SPN 3701: status

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            u16 diff_press = static_cast<u16>(dpf_differential_pressure_kpa / 0.1);
            data[0] = static_cast<u8>(diff_press & 0xFF);
            data[1] = static_cast<u8>((diff_press >> 8) & 0xFF);
            data[2] = static_cast<u8>(diesel_exhaust_fluid_concentration / 0.4);
            data[3] = static_cast<u8>(dpf_soot_load_percent / 0.4);
            data[4] = dpf_active_regeneration_status;
            data[5] = dpf_passive_regeneration_status;
            return data;
        }

        static Aftertreatment2 decode(const dp::Vector<u8> &data) {
            Aftertreatment2 msg;
            if (data.size() >= 6) {
                u16 diff_press = static_cast<u16>(data[0]) | (static_cast<u16>(data[1]) << 8);
                msg.dpf_differential_pressure_kpa = diff_press * 0.1;
                msg.diesel_exhaust_fluid_concentration = static_cast<f64>(data[2]) * 0.4;
                msg.dpf_soot_load_percent = static_cast<f64>(data[3]) * 0.4;
                msg.dpf_active_regeneration_status = data[4];
                msg.dpf_passive_regeneration_status = data[5];
            }
            return msg;
        }
    };

    // ─── Component Identification (PGN 0x0FEEB) ──────────────────────────────────
    // Variable-length, asterisk-delimited string: Make*Model*SerialNumber*UnitNumber*
    struct ComponentIdentification {
        dp::String make;
        dp::String model;
        dp::String serial_number;
        dp::String unit_number;

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data;
            for (char c : make)
                data.push_back(static_cast<u8>(c));
            data.push_back('*');
            for (char c : model)
                data.push_back(static_cast<u8>(c));
            data.push_back('*');
            for (char c : serial_number)
                data.push_back(static_cast<u8>(c));
            data.push_back('*');
            for (char c : unit_number)
                data.push_back(static_cast<u8>(c));
            data.push_back('*');
            return data;
        }

        static ComponentIdentification decode(const dp::Vector<u8> &data) {
            ComponentIdentification id;
            usize field = 0;
            for (u8 b : data) {
                if (b == '*') {
                    ++field;
                    continue;
                }
                char c = static_cast<char>(b);
                switch (field) {
                case 0:
                    id.make += c;
                    break;
                case 1:
                    id.model += c;
                    break;
                case 2:
                    id.serial_number += c;
                    break;
                case 3:
                    id.unit_number += c;
                    break;
                default:
                    break;
                }
            }
            return id;
        }
    };

    // ─── Vehicle Identification (PGN 0x0FEEC) ────────────────────────────────────
    // Variable-length VIN string, asterisk-terminated
    struct VehicleIdentification {
        dp::String vin;

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data;
            for (char c : vin)
                data.push_back(static_cast<u8>(c));
            data.push_back('*');
            return data;
        }

        static VehicleIdentification decode(const dp::Vector<u8> &data) {
            VehicleIdentification id;
            for (u8 b : data) {
                if (b == '*' || b == 0xFF)
                    break;
                id.vin += static_cast<char>(b);
            }
            return id;
        }
    };

    // ─── J1939 Engine Interface ──────────────────────────────────────────────────
    // Handles reception and transmission of J1939 engine parameter messages.
    class EngineInterface {
        IsoNet &net_;
        InternalCF *cf_;

      public:
        EngineInterface(IsoNet &net, InternalCF *cf) : net_(net), cf_(cf) {}

        Result<void> initialize() {
            if (!cf_) {
                return Result<void>::err(Error::invalid_state("control function not set"));
            }
            net_.register_pgn_callback(
                PGN_EEC1, [this](const Message &msg) { on_eec1.emit(EEC1::decode(msg.data), msg.source); });
            net_.register_pgn_callback(
                PGN_EEC2, [this](const Message &msg) { on_eec2.emit(EEC2::decode(msg.data), msg.source); });
            net_.register_pgn_callback(PGN_ET1, [this](const Message &msg) {
                on_engine_temp.emit(EngineTemp1::decode(msg.data), msg.source);
            });
            net_.register_pgn_callback(PGN_ET2, [this](const Message &msg) {
                on_engine_temp2.emit(EngineTemp2::decode(msg.data), msg.source);
            });
            net_.register_pgn_callback(PGN_EFLP, [this](const Message &msg) {
                on_engine_fluid.emit(EngineFluidLP::decode(msg.data), msg.source);
            });
            net_.register_pgn_callback(PGN_ENGINE_HOURS, [this](const Message &msg) {
                on_engine_hours.emit(EngineHours::decode(msg.data), msg.source);
            });
            net_.register_pgn_callback(PGN_FUEL_ECONOMY, [this](const Message &msg) {
                on_fuel_economy.emit(FuelEconomy::decode(msg.data), msg.source);
            });
            net_.register_pgn_callback(
                PGN_EEC3, [this](const Message &msg) { on_eec3.emit(EEC3::decode(msg.data), msg.source); });
            net_.register_pgn_callback(
                PGN_TSC1, [this](const Message &msg) { on_tsc1.emit(TSC1::decode(msg.data), msg.source); });
            net_.register_pgn_callback(
                PGN_VEP1, [this](const Message &msg) { on_vep1.emit(VEP1::decode(msg.data), msg.source); });
            net_.register_pgn_callback(PGN_AMBIENT_CONDITIONS, [this](const Message &msg) {
                on_ambient.emit(AmbientConditions::decode(msg.data), msg.source);
            });
            net_.register_pgn_callback(PGN_DASH_DISPLAY, [this](const Message &msg) {
                on_dash_display.emit(DashDisplay::decode(msg.data), msg.source);
            });
            net_.register_pgn_callback(PGN_VEHICLE_POSITION, [this](const Message &msg) {
                on_vehicle_position.emit(VehiclePosition::decode(msg.data), msg.source);
            });
            net_.register_pgn_callback(PGN_FUEL_CONSUMPTION, [this](const Message &msg) {
                on_fuel_consumption.emit(FuelConsumption::decode(msg.data), msg.source);
            });
            net_.register_pgn_callback(PGN_COMPONENT_ID, [this](const Message &msg) {
                on_component_id.emit(ComponentIdentification::decode(msg.data), msg.source);
            });
            net_.register_pgn_callback(PGN_VEHICLE_ID, [this](const Message &msg) {
                on_vehicle_id.emit(VehicleIdentification::decode(msg.data), msg.source);
            });
            net_.register_pgn_callback(PGN_AT1, [this](const Message &msg) {
                on_aftertreatment1.emit(Aftertreatment1::decode(msg.data), msg.source);
            });
            net_.register_pgn_callback(PGN_AT2, [this](const Message &msg) {
                on_aftertreatment2.emit(Aftertreatment2::decode(msg.data), msg.source);
            });
            echo::category("isobus.j1939.engine").debug("initialized");
            return {};
        }

        // Send methods
        Result<void> send_eec1(const EEC1 &msg) {
            return net_.send(PGN_EEC1, msg.encode(), cf_, nullptr, Priority::High);
        }
        Result<void> send_eec2(const EEC2 &msg) {
            return net_.send(PGN_EEC2, msg.encode(), cf_, nullptr, Priority::Default);
        }
        Result<void> send_engine_temp(const EngineTemp1 &msg) {
            return net_.send(PGN_ET1, msg.encode(), cf_, nullptr, Priority::Default);
        }
        Result<void> send_engine_temp2(const EngineTemp2 &msg) {
            return net_.send(PGN_ET2, msg.encode(), cf_, nullptr, Priority::Default);
        }
        Result<void> send_engine_fluid(const EngineFluidLP &msg) {
            return net_.send(PGN_EFLP, msg.encode(), cf_, nullptr, Priority::Default);
        }
        Result<void> send_engine_hours(const EngineHours &msg) {
            return net_.send(PGN_ENGINE_HOURS, msg.encode(), cf_, nullptr, Priority::Default);
        }
        Result<void> send_fuel_economy(const FuelEconomy &msg) {
            return net_.send(PGN_FUEL_ECONOMY, msg.encode(), cf_, nullptr, Priority::Default);
        }
        Result<void> send_eec3(const EEC3 &msg) {
            return net_.send(PGN_EEC3, msg.encode(), cf_, nullptr, Priority::Default);
        }
        Result<void> send_tsc1(const TSC1 &msg, ControlFunction *dest = nullptr) {
            return net_.send(PGN_TSC1, msg.encode(), cf_, dest, Priority::High);
        }
        Result<void> send_vep1(const VEP1 &msg) {
            return net_.send(PGN_VEP1, msg.encode(), cf_, nullptr, Priority::Default);
        }
        Result<void> send_ambient(const AmbientConditions &msg) {
            return net_.send(PGN_AMBIENT_CONDITIONS, msg.encode(), cf_, nullptr, Priority::Default);
        }
        Result<void> send_dash_display(const DashDisplay &msg) {
            return net_.send(PGN_DASH_DISPLAY, msg.encode(), cf_, nullptr, Priority::Default);
        }
        Result<void> send_vehicle_position(const VehiclePosition &msg) {
            return net_.send(PGN_VEHICLE_POSITION, msg.encode(), cf_, nullptr, Priority::Default);
        }
        Result<void> send_fuel_consumption(const FuelConsumption &msg) {
            return net_.send(PGN_FUEL_CONSUMPTION, msg.encode(), cf_, nullptr, Priority::Default);
        }
        Result<void> send_component_id(const ComponentIdentification &msg) {
            return net_.send(PGN_COMPONENT_ID, msg.encode(), cf_);
        }
        Result<void> send_vehicle_id(const VehicleIdentification &msg) {
            return net_.send(PGN_VEHICLE_ID, msg.encode(), cf_);
        }
        Result<void> send_aftertreatment1(const Aftertreatment1 &msg) {
            return net_.send(PGN_AT1, msg.encode(), cf_, nullptr, Priority::Default);
        }
        Result<void> send_aftertreatment2(const Aftertreatment2 &msg) {
            return net_.send(PGN_AT2, msg.encode(), cf_, nullptr, Priority::Default);
        }

        // Events
        Event<EEC1, Address> on_eec1;
        Event<EEC2, Address> on_eec2;
        Event<EEC3, Address> on_eec3;
        Event<TSC1, Address> on_tsc1;
        Event<VEP1, Address> on_vep1;
        Event<AmbientConditions, Address> on_ambient;
        Event<DashDisplay, Address> on_dash_display;
        Event<VehiclePosition, Address> on_vehicle_position;
        Event<EngineTemp1, Address> on_engine_temp;
        Event<EngineTemp2, Address> on_engine_temp2;
        Event<EngineFluidLP, Address> on_engine_fluid;
        Event<EngineHours, Address> on_engine_hours;
        Event<FuelEconomy, Address> on_fuel_economy;
        Event<FuelConsumption, Address> on_fuel_consumption;
        Event<Aftertreatment1, Address> on_aftertreatment1;
        Event<Aftertreatment2, Address> on_aftertreatment2;
        Event<ComponentIdentification, Address> on_component_id;
        Event<VehicleIdentification, Address> on_vehicle_id;
    };

} // namespace agrobus::j1939
