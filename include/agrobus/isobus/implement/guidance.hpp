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

    // ─── Steering System Readiness (ISO 11783-7 G Addendum) ────────────────────
    enum class SteeringReadiness : u8 {
        NotReady = 0,
        MechanicalReady = 1,
        FullyReady = 2,
        Error = 3,
        NotAvailable = 7
    };

    // ─── Mechanical System Lockout (ISO 11783-7 G Addendum) ────────────────────
    enum class MechanicalLockout : u8 { NotActive = 0, Active = 1, Error = 2, NotAvailable = 3 };

    // ─── Guidance Curvature Command (PGN 0xFE46) ──────────────────────────────
    // Sent by guidance controller to the steering system.
    // Broadcast at 100ms.
    struct CurvatureCommand {
        f64 curvature = 0.0;      // 1/km, desired path curvature, 0.25/km per bit offset -8032
        f64 curvature_rate = 0.0; // not standardized, reserved for implementation

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            // Curvature: 0.25 1/km per bit, offset -8032, 2 bytes (bytes 0-1)
            u16 curv_raw = static_cast<u16>((curvature + 8032.0) / 0.25);
            data[0] = static_cast<u8>(curv_raw & 0xFF);
            data[1] = static_cast<u8>((curv_raw >> 8) & 0xFF);
            // Bytes 2-7: Reserved
            return data;
        }

        static CurvatureCommand decode(const dp::Vector<u8> &data) {
            CurvatureCommand msg;
            if (data.size() >= 2) {
                u16 curv_raw = static_cast<u16>(data[0]) | (static_cast<u16>(data[1]) << 8);
                msg.curvature = curv_raw * 0.25 - 8032.0;
            }
            return msg;
        }
    };

    // ─── Guidance Machine Info (PGN 0xFE44) ────────────────────────────────────
    // Sent by the machine (TECU) to report current steering geometry.
    // Broadcast at 100ms.
    struct GuidanceMachineInfo {
        f64 estimated_curvature = 0.0; // 1/km, 0.25/km per bit offset -8032
        MechanicalLockout lockout = MechanicalLockout::NotAvailable;
        u8 request_reset_cmd = 0x03; // 2 bits: 0=reset not required, 1=reset required, 3=N/A

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            u16 curv_raw = static_cast<u16>((estimated_curvature + 8032.0) / 0.25);
            data[0] = static_cast<u8>(curv_raw & 0xFF);
            data[1] = static_cast<u8>((curv_raw >> 8) & 0xFF);
            data[2] = (static_cast<u8>(lockout) & 0x03) | ((request_reset_cmd & 0x03) << 2);
            return data;
        }

        static GuidanceMachineInfo decode(const dp::Vector<u8> &data) {
            GuidanceMachineInfo msg;
            if (data.size() >= 3) {
                u16 curv_raw = static_cast<u16>(data[0]) | (static_cast<u16>(data[1]) << 8);
                msg.estimated_curvature = curv_raw * 0.25 - 8032.0;
                msg.lockout = static_cast<MechanicalLockout>(data[2] & 0x03);
                msg.request_reset_cmd = (data[2] >> 2) & 0x03;
            }
            return msg;
        }
    };

    // ─── Guidance System Command (PGN 0xFE45) ─────────────────────────────────
    // Sent by the guidance controller to report system status.
    // Broadcast at 100ms.
    struct GuidanceSystemStatus {
        f64 estimated_curvature = 0.0; // 1/km, 0.25/km per bit offset -8032
        SteeringReadiness readiness = SteeringReadiness::NotAvailable;
        u8 integrity_level = 0x03; // 2 bits: steering integrity (0=lowest, 3=N/A)

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            u16 curv_raw = static_cast<u16>((estimated_curvature + 8032.0) / 0.25);
            data[0] = static_cast<u8>(curv_raw & 0xFF);
            data[1] = static_cast<u8>((curv_raw >> 8) & 0xFF);
            data[2] = (static_cast<u8>(readiness) & 0x07) | ((integrity_level & 0x03) << 4);
            return data;
        }

        static GuidanceSystemStatus decode(const dp::Vector<u8> &data) {
            GuidanceSystemStatus msg;
            if (data.size() >= 3) {
                u16 curv_raw = static_cast<u16>(data[0]) | (static_cast<u16>(data[1]) << 8);
                msg.estimated_curvature = curv_raw * 0.25 - 8032.0;
                msg.readiness = static_cast<SteeringReadiness>(data[2] & 0x07);
                msg.integrity_level = (data[2] >> 4) & 0x03;
            }
            return msg;
        }
    };

    // ─── Guidance Interface ────────────────────────────────────────────────────
    // Handles reception and transmission of guidance-related messages.
    class GuidanceCurvatureInterface {
        IsoNet &net_;
        InternalCF *cf_;

      public:
        GuidanceCurvatureInterface(IsoNet &net, InternalCF *cf) : net_(net), cf_(cf) {}

        Result<void> initialize() {
            if (!cf_) {
                return Result<void>::err(Error::invalid_state("control function not set"));
            }
            net_.register_pgn_callback(PGN_GUIDANCE_CURVATURE_CMD, [this](const Message &msg) {
                on_curvature_command.emit(CurvatureCommand::decode(msg.data), msg.source);
            });
            net_.register_pgn_callback(PGN_GUIDANCE_MACHINE, [this](const Message &msg) {
                on_machine_info.emit(GuidanceMachineInfo::decode(msg.data), msg.source);
            });
            net_.register_pgn_callback(PGN_GUIDANCE_SYSTEM, [this](const Message &msg) {
                on_system_status.emit(GuidanceSystemStatus::decode(msg.data), msg.source);
            });
            echo::category("isobus.implement.guidance").debug("initialized");
            return {};
        }

        // Send curvature command (guidance controller -> steering system)
        Result<void> send_curvature_command(const CurvatureCommand &msg) {
            return net_.send(PGN_GUIDANCE_CURVATURE_CMD, msg.encode(), cf_, nullptr, Priority::Default);
        }

        // Send machine info (TECU -> network)
        Result<void> send_machine_info(const GuidanceMachineInfo &msg) {
            return net_.send(PGN_GUIDANCE_MACHINE, msg.encode(), cf_, nullptr, Priority::Default);
        }

        // Send system status (guidance controller -> network)
        Result<void> send_system_status(const GuidanceSystemStatus &msg) {
            return net_.send(PGN_GUIDANCE_SYSTEM, msg.encode(), cf_, nullptr, Priority::Default);
        }

        // Events
        Event<CurvatureCommand, Address> on_curvature_command;
        Event<GuidanceMachineInfo, Address> on_machine_info;
        Event<GuidanceSystemStatus, Address> on_system_status;
    };

} // namespace agrobus::isobus::implement
