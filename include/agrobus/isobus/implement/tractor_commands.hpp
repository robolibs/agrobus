#pragma once

#include <agrobus/net/constants.hpp>
#include <agrobus/net/error.hpp>
#include <agrobus/net/event.hpp>
#include <agrobus/net/internal_cf.hpp>
#include <agrobus/net/message.hpp>
#include <agrobus/net/network_manager.hpp>
#include <datapod/datapod.hpp>
#include <echo/echo.hpp>

namespace agrobus::isobus::implement {
    using namespace agrobus::net;

    // ─── Command enumerations ────────────────────────────────────────────────────
    enum class HitchCommand : u8 { NoAction = 0, Lower = 1, Raise = 2, Position = 3 };

    enum class PTOCommand : u8 { NoAction = 0, Engage = 1, Disengage = 2, SetSpeed = 3 };

    enum class ValveCommand : u8 { NoAction = 0, Extend = 1, Retract = 2, Float = 3, Block = 4 };

    // ─── Hitch Command Message (ISO 11783-7) ────────────────────────────────────
    struct HitchCommandMsg {
        HitchCommand command = HitchCommand::NoAction;
        u16 target_position = 0xFFFF; // 0-100% scaled to 0-40000
        u8 rate = 0xFF;               // Rate of change

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            // Byte 0-1: Target position (0.0025% per bit, 0-100%)
            data[0] = static_cast<u8>(target_position & 0xFF);
            data[1] = static_cast<u8>((target_position >> 8) & 0xFF);
            // Byte 2: Reserved
            data[2] = 0xFF;
            // Byte 3: Rate of change
            data[3] = rate;
            // Byte 4: Command (bits 0-1)
            data[4] = static_cast<u8>(command);
            // Bytes 5-7: Reserved
            data[5] = 0xFF;
            data[6] = 0xFF;
            data[7] = 0xFF;
            return data;
        }

        static HitchCommandMsg decode(const dp::Vector<u8> &data) {
            HitchCommandMsg msg;
            if (data.size() >= 5) {
                msg.target_position = static_cast<u16>(data[0]) | (static_cast<u16>(data[1]) << 8);
                msg.rate = data[3];
                msg.command = static_cast<HitchCommand>(data[4] & 0x03);
            }
            return msg;
        }
    };

    // ─── PTO Command Message (ISO 11783-7) ──────────────────────────────────────
    struct PTOCommandMsg {
        PTOCommand command = PTOCommand::NoAction;
        u16 target_speed_rpm = 0xFFFF; // 0.125 rpm per bit
        u8 ramp_rate = 0xFF;           // Rate of change

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            // Byte 0-1: Target speed (0.125 rpm/bit)
            data[0] = static_cast<u8>(target_speed_rpm & 0xFF);
            data[1] = static_cast<u8>((target_speed_rpm >> 8) & 0xFF);
            // Byte 2: Reserved
            data[2] = 0xFF;
            // Byte 3: Ramp rate
            data[3] = ramp_rate;
            // Byte 4: Command (bits 0-1)
            data[4] = static_cast<u8>(command);
            // Bytes 5-7: Reserved
            data[5] = 0xFF;
            data[6] = 0xFF;
            data[7] = 0xFF;
            return data;
        }

        static PTOCommandMsg decode(const dp::Vector<u8> &data) {
            PTOCommandMsg msg;
            if (data.size() >= 5) {
                msg.target_speed_rpm = static_cast<u16>(data[0]) | (static_cast<u16>(data[1]) << 8);
                msg.ramp_rate = data[3];
                msg.command = static_cast<PTOCommand>(data[4] & 0x03);
            }
            return msg;
        }
    };

    // ─── Auxiliary Valve Command Message (ISO 11783-7) ───────────────────────────
    struct AuxValveCommandMsg {
        u8 valve_index = 0;
        ValveCommand command = ValveCommand::NoAction;
        u16 flow_rate = 0xFFFF; // % of max (0.4% per bit, 0-100%)

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            // Byte 0: Valve index
            data[0] = valve_index;
            // Byte 1-2: Flow rate (0.4% per bit)
            data[1] = static_cast<u8>(flow_rate & 0xFF);
            data[2] = static_cast<u8>((flow_rate >> 8) & 0xFF);
            // Byte 3: Command (bits 0-2)
            data[3] = static_cast<u8>(command);
            // Bytes 4-7: Reserved
            data[4] = 0xFF;
            data[5] = 0xFF;
            data[6] = 0xFF;
            data[7] = 0xFF;
            return data;
        }

        static AuxValveCommandMsg decode(const dp::Vector<u8> &data) {
            AuxValveCommandMsg msg;
            if (data.size() >= 4) {
                msg.valve_index = data[0];
                msg.flow_rate = static_cast<u16>(data[1]) | (static_cast<u16>(data[2]) << 8);
                msg.command = static_cast<ValveCommand>(data[3] & 0x07);
            }
            return msg;
        }
    };

    // ─── Tractor Control Mode (ISO 11783-7, PGN 0xFE4D) ─────────────────────────
    // Indicates the current operating mode of the tractor to connected implements.
    enum class TractorMode : u8 { Manual = 0, Automatic = 1, Reserved2 = 2, NotAvailable = 3 };

    struct TractorControlModeMsg {
        TractorMode hitch_mode = TractorMode::NotAvailable;       // 2 bits: rear hitch control mode
        TractorMode pto_mode = TractorMode::NotAvailable;         // 2 bits: rear PTO control mode
        TractorMode front_hitch_mode = TractorMode::NotAvailable; // 2 bits: front hitch control mode
        TractorMode front_pto_mode = TractorMode::NotAvailable;   // 2 bits: front PTO control mode
        u8 speed_control_state = 0xFF;                            // 0xFF = N/A, bit-encoded speed control modes

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            // Byte 0: bits 0-1: rear hitch, bits 2-3: rear PTO,
            //         bits 4-5: front hitch, bits 6-7: front PTO
            data[0] = (static_cast<u8>(hitch_mode) & 0x03) | ((static_cast<u8>(pto_mode) & 0x03) << 2) |
                      ((static_cast<u8>(front_hitch_mode) & 0x03) << 4) |
                      ((static_cast<u8>(front_pto_mode) & 0x03) << 6);
            data[1] = speed_control_state;
            return data;
        }

        static TractorControlModeMsg decode(const dp::Vector<u8> &data) {
            TractorControlModeMsg msg;
            if (data.size() >= 2) {
                msg.hitch_mode = static_cast<TractorMode>(data[0] & 0x03);
                msg.pto_mode = static_cast<TractorMode>((data[0] >> 2) & 0x03);
                msg.front_hitch_mode = static_cast<TractorMode>((data[0] >> 4) & 0x03);
                msg.front_pto_mode = static_cast<TractorMode>((data[0] >> 6) & 0x03);
                msg.speed_control_state = data[1];
            }
            return msg;
        }
    };

    // ─── Tractor Command Interface (ISO 11783-7) ────────────────────────────────
    // Provides send/receive interface for tractor implement commands:
    //   - Rear/Front hitch position control
    //   - Rear/Front PTO speed control
    //   - Auxiliary valve control (valves 0-15)
    //   - Tractor control mode
    class TractorCommands {
        IsoNet &net_;
        InternalCF *cf_;

      public:
        TractorCommands(IsoNet &net, InternalCF *cf) : net_(net), cf_(cf) {}

        Result<void> initialize() {
            // Register PGN callbacks for incoming tractor commands
            auto reg_result = net_.register_pgn_callback(PGN_REAR_HITCH_CMD, [this](const Message &msg) {
                auto cmd = HitchCommandMsg::decode(msg.data);
                echo::category("isobus.implement.tractor").debug("Rear hitch cmd received");
                on_rear_hitch_command.emit(cmd);
            });
            if (reg_result.is_err())
                return reg_result;

            reg_result = net_.register_pgn_callback(PGN_FRONT_HITCH_CMD, [this](const Message &msg) {
                auto cmd = HitchCommandMsg::decode(msg.data);
                echo::category("isobus.implement.tractor").debug("Front hitch cmd received");
                on_front_hitch_command.emit(cmd);
            });
            if (reg_result.is_err())
                return reg_result;

            reg_result = net_.register_pgn_callback(PGN_REAR_PTO_CMD, [this](const Message &msg) {
                auto cmd = PTOCommandMsg::decode(msg.data);
                echo::category("isobus.implement.tractor").debug("Rear PTO cmd received");
                on_rear_pto_command.emit(cmd);
            });
            if (reg_result.is_err())
                return reg_result;

            reg_result = net_.register_pgn_callback(PGN_FRONT_PTO_CMD, [this](const Message &msg) {
                auto cmd = PTOCommandMsg::decode(msg.data);
                echo::category("isobus.implement.tractor").debug("Front PTO cmd received");
                on_front_pto_command.emit(cmd);
            });
            if (reg_result.is_err())
                return reg_result;

            // Register all 16 aux valve command PGNs (0xFE30 + valve_index)
            for (u8 i = 0; i < 16; ++i) {
                PGN pgn = PGN_AUX_VALVE_CMD + i;
                reg_result = net_.register_pgn_callback(pgn, [this, i](const Message &msg) {
                    auto cmd = AuxValveCommandMsg::decode(msg.data);
                    cmd.valve_index = i;
                    echo::category("isobus.implement.tractor").debug("Aux valve cmd received: valve=", i);
                    on_aux_valve_command.emit(cmd);
                });
                if (reg_result.is_err())
                    return reg_result;
            }

            // Tractor control mode
            reg_result = net_.register_pgn_callback(PGN_TRACTOR_CONTROL_MODE, [this](const Message &msg) {
                auto mode = TractorControlModeMsg::decode(msg.data);
                echo::category("isobus.implement.tractor").debug("Tractor control mode received");
                on_control_mode.emit(mode);
            });
            if (reg_result.is_err())
                return reg_result;

            echo::category("isobus.implement.tractor").info("Tractor commands initialized");
            return {};
        }

        // ─── Send commands ──────────────────────────────────────────────────────
        Result<void> send_rear_hitch(HitchCommandMsg cmd) {
            auto data = cmd.encode();
            echo::category("isobus.implement.tractor").debug("Sending rear hitch cmd=", static_cast<u8>(cmd.command));
            return net_.send(PGN_REAR_HITCH_CMD, data, cf_);
        }

        Result<void> send_front_hitch(HitchCommandMsg cmd) {
            auto data = cmd.encode();
            echo::category("isobus.implement.tractor").debug("Sending front hitch cmd=", static_cast<u8>(cmd.command));
            return net_.send(PGN_FRONT_HITCH_CMD, data, cf_);
        }

        Result<void> send_rear_pto(PTOCommandMsg cmd) {
            auto data = cmd.encode();
            echo::category("isobus.implement.tractor").debug("Sending rear PTO cmd=", static_cast<u8>(cmd.command));
            return net_.send(PGN_REAR_PTO_CMD, data, cf_);
        }

        Result<void> send_front_pto(PTOCommandMsg cmd) {
            auto data = cmd.encode();
            echo::category("isobus.implement.tractor").debug("Sending front PTO cmd=", static_cast<u8>(cmd.command));
            return net_.send(PGN_FRONT_PTO_CMD, data, cf_);
        }

        Result<void> send_aux_valve(AuxValveCommandMsg cmd) {
            auto data = cmd.encode();
            PGN pgn = PGN_AUX_VALVE_CMD + cmd.valve_index;
            echo::category("isobus.implement.tractor")
                .debug("Sending aux valve cmd: valve=", cmd.valve_index, " cmd=", static_cast<u8>(cmd.command));
            return net_.send(pgn, data, cf_);
        }

        Result<void> send_control_mode(const TractorControlModeMsg &msg) {
            return net_.send(PGN_TRACTOR_CONTROL_MODE, msg.encode(), cf_);
        }

        // ─── Events for received commands ───────────────────────────────────────
        Event<HitchCommandMsg> on_rear_hitch_command;
        Event<HitchCommandMsg> on_front_hitch_command;
        Event<PTOCommandMsg> on_rear_pto_command;
        Event<PTOCommandMsg> on_front_pto_command;
        Event<AuxValveCommandMsg> on_aux_valve_command;
        Event<TractorControlModeMsg> on_control_mode;
    };

} // namespace agrobus::isobus::implement
