#pragma once

#include <agrobus/net/constants.hpp>
#include <agrobus/net/control_function.hpp>
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

    enum class AckControl : u8 { PositiveAck = 0, NegativeAck = 1, AccessDenied = 2, CannotRespond = 3 };

    struct Acknowledgment {
        AckControl control = AckControl::PositiveAck;
        u8 group_function = 0xFF;
        PGN acknowledged_pgn = 0;
        Address address = 0xFF;

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = static_cast<u8>(control);
            data[1] = group_function;
            data[2] = 0xFF; // reserved
            data[3] = 0xFF; // reserved
            data[4] = address;
            data[5] = static_cast<u8>(acknowledged_pgn & 0xFF);
            data[6] = static_cast<u8>((acknowledged_pgn >> 8) & 0xFF);
            data[7] = static_cast<u8>((acknowledged_pgn >> 16) & 0xFF);
            return data;
        }

        static Acknowledgment decode(const dp::Vector<u8> &data) {
            Acknowledgment ack;
            if (data.size() >= 8) {
                ack.control = static_cast<AckControl>(data[0]);
                ack.group_function = data[1];
                ack.address = data[4];
                ack.acknowledged_pgn =
                    static_cast<PGN>(data[5]) | (static_cast<PGN>(data[6]) << 8) | (static_cast<PGN>(data[7]) << 16);
            }
            return ack;
        }
    };

    // ─── Helper class for sending ACK/NACK ──────────────────────────────────────
    class AcknowledgmentHandler {
        IsoNet &net_;
        InternalCF *cf_;

      public:
        AcknowledgmentHandler(IsoNet &net, InternalCF *cf) : net_(net), cf_(cf) {}

        Result<void> send_ack(PGN pgn, Address dest) {
            Acknowledgment ack;
            ack.control = AckControl::PositiveAck;
            ack.acknowledged_pgn = pgn;
            ack.address = dest;
            ControlFunction dest_cf;
            dest_cf.address = dest;
            echo::category("isobus.ack").debug("Sending ACK for PGN ", pgn, " to ", dest);
            return net_.send(PGN_ACKNOWLEDGMENT, ack.encode(), cf_, &dest_cf);
        }

        Result<void> send_nack(PGN pgn, Address dest) {
            Acknowledgment ack;
            ack.control = AckControl::NegativeAck;
            ack.acknowledged_pgn = pgn;
            ack.address = dest;
            ControlFunction dest_cf;
            dest_cf.address = dest;
            echo::category("isobus.ack").debug("Sending NACK for PGN ", pgn, " to ", dest);
            return net_.send(PGN_ACKNOWLEDGMENT, ack.encode(), cf_, &dest_cf);
        }

        Result<void> send_access_denied(PGN pgn, Address dest) {
            Acknowledgment ack;
            ack.control = AckControl::AccessDenied;
            ack.acknowledged_pgn = pgn;
            ack.address = dest;
            ControlFunction dest_cf;
            dest_cf.address = dest;
            echo::category("isobus.ack").debug("Sending Access Denied for PGN ", pgn, " to ", dest);
            return net_.send(PGN_ACKNOWLEDGMENT, ack.encode(), cf_, &dest_cf);
        }

        Result<void> send_cannot_respond(PGN pgn, Address dest) {
            Acknowledgment ack;
            ack.control = AckControl::CannotRespond;
            ack.acknowledged_pgn = pgn;
            ack.address = dest;
            ControlFunction dest_cf;
            dest_cf.address = dest;
            echo::category("isobus.ack").debug("Sending Cannot Respond for PGN ", pgn, " to ", dest);
            return net_.send(PGN_ACKNOWLEDGMENT, ack.encode(), cf_, &dest_cf);
        }

        // Event for received ACKs/NACKs
        Event<Acknowledgment, Address> on_acknowledgment;

        Result<void> initialize() {
            net_.register_pgn_callback(PGN_ACKNOWLEDGMENT, [this](const Message &msg) {
                auto ack = Acknowledgment::decode(msg.data);
                on_acknowledgment.emit(ack, msg.source);
                echo::category("isobus.ack")
                    .debug("Received ACK/NACK: control=", static_cast<u8>(ack.control), " pgn=", ack.acknowledged_pgn,
                           " from=", msg.source);
            });
            echo::category("isobus.ack").debug("AcknowledgmentHandler initialized");
            return {};
        }
    };
} // namespace agrobus::j1939
