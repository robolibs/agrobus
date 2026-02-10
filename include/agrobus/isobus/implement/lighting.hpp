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

    // ─── Light state enum (2 bits per light) ──────────────────────────────────────
    // 00=off, 01=on, 10=error, 11=not available
    enum class LightState : u8 { Off = 0, On = 1, Error = 2, NotAvailable = 3 };

    // ─── Lighting State (ISO 11783-7 lighting messages) ───────────────────────────
    // All light types defined in ISO 11783-7 lighting data/command messages.
    // Encoding: 4 lights per byte (2 bits each), packed LSB-first.
    //   Byte 0: left_turn(0:1), right_turn(2:3), low_beam(4:5), high_beam(6:7)
    //   Byte 1: front_fog(0:1), rear_fog(2:3), beacon(4:5), running(6:7)
    //   Byte 2: rear_work(0:1), front_work(2:3), side_work(4:5), hazard(6:7)
    //   Byte 3: backup(0:1), center_stop(2:3), left_stop(4:5), right_stop(6:7)
    //   Bytes 4-7: 0xFF (reserved)
    struct LightingState {
        LightState left_turn = LightState::NotAvailable;
        LightState right_turn = LightState::NotAvailable;
        LightState low_beam = LightState::NotAvailable;
        LightState high_beam = LightState::NotAvailable;
        LightState front_fog = LightState::NotAvailable;
        LightState rear_fog = LightState::NotAvailable;
        LightState beacon = LightState::NotAvailable;
        LightState running = LightState::NotAvailable;     // daytime running lights
        LightState rear_work = LightState::NotAvailable;   // rear work light
        LightState front_work = LightState::NotAvailable;  // front work light
        LightState side_work = LightState::NotAvailable;   // side work light (implement)
        LightState hazard = LightState::NotAvailable;      // hazard warning
        LightState backup = LightState::NotAvailable;      // back-up / reverse
        LightState center_stop = LightState::NotAvailable; // center stop
        LightState left_stop = LightState::NotAvailable;
        LightState right_stop = LightState::NotAvailable;

        // Encode to 8-byte CAN data (2 bits per light, packed into bytes)
        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            // Byte 0: left_turn(0:1), right_turn(2:3), low_beam(4:5), high_beam(6:7)
            data[0] = static_cast<u8>((static_cast<u8>(left_turn) << 0) | (static_cast<u8>(right_turn) << 2) |
                                      (static_cast<u8>(low_beam) << 4) | (static_cast<u8>(high_beam) << 6));
            // Byte 1: front_fog(0:1), rear_fog(2:3), beacon(4:5), running(6:7)
            data[1] = static_cast<u8>((static_cast<u8>(front_fog) << 0) | (static_cast<u8>(rear_fog) << 2) |
                                      (static_cast<u8>(beacon) << 4) | (static_cast<u8>(running) << 6));
            // Byte 2: rear_work(0:1), front_work(2:3), side_work(4:5), hazard(6:7)
            data[2] = static_cast<u8>((static_cast<u8>(rear_work) << 0) | (static_cast<u8>(front_work) << 2) |
                                      (static_cast<u8>(side_work) << 4) | (static_cast<u8>(hazard) << 6));
            // Byte 3: backup(0:1), center_stop(2:3), left_stop(4:5), right_stop(6:7)
            data[3] = static_cast<u8>((static_cast<u8>(backup) << 0) | (static_cast<u8>(center_stop) << 2) |
                                      (static_cast<u8>(left_stop) << 4) | (static_cast<u8>(right_stop) << 6));
            // Bytes 4-7: reserved (already 0xFF)
            return data;
        }

        // Decode from 8-byte CAN data
        static LightingState decode(const dp::Vector<u8> &data) {
            LightingState state;
            if (data.size() >= 4) {
                // Byte 0
                state.left_turn = static_cast<LightState>((data[0] >> 0) & 0x03);
                state.right_turn = static_cast<LightState>((data[0] >> 2) & 0x03);
                state.low_beam = static_cast<LightState>((data[0] >> 4) & 0x03);
                state.high_beam = static_cast<LightState>((data[0] >> 6) & 0x03);
                // Byte 1
                state.front_fog = static_cast<LightState>((data[1] >> 0) & 0x03);
                state.rear_fog = static_cast<LightState>((data[1] >> 2) & 0x03);
                state.beacon = static_cast<LightState>((data[1] >> 4) & 0x03);
                state.running = static_cast<LightState>((data[1] >> 6) & 0x03);
                // Byte 2
                state.rear_work = static_cast<LightState>((data[2] >> 0) & 0x03);
                state.front_work = static_cast<LightState>((data[2] >> 2) & 0x03);
                state.side_work = static_cast<LightState>((data[2] >> 4) & 0x03);
                state.hazard = static_cast<LightState>((data[2] >> 6) & 0x03);
                // Byte 3
                state.backup = static_cast<LightState>((data[3] >> 0) & 0x03);
                state.center_stop = static_cast<LightState>((data[3] >> 2) & 0x03);
                state.left_stop = static_cast<LightState>((data[3] >> 4) & 0x03);
                state.right_stop = static_cast<LightState>((data[3] >> 6) & 0x03);
            }
            return state;
        }
    };

    // ─── Lighting Interface (ISO 11783-7 Section 4.5) ─────────────────────────────
    // Provides send/receive interface for tractor/implement lighting messages:
    //   - PGN_LIGHTING_DATA: Broadcast from tractor, 100ms rate when active
    //   - PGN_LIGHTING_COMMAND: Broadcast from TECU to implements
    class LightingInterface {
        IsoNet &net_;
        InternalCF *cf_;
        dp::Optional<LightingState> latest_data_;
        dp::Optional<LightingState> latest_command_;

      public:
        LightingInterface(IsoNet &net, InternalCF *cf) : net_(net), cf_(cf) {}

        Result<void> initialize() {
            // Register PGN callback for lighting data (tractor broadcast)
            auto reg_result = net_.register_pgn_callback(PGN_LIGHTING_DATA, [this](const Message &msg) {
                auto state = LightingState::decode(msg.data);
                latest_data_ = state;
                echo::category("isobus.implement.lighting").debug("Lighting data received");
                on_lighting_data.emit(state);
            });
            if (reg_result.is_err())
                return reg_result;

            // Register PGN callback for lighting command (TECU to implements)
            reg_result = net_.register_pgn_callback(PGN_LIGHTING_COMMAND, [this](const Message &msg) {
                auto state = LightingState::decode(msg.data);
                latest_command_ = state;
                echo::category("isobus.implement.lighting").debug("Lighting command received");
                on_lighting_command.emit(state);
            });
            if (reg_result.is_err())
                return reg_result;

            echo::category("isobus.implement.lighting").info("Lighting interface initialized");
            return {};
        }

        // ─── Send messages ───────────────────────────────────────────────────────
        Result<void> send_lighting_data(LightingState state) {
            auto data = state.encode();
            echo::category("isobus.implement.lighting").debug("Sending lighting data");
            return net_.send(PGN_LIGHTING_DATA, data, cf_);
        }

        Result<void> send_lighting_command(LightingState state) {
            auto data = state.encode();
            echo::category("isobus.implement.lighting").debug("Sending lighting command");
            return net_.send(PGN_LIGHTING_COMMAND, data, cf_);
        }

        // ─── Latest received state ──────────────────────────────────────────────
        dp::Optional<LightingState> latest_data() const { return latest_data_; }
        dp::Optional<LightingState> latest_command() const { return latest_command_; }

        // ─── Events for received messages ────────────────────────────────────────
        Event<LightingState> on_lighting_data;
        Event<LightingState> on_lighting_command;
    };

} // namespace agrobus::isobus::implement
