#pragma once

#include <agrobus/net/error.hpp>
#include <agrobus/net/event.hpp>
#include <agrobus/net/internal_cf.hpp>
#include <agrobus/net/message.hpp>
#include <agrobus/net/network_manager.hpp>
#include <datapod/datapod.hpp>
#include <echo/echo.hpp>

namespace agrobus::j1939 {
    using namespace agrobus::net;

    // ─── Proprietary Message Types (ISO 11783-3, Section 5.4.6) ─────────────────
    //
    // Proprietary A (PGN 0xEF00): Destination-specific, up to 1785 bytes via TP.
    //   Manufacturer identifies itself via source NAME.
    //
    // Proprietary A2 (PGN 0x1EF00): Extended data page version of Proprietary A.
    //
    // Proprietary B (PGN 0xFF00-0xFFFF): Broadcast, 256 PGNs available.
    //   Each manufacturer uses a subset identified by the group extension byte.

    struct ProprietaryMsg {
        PGN pgn = PGN_PROPRIETARY_A;
        dp::Vector<u8> data;
        Address source = NULL_ADDRESS;
        Address destination = BROADCAST_ADDRESS;

        bool is_proprietary_a() const noexcept { return pgn == PGN_PROPRIETARY_A; }
        bool is_proprietary_a2() const noexcept { return pgn == PGN_PROPRIETARY_A2; }
        bool is_proprietary_b() const noexcept { return pgn >= PGN_PROPRIETARY_B_BASE && pgn <= 0xFFFF; }
        u8 group_extension() const noexcept { return static_cast<u8>(pgn & 0xFF); }
    };

    // ─── Proprietary Message Interface ──────────────────────────────────────────
    // Provides send/receive for manufacturer-proprietary messages.
    class ProprietaryInterface {
        IsoNet &net_;
        InternalCF *cf_;

      public:
        ProprietaryInterface(IsoNet &net, InternalCF *cf) : net_(net), cf_(cf) {}

        Result<void> initialize() {
            if (!cf_) {
                return Result<void>::err(Error::invalid_state("control function not set"));
            }

            // Register Proprietary A
            net_.register_pgn_callback(PGN_PROPRIETARY_A, [this](const Message &msg) {
                ProprietaryMsg pmsg;
                pmsg.pgn = PGN_PROPRIETARY_A;
                pmsg.data = msg.data;
                pmsg.source = msg.source;
                pmsg.destination = msg.destination;
                on_proprietary_a.emit(pmsg);
            });

            // Register Proprietary A2
            net_.register_pgn_callback(PGN_PROPRIETARY_A2, [this](const Message &msg) {
                ProprietaryMsg pmsg;
                pmsg.pgn = PGN_PROPRIETARY_A2;
                pmsg.data = msg.data;
                pmsg.source = msg.source;
                pmsg.destination = msg.destination;
                on_proprietary_a2.emit(pmsg);
            });

            echo::category("isobus.proprietary").debug("initialized");
            return {};
        }

        // Register a Proprietary B group extension PGN for reception
        Result<void> register_proprietary_b(u8 group_extension) {
            PGN pgn = PGN_PROPRIETARY_B_BASE + group_extension;
            return net_.register_pgn_callback(pgn, [this, pgn](const Message &msg) {
                ProprietaryMsg pmsg;
                pmsg.pgn = pgn;
                pmsg.data = msg.data;
                pmsg.source = msg.source;
                pmsg.destination = msg.destination;
                on_proprietary_b.emit(pmsg);
            });
        }

        // ─── Send methods ─────────────────────────────────────────────────────────
        Result<void> send_proprietary_a(const dp::Vector<u8> &data, ControlFunction *dest) {
            if (!dest) {
                return Result<void>::err(Error::invalid_state("Proprietary A requires destination"));
            }
            return net_.send(PGN_PROPRIETARY_A, data, cf_, dest);
        }

        Result<void> send_proprietary_a2(const dp::Vector<u8> &data, ControlFunction *dest) {
            if (!dest) {
                return Result<void>::err(Error::invalid_state("Proprietary A2 requires destination"));
            }
            return net_.send(PGN_PROPRIETARY_A2, data, cf_, dest);
        }

        Result<void> send_proprietary_b(u8 group_extension, const dp::Vector<u8> &data) {
            PGN pgn = PGN_PROPRIETARY_B_BASE + group_extension;
            return net_.send(pgn, data, cf_);
        }

        // ─── Events ──────────────────────────────────────────────────────────────
        Event<const ProprietaryMsg &> on_proprietary_a;
        Event<const ProprietaryMsg &> on_proprietary_a2;
        Event<const ProprietaryMsg &> on_proprietary_b;
    };
} // namespace agrobus::j1939
