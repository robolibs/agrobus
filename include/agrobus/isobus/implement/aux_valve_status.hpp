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

    inline constexpr u8 MAX_AUX_VALVES = 16;

    // ─── Valve state (2 bits) ─────────────────────────────────────────────────────
    enum class ValveState : u8 { Blocked = 0, Extending = 1, Retracting = 2, FloatPosition = 3 };

    // ─── Valve limit status (3 bits) ──────────────────────────────────────────────
    enum class ValveLimitStatus : u8 {
        NotLimited = 0,
        OperatorLimited = 1,
        SystemLimited = 2,
        Reserved3 = 3,
        Reserved4 = 4,
        Reserved5 = 5,
        Error = 6,
        NotAvailable = 7
    };

    // ─── Valve fail-safe mode (2 bits) ────────────────────────────────────────────
    enum class ValveFailSafe : u8 { Block = 0, Float = 1, Extend = 2, Retract = 3 };

    // ─── Auxiliary Valve Flow Message (ISO 11783-7) ───────────────────────────────
    // Used for both estimated and measured flow reporting.
    // Byte layout:
    //   Byte 0: extend_flow_percent (0.4% per bit, 0-250 = 0-100%)
    //   Byte 1: retract_flow_percent (0.4% per bit)
    //   Byte 2: bits 0-1: state, bits 2-4: limit_status, bits 5-6: fail_safe, bit 7: reserved
    //   Bytes 3-7: 0xFF reserved
    struct AuxValveFlowMsg {
        u8 valve_index = 0;
        u8 extend_flow_percent = 0xFF;  // 0.4% per bit (0-250 = 0-100%)
        u8 retract_flow_percent = 0xFF; // 0.4% per bit (0-250 = 0-100%)
        ValveState state = ValveState::Blocked;
        ValveLimitStatus limit_status = ValveLimitStatus::NotAvailable;
        ValveFailSafe fail_safe = ValveFailSafe::Block;

        f64 extend_flow() const { return extend_flow_percent == 0xFF ? 0.0 : extend_flow_percent * 0.4; }
        f64 retract_flow() const { return retract_flow_percent == 0xFF ? 0.0 : retract_flow_percent * 0.4; }

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            // Byte 0: Extend flow percent
            data[0] = extend_flow_percent;
            // Byte 1: Retract flow percent
            data[1] = retract_flow_percent;
            // Byte 2: state (bits 0-1) | limit_status (bits 2-4) | fail_safe (bits 5-6) | reserved (bit 7)
            data[2] = static_cast<u8>((static_cast<u8>(state) & 0x03) | ((static_cast<u8>(limit_status) & 0x07) << 2) |
                                      ((static_cast<u8>(fail_safe) & 0x03) << 5) | (0x01 << 7) // reserved bit set to 1
            );
            // Bytes 3-7: Reserved (already 0xFF)
            return data;
        }

        static AuxValveFlowMsg decode(const dp::Vector<u8> &data, u8 valve_idx) {
            AuxValveFlowMsg msg;
            msg.valve_index = valve_idx;
            if (data.size() >= 3) {
                msg.extend_flow_percent = data[0];
                msg.retract_flow_percent = data[1];
                msg.state = static_cast<ValveState>(data[2] & 0x03);
                msg.limit_status = static_cast<ValveLimitStatus>((data[2] >> 2) & 0x07);
                msg.fail_safe = static_cast<ValveFailSafe>((data[2] >> 5) & 0x03);
            }
            return msg;
        }
    };

    // ─── Auxiliary Valve Status Interface (ISO 11783-7 Class 2 TECU) ──────────────
    // Reports estimated and measured hydraulic valve flow for up to 16 valves.
    // Transmission rate: 100ms per valve.
    class AuxValveStatusInterface {
        IsoNet &net_;
        InternalCF *cf_;
        dp::Array<dp::Optional<AuxValveFlowMsg>, MAX_AUX_VALVES> estimated_;
        dp::Array<dp::Optional<AuxValveFlowMsg>, MAX_AUX_VALVES> measured_;

      public:
        AuxValveStatusInterface(IsoNet &net, InternalCF *cf) : net_(net), cf_(cf) {}

        Result<void> initialize() {
            // Register PGN callbacks for all 16 valves (estimated flow)
            for (u8 i = 0; i < MAX_AUX_VALVES; ++i) {
                PGN pgn = PGN_AUX_VALVE_ESTIMATED_FLOW_BASE + i;
                auto reg_result = net_.register_pgn_callback(pgn, [this, i](const Message &msg) {
                    auto flow = AuxValveFlowMsg::decode(msg.data, i);
                    echo::category("isobus.implement.aux_valve")
                        .debug("Estimated flow received: valve=", i, " extend=", flow.extend_flow(),
                               "% retract=", flow.retract_flow(), "%");
                    estimated_[i] = flow;
                    on_estimated_flow.emit(flow);
                });
                if (reg_result.is_err())
                    return reg_result;
            }

            // Register PGN callbacks for all 16 valves (measured flow)
            for (u8 i = 0; i < MAX_AUX_VALVES; ++i) {
                PGN pgn = PGN_AUX_VALVE_MEASURED_FLOW_BASE + i;
                auto reg_result = net_.register_pgn_callback(pgn, [this, i](const Message &msg) {
                    auto flow = AuxValveFlowMsg::decode(msg.data, i);
                    echo::category("isobus.implement.aux_valve")
                        .debug("Measured flow received: valve=", i, " extend=", flow.extend_flow(),
                               "% retract=", flow.retract_flow(), "%");
                    measured_[i] = flow;
                    on_measured_flow.emit(flow);
                });
                if (reg_result.is_err())
                    return reg_result;
            }

            echo::category("isobus.implement.aux_valve").info("Aux valve status interface initialized");
            return {};
        }

        // ─── Send flow status ────────────────────────────────────────────────────
        Result<void> send_estimated_flow(AuxValveFlowMsg msg) {
            auto data = msg.encode();
            PGN pgn = PGN_AUX_VALVE_ESTIMATED_FLOW_BASE + msg.valve_index;
            echo::category("isobus.implement.aux_valve")
                .debug("Sending estimated flow: valve=", msg.valve_index, " extend=", msg.extend_flow(),
                       "% retract=", msg.retract_flow(), "%");
            return net_.send(pgn, data, cf_);
        }

        Result<void> send_measured_flow(AuxValveFlowMsg msg) {
            auto data = msg.encode();
            PGN pgn = PGN_AUX_VALVE_MEASURED_FLOW_BASE + msg.valve_index;
            echo::category("isobus.implement.aux_valve")
                .debug("Sending measured flow: valve=", msg.valve_index, " extend=", msg.extend_flow(),
                       "% retract=", msg.retract_flow(), "%");
            return net_.send(pgn, data, cf_);
        }

        // ─── Query latest values ────────────────────────────────────────────────
        dp::Optional<AuxValveFlowMsg> latest_estimated(u8 valve_index) const {
            if (valve_index >= MAX_AUX_VALVES)
                return dp::nullopt;
            return estimated_[valve_index];
        }

        dp::Optional<AuxValveFlowMsg> latest_measured(u8 valve_index) const {
            if (valve_index >= MAX_AUX_VALVES)
                return dp::nullopt;
            return measured_[valve_index];
        }

        // ─── Events for received flow messages ──────────────────────────────────
        Event<AuxValveFlowMsg> on_estimated_flow;
        Event<AuxValveFlowMsg> on_measured_flow;
    };

} // namespace agrobus::isobus::implement
