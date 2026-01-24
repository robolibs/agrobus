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

namespace agrobus::isobus::implement {
    using namespace agrobus::net;

    // ─── Drive Strategy Mode (ISO 11783-7 Section 11) ──────────────────────────
    enum class DriveStrategyMode : u8 { NoAction = 0, MaxPower = 1, MaxEconomy = 2, MaxSpeed = 3, Reserved = 0xFF };

    // ─── Drive Strategy Command (PGN 0xFCCE) ──────────────────────────────────
    // Sent by implement to request a specific drive strategy from the tractor.
    // Allows coordination of engine/transmission behavior.
    struct DriveStrategyCmd {
        DriveStrategyMode mode = DriveStrategyMode::NoAction;
        u8 target_speed_limit_percent = 0xFF; // 0.4%/bit (0-100%), 0xFF=N/A
        u8 target_engine_load_percent = 0xFF; // 0.4%/bit (0-100%), 0xFF=N/A

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = static_cast<u8>(mode);
            data[1] = target_speed_limit_percent;
            data[2] = target_engine_load_percent;
            return data;
        }

        static DriveStrategyCmd decode(const dp::Vector<u8> &data) {
            DriveStrategyCmd msg;
            if (data.size() >= 3) {
                msg.mode = static_cast<DriveStrategyMode>(data[0]);
                msg.target_speed_limit_percent = data[1];
                msg.target_engine_load_percent = data[2];
            }
            return msg;
        }
    };

    // ─── Guidance System Command (PGN 0xAD00) ─────────────────────────────────
    // Sent by Class xG TECU or external guidance controller.
    // Direct steering control command for auto-guidance.
    struct GuidanceSystemCmd {
        f64 commanded_curvature = 0.0;      // 1/km, 0.25/km per bit offset -8032
        u8 steering_input_authority = 0xFF; // 0.4%/bit (0-100%), 0xFF=N/A
        u8 system_command = 0xFF;           // Implementation-specific

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            u16 curv_raw = static_cast<u16>((commanded_curvature + 8032.0) / 0.25);
            data[0] = static_cast<u8>(curv_raw & 0xFF);
            data[1] = static_cast<u8>((curv_raw >> 8) & 0xFF);
            data[2] = steering_input_authority;
            data[3] = system_command;
            return data;
        }

        static GuidanceSystemCmd decode(const dp::Vector<u8> &data) {
            GuidanceSystemCmd msg;
            if (data.size() >= 4) {
                u16 curv_raw = static_cast<u16>(data[0]) | (static_cast<u16>(data[1]) << 8);
                msg.commanded_curvature = curv_raw * 0.25 - 8032.0;
                msg.steering_input_authority = data[2];
                msg.system_command = data[3];
            }
            return msg;
        }
    };

    // ─── Hitch and PTO Combined Command (PGN 0xFE42) ──────────────────────────
    // Coordinated hitch position and PTO engagement in a single message.
    struct HitchPTOCombinedCmd {
        u16 hitch_position = 0xFFFF; // 0.0025%/bit (0-100%)
        u16 pto_speed_raw = 0xFFFF;  // 0.125 rpm/bit
        u8 hitch_cmd = 0x03;         // 2 bits: 0=no action, 1=lower, 2=raise, 3=N/A
        u8 pto_cmd = 0x03;           // 2 bits: 0=no action, 1=engage, 2=disengage, 3=N/A

        f64 pto_speed_rpm() const { return pto_speed_raw == 0xFFFF ? 0.0 : pto_speed_raw * 0.125; }

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = static_cast<u8>(hitch_position & 0xFF);
            data[1] = static_cast<u8>((hitch_position >> 8) & 0xFF);
            data[2] = static_cast<u8>(pto_speed_raw & 0xFF);
            data[3] = static_cast<u8>((pto_speed_raw >> 8) & 0xFF);
            data[4] = (hitch_cmd & 0x03) | ((pto_cmd & 0x03) << 2);
            return data;
        }

        static HitchPTOCombinedCmd decode(const dp::Vector<u8> &data) {
            HitchPTOCombinedCmd msg;
            if (data.size() >= 5) {
                msg.hitch_position = static_cast<u16>(data[0]) | (static_cast<u16>(data[1]) << 8);
                msg.pto_speed_raw = static_cast<u16>(data[2]) | (static_cast<u16>(data[3]) << 8);
                msg.hitch_cmd = data[4] & 0x03;
                msg.pto_cmd = (data[4] >> 2) & 0x03;
            }
            return msg;
        }
    };

    // ─── Hitch Roll/Pitch Command (PGN 0xF100/0xF102) ─────────────────────────
    // Commands for independent roll and pitch control of hitch.
    struct HitchRollPitchCmd {
        u16 roll_position = 0xFFFF;  // 0.0025%/bit, offset 50% (center)
        u16 pitch_position = 0xFFFF; // 0.0025%/bit, offset 50% (center)
        bool is_front = false;       // true = front (0xF100), false = rear (0xF102)

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = static_cast<u8>(roll_position & 0xFF);
            data[1] = static_cast<u8>((roll_position >> 8) & 0xFF);
            data[2] = static_cast<u8>(pitch_position & 0xFF);
            data[3] = static_cast<u8>((pitch_position >> 8) & 0xFF);
            return data;
        }

        static HitchRollPitchCmd decode(const dp::Vector<u8> &data) {
            HitchRollPitchCmd msg;
            if (data.size() >= 4) {
                msg.roll_position = static_cast<u16>(data[0]) | (static_cast<u16>(data[1]) << 8);
                msg.pitch_position = static_cast<u16>(data[2]) | (static_cast<u16>(data[3]) << 8);
            }
            return msg;
        }
    };

    // ─── Drive Strategy Interface ──────────────────────────────────────────────
    class DriveStrategyInterface {
        IsoNet &net_;
        InternalCF *cf_;

      public:
        DriveStrategyInterface(IsoNet &net, InternalCF *cf) : net_(net), cf_(cf) {}

        Result<void> initialize() {
            if (!cf_) {
                return Result<void>::err(Error::invalid_state("control function not set"));
            }
            net_.register_pgn_callback(PGN_DRIVE_STRATEGY_CMD, [this](const Message &msg) {
                on_drive_strategy.emit(DriveStrategyCmd::decode(msg.data), msg.source);
            });
            net_.register_pgn_callback(PGN_GUIDANCE_SYSTEM_CMD, [this](const Message &msg) {
                on_guidance_system_cmd.emit(GuidanceSystemCmd::decode(msg.data), msg.source);
            });
            net_.register_pgn_callback(PGN_HITCH_PTO_COMBINED_CMD, [this](const Message &msg) {
                on_hitch_pto_combined.emit(HitchPTOCombinedCmd::decode(msg.data), msg.source);
            });
            net_.register_pgn_callback(PGN_FRONT_HITCH_ROLL_PITCH_CMD, [this](const Message &msg) {
                auto cmd = HitchRollPitchCmd::decode(msg.data);
                cmd.is_front = true;
                on_hitch_roll_pitch.emit(cmd, msg.source);
            });
            net_.register_pgn_callback(PGN_REAR_HITCH_ROLL_PITCH_CMD, [this](const Message &msg) {
                auto cmd = HitchRollPitchCmd::decode(msg.data);
                cmd.is_front = false;
                on_hitch_roll_pitch.emit(cmd, msg.source);
            });
            echo::category("isobus.implement.drive_strategy").debug("initialized");
            return {};
        }

        Result<void> send_drive_strategy(const DriveStrategyCmd &cmd) {
            return net_.send(PGN_DRIVE_STRATEGY_CMD, cmd.encode(), cf_, nullptr, Priority::Default);
        }

        Result<void> send_guidance_system_cmd(const GuidanceSystemCmd &cmd) {
            return net_.send(PGN_GUIDANCE_SYSTEM_CMD, cmd.encode(), cf_, nullptr, Priority::Default);
        }

        Result<void> send_hitch_pto_combined(const HitchPTOCombinedCmd &cmd) {
            return net_.send(PGN_HITCH_PTO_COMBINED_CMD, cmd.encode(), cf_, nullptr, Priority::Default);
        }

        Result<void> send_hitch_roll_pitch(const HitchRollPitchCmd &cmd) {
            PGN pgn = cmd.is_front ? PGN_FRONT_HITCH_ROLL_PITCH_CMD : PGN_REAR_HITCH_ROLL_PITCH_CMD;
            return net_.send(pgn, cmd.encode(), cf_, nullptr, Priority::Default);
        }

        // Events
        Event<DriveStrategyCmd, Address> on_drive_strategy;
        Event<GuidanceSystemCmd, Address> on_guidance_system_cmd;
        Event<HitchPTOCombinedCmd, Address> on_hitch_pto_combined;
        Event<HitchRollPitchCmd, Address> on_hitch_roll_pitch;
    };

} // namespace agrobus::isobus::implement
