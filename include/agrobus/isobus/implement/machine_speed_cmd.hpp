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

    // ─── Speed source (2 bits in machine selected speed message) ────────────────
    enum class SpeedSource : u8 { WheelBased = 0, GroundBased = 1, NavigationBased = 2, Blended = 3 };

    // ─── Machine direction ──────────────────────────────────────────────────────
    enum class MachineDirection : u8 { Forward = 0, Reverse = 1, Error = 2, NotAvailable = 3 };

    // ─── Exit/reason codes for speed limit ──────────────────────────────────────
    enum class SpeedExitCode : u8 { NotLimited = 0, OperatorLimited = 1, SystemLimited = 2, NotAvailable = 3 };

    // ─── Machine Selected Speed status (from TECU, PGN 0xF022) ─────────────────
    struct MachineSelectedSpeedMsg {
        u16 speed_raw = 0xFFFF; // 0.001 m/s per bit (0-65.534 m/s)
        MachineDirection direction = MachineDirection::NotAvailable;
        SpeedSource source = SpeedSource::WheelBased;
        SpeedExitCode limit_status = SpeedExitCode::NotAvailable;

        f64 speed_mps() const { return speed_raw == 0xFFFF ? 0.0 : speed_raw * 0.001; }

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            // Byte 0-1: speed (0.001 m/s per bit, LE)
            data[0] = static_cast<u8>(speed_raw & 0xFF);
            data[1] = static_cast<u8>((speed_raw >> 8) & 0xFF);
            // Byte 2-3: distance (not used here, 0xFF)
            data[2] = 0xFF;
            data[3] = 0xFF;
            // Byte 4: bits 0-1: direction, bits 2-3: source, bits 4-5: limit_status, bits 6-7: reserved
            data[4] = static_cast<u8>((static_cast<u8>(direction) & 0x03) | ((static_cast<u8>(source) & 0x03) << 2) |
                                      ((static_cast<u8>(limit_status) & 0x03) << 4) | 0xC0 // reserved bits set to 1
            );
            // Bytes 5-7: reserved
            data[5] = 0xFF;
            data[6] = 0xFF;
            data[7] = 0xFF;
            return data;
        }

        static MachineSelectedSpeedMsg decode(const dp::Vector<u8> &data) {
            MachineSelectedSpeedMsg msg;
            if (data.size() >= 5) {
                msg.speed_raw = static_cast<u16>(data[0]) | (static_cast<u16>(data[1]) << 8);
                msg.direction = static_cast<MachineDirection>(data[4] & 0x03);
                msg.source = static_cast<SpeedSource>((data[4] >> 2) & 0x03);
                msg.limit_status = static_cast<SpeedExitCode>((data[4] >> 4) & 0x03);
            }
            return msg;
        }
    };

    // ─── Machine Selected Speed Command (from implement, PGN 0xFE4C) ───────────
    struct MachineSpeedCommandMsg {
        u16 target_speed_raw = 0xFFFF; // 0.001 m/s per bit
        MachineDirection direction_cmd = MachineDirection::NotAvailable;

        f64 target_speed_mps() const { return target_speed_raw == 0xFFFF ? 0.0 : target_speed_raw * 0.001; }

        MachineSpeedCommandMsg &set_speed_mps(f64 mps) {
            target_speed_raw = static_cast<u16>(mps / 0.001);
            return *this;
        }

        MachineSpeedCommandMsg &set_direction(MachineDirection d) {
            direction_cmd = d;
            return *this;
        }

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            // Byte 0-1: target speed (0.001 m/s per bit, LE)
            data[0] = static_cast<u8>(target_speed_raw & 0xFF);
            data[1] = static_cast<u8>((target_speed_raw >> 8) & 0xFF);
            // Byte 2: bits 0-1: direction command, bits 2-7: reserved (0xFF mask)
            data[2] = static_cast<u8>((static_cast<u8>(direction_cmd) & 0x03) | 0xFC);
            // Bytes 3-7: reserved
            data[3] = 0xFF;
            data[4] = 0xFF;
            data[5] = 0xFF;
            data[6] = 0xFF;
            data[7] = 0xFF;
            return data;
        }

        static MachineSpeedCommandMsg decode(const dp::Vector<u8> &data) {
            MachineSpeedCommandMsg msg;
            if (data.size() >= 3) {
                msg.target_speed_raw = static_cast<u16>(data[0]) | (static_cast<u16>(data[1]) << 8);
                msg.direction_cmd = static_cast<MachineDirection>(data[2] & 0x03);
            }
            return msg;
        }
    };

    // ─── Machine Speed Interface (ISO 11783-7/9) ────────────────────────────────
    // Provides send/receive interface for machine selected speed:
    //   - Status from TECU (PGN 0xF022, 100ms broadcast)
    //   - Command from implement to TECU (PGN 0xFE4C)
    class MachineSpeedInterface {
        IsoNet &net_;
        InternalCF *cf_;
        dp::Optional<MachineSelectedSpeedMsg> latest_status_;

      public:
        MachineSpeedInterface(IsoNet &net, InternalCF *cf) : net_(net), cf_(cf) {}

        Result<void> initialize() {
            // Register callback for machine selected speed status (from TECU)
            auto reg_result = net_.register_pgn_callback(PGN_MACHINE_SPEED, [this](const Message &msg) {
                auto status = MachineSelectedSpeedMsg::decode(msg.data);
                echo::category("isobus.implement.speed_cmd")
                    .debug("Speed status received: ", status.speed_mps(), " m/s");
                latest_status_ = status;
                on_speed_status.emit(status);
            });
            if (reg_result.is_err())
                return reg_result;

            // Register callback for machine speed command (from implement)
            reg_result = net_.register_pgn_callback(PGN_MACHINE_SELECTED_SPEED_CMD, [this](const Message &msg) {
                auto cmd = MachineSpeedCommandMsg::decode(msg.data);
                echo::category("isobus.implement.speed_cmd")
                    .debug("Speed command received: ", cmd.target_speed_mps(), " m/s");
                on_speed_command.emit(cmd);
            });
            if (reg_result.is_err())
                return reg_result;

            echo::category("isobus.implement.speed_cmd").info("Machine speed interface initialized");
            return {};
        }

        // ─── Send commands ──────────────────────────────────────────────────────
        Result<void> send_speed_command(MachineSpeedCommandMsg cmd) {
            auto data = cmd.encode();
            echo::category("isobus.implement.speed_cmd")
                .debug("Sending speed command: ", cmd.target_speed_mps(), " m/s");
            return net_.send(PGN_MACHINE_SELECTED_SPEED_CMD, data, cf_);
        }

        Result<void> send_speed_status(MachineSelectedSpeedMsg status) {
            auto data = status.encode();
            echo::category("isobus.implement.speed_cmd").debug("Sending speed status: ", status.speed_mps(), " m/s");
            return net_.send(PGN_MACHINE_SPEED, data, cf_);
        }

        // ─── Accessors ─────────────────────────────────────────────────────────
        dp::Optional<MachineSelectedSpeedMsg> latest_status() const noexcept { return latest_status_; }

        // ─── Events for received messages ───────────────────────────────────────
        Event<MachineSelectedSpeedMsg> on_speed_status;
        Event<MachineSpeedCommandMsg> on_speed_command;
    };

} // namespace agrobus::isobus::implement
