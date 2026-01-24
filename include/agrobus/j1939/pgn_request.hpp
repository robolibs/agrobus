#pragma once

#include "acknowledgment.hpp"
#include <agrobus/net/constants.hpp>
#include <agrobus/net/error.hpp>
#include <agrobus/net/event.hpp>
#include <agrobus/net/frame.hpp>
#include <agrobus/net/message.hpp>
#include <agrobus/net/network_manager.hpp>
#include <agrobus/net/types.hpp>
#include <datapod/datapod.hpp>
#include <echo/echo.hpp>
#include <functional>

namespace agrobus::j1939 {
    using namespace agrobus::net;

    // AckType is a compatibility alias for AckControl
    using AckType = AckControl;

    // ─── PGN Request/Response protocol ──────────────────────────────────────────
    class PGNRequestProtocol {
        IsoNet &net_;
        InternalCF *cf_;

        // Registered PGN responders
        dp::Map<PGN, std::function<dp::Vector<u8>()>> responders_;

      public:
        PGNRequestProtocol(IsoNet &net, InternalCF *cf) : net_(net), cf_(cf) {}

        Result<void> initialize() {
            if (!cf_) {
                return Result<void>::err(Error::invalid_state("control function not set"));
            }
            net_.register_pgn_callback(PGN_REQUEST, [this](const Message &msg) { handle_request(msg); });
            net_.register_pgn_callback(PGN_ACKNOWLEDGMENT, [this](const Message &msg) { handle_ack(msg); });
            echo::category("isobus.protocol.pgn_req").debug("initialized");
            return {};
        }

        // Register a responder for a specific PGN
        Result<void> register_responder(PGN pgn, std::function<dp::Vector<u8>()> responder) {
            if (!responder) {
                return Result<void>::err(Error::invalid_state("null responder"));
            }
            responders_[pgn] = std::move(responder);
            echo::category("isobus.protocol.pgn_req").debug("responder registered for pgn=", pgn);
            return {};
        }

        // Send a PGN request to a specific destination or broadcast
        Result<void> request(PGN pgn, Address destination = BROADCAST_ADDRESS) {
            dp::Vector<u8> data(3);
            data[0] = static_cast<u8>(pgn & 0xFF);
            data[1] = static_cast<u8>((pgn >> 8) & 0xFF);
            data[2] = static_cast<u8>((pgn >> 16) & 0xFF);

            echo::category("isobus.protocol.pgn_req").debug("PGN request: ", pgn, " to ", destination);

            if (destination == BROADCAST_ADDRESS) {
                return net_.send(PGN_REQUEST, data, cf_, nullptr, Priority::Default);
            }

            // Destination-specific request
            ControlFunction dest_cf;
            dest_cf.address = destination;
            return net_.send(PGN_REQUEST, data, cf_, &dest_cf, Priority::Default);
        }

        // Send an acknowledgment (positive or negative)
        Result<void> send_ack(AckType type, PGN pgn, Address destination) {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = static_cast<u8>(type);
            data[1] = 0xFF;        // group function
            data[2] = 0xFF;        // reserved
            data[3] = 0xFF;        // reserved
            data[4] = destination; // address acknowledged
            data[5] = static_cast<u8>(pgn & 0xFF);
            data[6] = static_cast<u8>((pgn >> 8) & 0xFF);
            data[7] = static_cast<u8>((pgn >> 16) & 0xFF);

            return net_.send(PGN_ACKNOWLEDGMENT, data, cf_);
        }

        // Events
        Event<PGN, Address> on_request_received; // (requested_pgn, requester_address)
        Event<const Acknowledgment &> on_ack_received;

      private:
        void handle_request(const Message &msg) {
            if (msg.data.size() < 3)
                return;

            PGN requested_pgn = static_cast<u32>(msg.data[0]) | (static_cast<u32>(msg.data[1]) << 8) |
                                (static_cast<u32>(msg.data[2]) << 16);

            echo::category("isobus.protocol.pgn_req")
                .debug("PGN request received: ", requested_pgn, " from ", msg.source);
            on_request_received.emit(requested_pgn, msg.source);

            auto it = responders_.find(requested_pgn);
            if (it != responders_.end()) {
                auto response_data = it->second();
                if (!response_data.empty()) {
                    // Response goes back to requester if PDU1, or broadcast if PDU2
                    u8 pf = (requested_pgn >> 8) & 0xFF;
                    if (pf < 240 && msg.source != BROADCAST_ADDRESS) {
                        ControlFunction dest_cf;
                        dest_cf.address = msg.source;
                        net_.send(requested_pgn, response_data, cf_, &dest_cf, Priority::Default);
                    } else {
                        net_.send(requested_pgn, response_data, cf_, nullptr, Priority::Default);
                    }
                }
            } else {
                // Send NACK for unknown PGN (only if addressed to us specifically)
                if (msg.destination != BROADCAST_ADDRESS) {
                    echo::category("isobus.protocol.pgn_req").debug("NACK: no responder for pgn=", requested_pgn);
                    send_ack(AckControl::NegativeAck, requested_pgn, msg.source);
                }
            }
        }

        void handle_ack(const Message &msg) {
            if (msg.data.size() < 8)
                return;

            auto ack = Acknowledgment::decode(msg.data);

            echo::category("isobus.protocol.pgn_req")
                .debug("ACK received: type=", static_cast<u8>(ack.control), " pgn=", ack.acknowledged_pgn);
            on_ack_received.emit(ack);
        }
    };
} // namespace agrobus::j1939
