#pragma once

#include "machine_speed_cmd.hpp"
#include <agrobus/net/constants.hpp>
#include <agrobus/net/error.hpp>
#include <agrobus/net/event.hpp>
#include <agrobus/net/internal_cf.hpp>
#include <agrobus/net/message.hpp>
#include <agrobus/net/network_manager.hpp>
#include <agrobus/net/types.hpp>
#include <datapod/datapod.hpp>
#include <echo/echo.hpp>

namespace agrobus::isobus::implement {
    using namespace agrobus::net;

    // MachineDirection and SpeedSource enums defined in machine_speed_cmd.hpp

    // ─── Wheel-Based Speed and Distance (PGN 0xFE48) ────────────────────────────
    // ISO 11783-7: Broadcast by TECU at 100ms (Class 1, 2, 3)
    struct WheelBasedSpeedDist {
        f64 speed_mps = 0.0;  // 0.001 m/s per bit, 2 bytes
        f64 distance_m = 0.0; // 0.001 m per bit, 4 bytes (total accumulated)
        MachineDirection direction = MachineDirection::NotAvailable;
        u8 max_power_time_min = 0xFF; // Max time of tractor power, minutes (0xFF=N/A)
        u8 start_stop_state = 0x03;   // 2 bits: 0=key off, 1=not requested, 2=error, 3=N/A

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            u16 spd = static_cast<u16>(speed_mps / 0.001);
            data[0] = static_cast<u8>(spd & 0xFF);
            data[1] = static_cast<u8>((spd >> 8) & 0xFF);
            u32 dist = static_cast<u32>(distance_m / 0.001);
            data[2] = static_cast<u8>(dist & 0xFF);
            data[3] = static_cast<u8>((dist >> 8) & 0xFF);
            data[4] = static_cast<u8>((dist >> 16) & 0xFF);
            data[5] = static_cast<u8>((dist >> 24) & 0xFF);
            data[6] = max_power_time_min;
            data[7] = (static_cast<u8>(direction) & 0x03) | ((start_stop_state & 0x03) << 2);
            return data;
        }

        static WheelBasedSpeedDist decode(const dp::Vector<u8> &data) {
            WheelBasedSpeedDist msg;
            if (data.size() >= 8) {
                u16 spd = static_cast<u16>(data[0]) | (static_cast<u16>(data[1]) << 8);
                msg.speed_mps = spd * 0.001;
                u32 dist = static_cast<u32>(data[2]) | (static_cast<u32>(data[3]) << 8) |
                           (static_cast<u32>(data[4]) << 16) | (static_cast<u32>(data[5]) << 24);
                msg.distance_m = dist * 0.001;
                msg.max_power_time_min = data[6];
                msg.direction = static_cast<MachineDirection>(data[7] & 0x03);
                msg.start_stop_state = (data[7] >> 2) & 0x03;
            }
            return msg;
        }
    };

    // ─── Ground-Based Speed and Distance (PGN 0xFE49) ───────────────────────────
    // ISO 11783-7: Broadcast by TECU at 100ms (Class 2, 3)
    struct GroundBasedSpeedDist {
        f64 speed_mps = 0.0;  // 0.001 m/s per bit, 2 bytes
        f64 distance_m = 0.0; // 0.001 m per bit, 4 bytes (total accumulated)
        MachineDirection direction = MachineDirection::NotAvailable;

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            u16 spd = static_cast<u16>(speed_mps / 0.001);
            data[0] = static_cast<u8>(spd & 0xFF);
            data[1] = static_cast<u8>((spd >> 8) & 0xFF);
            u32 dist = static_cast<u32>(distance_m / 0.001);
            data[2] = static_cast<u8>(dist & 0xFF);
            data[3] = static_cast<u8>((dist >> 8) & 0xFF);
            data[4] = static_cast<u8>((dist >> 16) & 0xFF);
            data[5] = static_cast<u8>((dist >> 24) & 0xFF);
            data[6] = 0xFF; // reserved
            data[7] = static_cast<u8>(direction) & 0x03;
            return data;
        }

        static GroundBasedSpeedDist decode(const dp::Vector<u8> &data) {
            GroundBasedSpeedDist msg;
            if (data.size() >= 8) {
                u16 spd = static_cast<u16>(data[0]) | (static_cast<u16>(data[1]) << 8);
                msg.speed_mps = spd * 0.001;
                u32 dist = static_cast<u32>(data[2]) | (static_cast<u32>(data[3]) << 8) |
                           (static_cast<u32>(data[4]) << 16) | (static_cast<u32>(data[5]) << 24);
                msg.distance_m = dist * 0.001;
                msg.direction = static_cast<MachineDirection>(data[7] & 0x03);
            }
            return msg;
        }
    };

    // ─── Machine Selected Speed (PGN 0xF022) ────────────────────────────────────
    // ISO 11783-7: Broadcast by TECU at 100ms
    struct MachineSelectedSpeed {
        f64 speed_mps = 0.0;  // 0.001 m/s per bit, 2 bytes
        f64 distance_m = 0.0; // 0.001 m per bit, 4 bytes
        MachineDirection direction = MachineDirection::NotAvailable;
        SpeedSource source = SpeedSource::WheelBased;
        u8 limit_status = 0x07; // 3 bits: 0=not limited, ...7=N/A
        u8 exit_code = 0xFF;

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            u16 spd = static_cast<u16>(speed_mps / 0.001);
            data[0] = static_cast<u8>(spd & 0xFF);
            data[1] = static_cast<u8>((spd >> 8) & 0xFF);
            u32 dist = static_cast<u32>(distance_m / 0.001);
            data[2] = static_cast<u8>(dist & 0xFF);
            data[3] = static_cast<u8>((dist >> 8) & 0xFF);
            data[4] = static_cast<u8>((dist >> 16) & 0xFF);
            data[5] = static_cast<u8>((dist >> 24) & 0xFF);
            data[6] = exit_code;
            data[7] = (static_cast<u8>(direction) & 0x03) | ((static_cast<u8>(source) & 0x03) << 2) |
                      ((limit_status & 0x07) << 4);
            return data;
        }

        static MachineSelectedSpeed decode(const dp::Vector<u8> &data) {
            MachineSelectedSpeed msg;
            if (data.size() >= 8) {
                u16 spd = static_cast<u16>(data[0]) | (static_cast<u16>(data[1]) << 8);
                msg.speed_mps = spd * 0.001;
                u32 dist = static_cast<u32>(data[2]) | (static_cast<u32>(data[3]) << 8) |
                           (static_cast<u32>(data[4]) << 16) | (static_cast<u32>(data[5]) << 24);
                msg.distance_m = dist * 0.001;
                msg.exit_code = data[6];
                msg.direction = static_cast<MachineDirection>(data[7] & 0x03);
                msg.source = static_cast<SpeedSource>((data[7] >> 2) & 0x03);
                msg.limit_status = (data[7] >> 4) & 0x07;
            }
            return msg;
        }
    };

    // ─── Hitch / PTO Status Feedback (ISO 11783-7 Class 3) ──────────────────────

    enum class LimitStatus : u8 { NotLimited = 0, OperatorLimited = 1, SystemLimited = 2, NotAvailable = 3 };
    enum class ExitReasonCode : u8 { NoExit = 0, OperatorCmd = 1, SystemCmd = 2, Fault = 3, NotAvailable = 7 };

    struct HitchStatus {
        u8 position_percent = 0xFF; // 0-100 (0.4% resolution per bit)
        u8 in_work_indication = 3;  // 2 bits: 0=not in work, 1=in work, 2=error, 3=N/A
        LimitStatus limit_status = LimitStatus::NotAvailable;
        ExitReasonCode exit_code = ExitReasonCode::NotAvailable;
        f64 draft_force_n = 0.0; // Draft force (Class 2 TECU, 10 N/bit, offset -320000)
        bool is_rear = true;

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = position_percent;
            data[1] = (in_work_indication & 0x03) | ((static_cast<u8>(limit_status) & 0x03) << 2) |
                      ((static_cast<u8>(exit_code) & 0x07) << 4);
            // Bytes 2-3: nominal lower link force (rear draft), 10 N/bit, offset -320000
            i32 force_raw = static_cast<i32>((draft_force_n + 320000.0) / 10.0);
            data[2] = static_cast<u8>(force_raw & 0xFF);
            data[3] = static_cast<u8>((force_raw >> 8) & 0xFF);
            return data;
        }

        static HitchStatus decode(const dp::Vector<u8> &data) {
            HitchStatus msg;
            if (data.size() >= 4) {
                msg.position_percent = data[0];
                msg.in_work_indication = data[1] & 0x03;
                msg.limit_status = static_cast<LimitStatus>((data[1] >> 2) & 0x03);
                msg.exit_code = static_cast<ExitReasonCode>((data[1] >> 4) & 0x07);
                i32 force_raw = static_cast<i32>(data[2]) | (static_cast<i32>(data[3]) << 8);
                msg.draft_force_n = force_raw * 10.0 - 320000.0;
            }
            return msg;
        }
    };

    struct PTOStatus {
        f64 shaft_speed_rpm = 0.0; // 0.125 rpm/bit, 2 bytes
        u8 engagement = 3;         // 2 bits: 0=disengaged, 1=engaged, 2=error, 3=N/A
        LimitStatus limit_status = LimitStatus::NotAvailable;
        ExitReasonCode exit_code = ExitReasonCode::NotAvailable;
        u8 economy_mode = 3; // 2 bits: 0=not active, 1=active, 2=error, 3=N/A
        bool is_rear = true;

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            u16 rpm = static_cast<u16>(shaft_speed_rpm / 0.125);
            data[0] = static_cast<u8>(rpm & 0xFF);
            data[1] = static_cast<u8>((rpm >> 8) & 0xFF);
            data[2] = (engagement & 0x03) | ((economy_mode & 0x03) << 2) |
                      ((static_cast<u8>(limit_status) & 0x03) << 4) | ((static_cast<u8>(exit_code) & 0x03) << 6);
            return data;
        }

        static PTOStatus decode(const dp::Vector<u8> &data) {
            PTOStatus msg;
            if (data.size() >= 3) {
                u16 rpm = static_cast<u16>(data[0]) | (static_cast<u16>(data[1]) << 8);
                msg.shaft_speed_rpm = rpm * 0.125;
                msg.engagement = data[2] & 0x03;
                msg.economy_mode = (data[2] >> 2) & 0x03;
                msg.limit_status = static_cast<LimitStatus>((data[2] >> 4) & 0x03);
                msg.exit_code = static_cast<ExitReasonCode>((data[2] >> 6) & 0x03);
            }
            return msg;
        }
    };

    // ─── Speed/Distance Interface ────────────────────────────────────────────────
    // Handles reception and transmission of speed/distance/direction messages.

    class TECUSpeedDistance {
        IsoNet &net_;
        InternalCF *cf_;

      public:
        TECUSpeedDistance(IsoNet &net, InternalCF *cf) : net_(net), cf_(cf) {}

        Result<void> initialize() {
            if (!cf_) {
                return Result<void>::err(Error::invalid_state("control function not set"));
            }
            net_.register_pgn_callback(PGN_WHEEL_BASED_SPEED_DIST,
                                       [this](const Message &msg) { handle_wheel_speed(msg); });
            net_.register_pgn_callback(PGN_GROUND_BASED_SPEED_DIST,
                                       [this](const Message &msg) { handle_ground_speed(msg); });
            net_.register_pgn_callback(PGN_MACHINE_SELECTED_SPEED,
                                       [this](const Message &msg) { handle_machine_speed(msg); });
            net_.register_pgn_callback(PGN_REAR_HITCH, [this](const Message &msg) {
                auto s = HitchStatus::decode(msg.data);
                s.is_rear = true;
                on_hitch_status.emit(s, msg.source);
            });
            net_.register_pgn_callback(PGN_FRONT_HITCH, [this](const Message &msg) {
                auto s = HitchStatus::decode(msg.data);
                s.is_rear = false;
                on_hitch_status.emit(s, msg.source);
            });
            net_.register_pgn_callback(PGN_REAR_PTO, [this](const Message &msg) {
                auto s = PTOStatus::decode(msg.data);
                s.is_rear = true;
                on_pto_status.emit(s, msg.source);
            });
            net_.register_pgn_callback(PGN_FRONT_PTO, [this](const Message &msg) {
                auto s = PTOStatus::decode(msg.data);
                s.is_rear = false;
                on_pto_status.emit(s, msg.source);
            });
            echo::category("isobus.implement.speed").debug("initialized");
            return {};
        }

        // Send wheel-based speed and distance
        Result<void> send_wheel_speed(const WheelBasedSpeedDist &msg) {
            return net_.send(PGN_WHEEL_BASED_SPEED_DIST, msg.encode(), cf_, nullptr, Priority::Default);
        }

        // Send ground-based speed and distance
        Result<void> send_ground_speed(const GroundBasedSpeedDist &msg) {
            return net_.send(PGN_GROUND_BASED_SPEED_DIST, msg.encode(), cf_, nullptr, Priority::Default);
        }

        // Send machine selected speed
        Result<void> send_machine_speed(const MachineSelectedSpeed &msg) {
            return net_.send(PGN_MACHINE_SELECTED_SPEED, msg.encode(), cf_, nullptr, Priority::Default);
        }

        // Send hitch/PTO status
        Result<void> send_hitch_status(const HitchStatus &msg) {
            PGN pgn = msg.is_rear ? PGN_REAR_HITCH : PGN_FRONT_HITCH;
            return net_.send(pgn, msg.encode(), cf_, nullptr, Priority::Default);
        }
        Result<void> send_pto_status(const PTOStatus &msg) {
            PGN pgn = msg.is_rear ? PGN_REAR_PTO : PGN_FRONT_PTO;
            return net_.send(pgn, msg.encode(), cf_, nullptr, Priority::Default);
        }

        // Events
        Event<WheelBasedSpeedDist, Address> on_wheel_speed;
        Event<GroundBasedSpeedDist, Address> on_ground_speed;
        Event<MachineSelectedSpeed, Address> on_machine_speed;
        Event<HitchStatus, Address> on_hitch_status;
        Event<PTOStatus, Address> on_pto_status;

      private:
        void handle_wheel_speed(const Message &msg) {
            auto data = WheelBasedSpeedDist::decode(msg.data);
            on_wheel_speed.emit(data, msg.source);
        }

        void handle_ground_speed(const Message &msg) {
            auto data = GroundBasedSpeedDist::decode(msg.data);
            on_ground_speed.emit(data, msg.source);
        }

        void handle_machine_speed(const Message &msg) {
            auto data = MachineSelectedSpeed::decode(msg.data);
            on_machine_speed.emit(data, msg.source);
        }
    };

} // namespace agrobus::isobus::implement
