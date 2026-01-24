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

    // ─── Electronic Transmission Controller 1 (ETC1, PGN 0x0F005) ────────────
    // Broadcast at 10-100ms by transmission controller
    struct ETC1 {
        i8 current_gear = -125;            // SPN 523: offset -125 (-125 to 125)
        i8 selected_gear = -125;           // SPN 524: offset -125
        f64 output_shaft_speed_rpm = 0.0;  // SPN 191: 0.125 rpm/bit, 2 bytes
        u8 shift_in_progress = 0x03;       // SPN 574: 2 bits
        u8 torque_converter_lockup = 0x03; // SPN 573: 2 bits

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = (shift_in_progress & 0x03) | ((torque_converter_lockup & 0x03) << 2);
            u16 speed = static_cast<u16>(output_shaft_speed_rpm / 0.125);
            data[1] = static_cast<u8>(speed & 0xFF);
            data[2] = static_cast<u8>((speed >> 8) & 0xFF);
            data[3] = static_cast<u8>(current_gear + 125);
            data[4] = static_cast<u8>(selected_gear + 125);
            return data;
        }

        static ETC1 decode(const dp::Vector<u8> &data) {
            ETC1 msg;
            if (data.size() >= 5) {
                msg.shift_in_progress = data[0] & 0x03;
                msg.torque_converter_lockup = (data[0] >> 2) & 0x03;
                u16 speed = static_cast<u16>(data[1]) | (static_cast<u16>(data[2]) << 8);
                msg.output_shaft_speed_rpm = speed * 0.125;
                msg.current_gear = static_cast<i8>(data[3]) - 125;
                msg.selected_gear = static_cast<i8>(data[4]) - 125;
            }
            return msg;
        }
    };

    // ─── Transmission Oil Temperature (PGN included in ET2, 0x0FEED) ─────────
    struct TransmissionOilTemp {
        f64 oil_temp_c = -40.0; // SPN 177: 0.03125 C/bit, offset -273, 2 bytes

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            u16 temp = static_cast<u16>((oil_temp_c + 273.0) / 0.03125);
            data[0] = static_cast<u8>(temp & 0xFF);
            data[1] = static_cast<u8>((temp >> 8) & 0xFF);
            return data;
        }

        static TransmissionOilTemp decode(const dp::Vector<u8> &data) {
            TransmissionOilTemp msg;
            if (data.size() >= 2) {
                u16 temp = static_cast<u16>(data[0]) | (static_cast<u16>(data[1]) << 8);
                msg.oil_temp_c = temp * 0.03125 - 273.0;
            }
            return msg;
        }
    };

    // ─── Cruise Control / Vehicle Speed (CCVS, PGN 0x0FEF1) ─────────────────
    struct CruiseControl {
        f64 wheel_speed_kmh = 0.0;  // SPN 84: 1/256 km/h per bit, 2 bytes
        u8 cc_active = 0x03;        // SPN 595: 2 bits (0=off, 1=on, 2=error, 3=N/A)
        u8 brake_switch = 0x03;     // SPN 597: 2 bits
        u8 clutch_switch = 0x03;    // SPN 598: 2 bits
        u8 park_brake = 0x03;       // SPN 70: 2 bits
        f64 cc_set_speed_kmh = 0.0; // SPN 86: 1/256 km/h per bit, 2 bytes

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            u16 speed = static_cast<u16>(wheel_speed_kmh * 256.0);
            data[0] = static_cast<u8>(speed & 0xFF);
            data[1] = static_cast<u8>((speed >> 8) & 0xFF);
            data[2] = (cc_active & 0x03) | ((brake_switch & 0x03) << 2) | ((clutch_switch & 0x03) << 4) |
                      ((park_brake & 0x03) << 6);
            u16 set_speed = static_cast<u16>(cc_set_speed_kmh * 256.0);
            data[3] = static_cast<u8>(set_speed & 0xFF);
            data[4] = static_cast<u8>((set_speed >> 8) & 0xFF);
            return data;
        }

        static CruiseControl decode(const dp::Vector<u8> &data) {
            CruiseControl msg;
            if (data.size() >= 5) {
                u16 speed = static_cast<u16>(data[0]) | (static_cast<u16>(data[1]) << 8);
                msg.wheel_speed_kmh = speed / 256.0;
                msg.cc_active = data[2] & 0x03;
                msg.brake_switch = (data[2] >> 2) & 0x03;
                msg.clutch_switch = (data[2] >> 4) & 0x03;
                msg.park_brake = (data[2] >> 6) & 0x03;
                u16 set_speed = static_cast<u16>(data[3]) | (static_cast<u16>(data[4]) << 8);
                msg.cc_set_speed_kmh = set_speed / 256.0;
            }
            return msg;
        }
    };

    // ─── J1939 Transmission Interface ──────────────────────────────────────────
    // Handles reception and transmission of J1939 transmission/drivetrain messages.
    class TransmissionInterface {
        IsoNet &net_;
        InternalCF *cf_;

      public:
        TransmissionInterface(IsoNet &net, InternalCF *cf) : net_(net), cf_(cf) {}

        Result<void> initialize() {
            if (!cf_) {
                return Result<void>::err(Error::invalid_state("control function not set"));
            }
            net_.register_pgn_callback(
                PGN_TRANSMISSION_1, [this](const Message &msg) { on_etc1.emit(ETC1::decode(msg.data), msg.source); });
            net_.register_pgn_callback(PGN_CRUISE_CONTROL, [this](const Message &msg) {
                on_cruise_control.emit(CruiseControl::decode(msg.data), msg.source);
            });
            echo::category("isobus.j1939.transmission").debug("initialized");
            return {};
        }

        // Send methods
        Result<void> send_etc1(const ETC1 &msg) {
            return net_.send(PGN_TRANSMISSION_1, msg.encode(), cf_, nullptr, Priority::High);
        }
        Result<void> send_cruise_control(const CruiseControl &msg) {
            return net_.send(PGN_CRUISE_CONTROL, msg.encode(), cf_, nullptr, Priority::Default);
        }

        // Events
        Event<ETC1, Address> on_etc1;
        Event<CruiseControl, Address> on_cruise_control;
    };

} // namespace agrobus::j1939
