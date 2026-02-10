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

namespace agrobus::j1939 {
    using namespace agrobus::net;

    // ─── Request2 Protocol (ISO 11783-3 Section 5.4.7) ────────────────────────────
    // Request2 allows requesting a PGN with extended identifiers, and optionally
    // requesting the response via Transfer PGN instead of the normal PGN response.

    struct Request2Msg {
        PGN requested_pgn = 0;
        dp::Vector<u8> extended_id; // 1-3 bytes of extended identifier
        bool use_transfer = false;  // If true, response should use Transfer PGN

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = static_cast<u8>(requested_pgn & 0xFF);
            data[1] = static_cast<u8>((requested_pgn >> 8) & 0xFF);
            data[2] = static_cast<u8>((requested_pgn >> 16) & 0xFF);
            data[3] = use_transfer ? 0x01 : 0x00;
            for (usize i = 0; i < extended_id.size() && i < 3; ++i) {
                data[4 + i] = extended_id[i];
            }
            return data;
        }

        static Request2Msg decode(const dp::Vector<u8> &data) {
            Request2Msg msg;
            if (data.size() >= 4) {
                msg.requested_pgn =
                    static_cast<PGN>(data[0]) | (static_cast<PGN>(data[1]) << 8) | (static_cast<PGN>(data[2]) << 16);
                msg.use_transfer = (data[3] & 0x01) != 0;
                for (usize i = 4; i < data.size() && i < 7; ++i) {
                    if (data[i] != 0xFF) {
                        msg.extended_id.push_back(data[i]);
                    }
                }
            }
            return msg;
        }
    };

    struct TransferMsg {
        PGN original_pgn = 0; // PGN that was originally requested
        dp::Vector<u8> data;  // Response payload

        dp::Vector<u8> encode() const {
            dp::Vector<u8> out;
            out.push_back(static_cast<u8>(original_pgn & 0xFF));
            out.push_back(static_cast<u8>((original_pgn >> 8) & 0xFF));
            out.push_back(static_cast<u8>((original_pgn >> 16) & 0xFF));
            for (auto b : data) {
                out.push_back(b);
            }
            return out;
        }

        static TransferMsg decode(const dp::Vector<u8> &raw) {
            TransferMsg msg;
            if (raw.size() >= 3) {
                msg.original_pgn =
                    static_cast<PGN>(raw[0]) | (static_cast<PGN>(raw[1]) << 8) | (static_cast<PGN>(raw[2]) << 16);
                for (usize i = 3; i < raw.size(); ++i) {
                    msg.data.push_back(raw[i]);
                }
            }
            return msg;
        }
    };

    // ─── Request2 Protocol Handler ────────────────────────────────────────────────
    class Request2Protocol {
        IsoNet &net_;
        InternalCF *cf_;

        // Registered responders that support extended identifiers
        dp::Map<PGN, std::function<dp::Vector<u8>(const dp::Vector<u8> &extended_id)>> responders_;

      public:
        Request2Protocol(IsoNet &net, InternalCF *cf) : net_(net), cf_(cf) {}

        Result<void> initialize() {
            if (!cf_) {
                return Result<void>::err(Error::invalid_state("control function not set"));
            }
            net_.register_pgn_callback(PGN_REQUEST2, [this](const Message &msg) { handle_request2(msg); });
            net_.register_pgn_callback(PGN_TRANSFER, [this](const Message &msg) { handle_transfer(msg); });
            echo::category("isobus.protocol.request2").debug("initialized");
            return {};
        }

        // Register a responder for Request2 messages targeting a specific PGN
        Result<void> register_responder(PGN pgn, std::function<dp::Vector<u8>(const dp::Vector<u8> &)> responder) {
            if (!responder) {
                return Result<void>::err(Error::invalid_state("null responder"));
            }
            responders_[pgn] = std::move(responder);
            return {};
        }

        // Send a Request2 message
        Result<void> request2(PGN pgn, const dp::Vector<u8> &extended_id = {}, bool use_transfer = false,
                              Address destination = BROADCAST_ADDRESS) {
            Request2Msg req;
            req.requested_pgn = pgn;
            req.extended_id = extended_id;
            req.use_transfer = use_transfer;

            echo::category("isobus.protocol.request2").debug("sending Request2: pgn=", pgn, " transfer=", use_transfer);

            if (destination == BROADCAST_ADDRESS) {
                return net_.send(PGN_REQUEST2, req.encode(), cf_, nullptr, Priority::Default);
            }
            ControlFunction dest_cf;
            dest_cf.address = destination;
            return net_.send(PGN_REQUEST2, req.encode(), cf_, &dest_cf, Priority::Default);
        }

        // Events
        Event<Request2Msg, Address> on_request2_received;
        Event<TransferMsg, Address> on_transfer_received;

      private:
        void handle_request2(const Message &msg) {
            auto req = Request2Msg::decode(msg.data);
            echo::category("isobus.protocol.request2")
                .debug("Request2 received: pgn=", req.requested_pgn, " from=", msg.source);
            on_request2_received.emit(req, msg.source);

            auto it = responders_.find(req.requested_pgn);
            if (it != responders_.end()) {
                auto response_data = it->second(req.extended_id);
                if (!response_data.empty()) {
                    ControlFunction dest_cf;
                    dest_cf.address = msg.source;
                    if (req.use_transfer) {
                        TransferMsg transfer;
                        transfer.original_pgn = req.requested_pgn;
                        transfer.data = std::move(response_data);
                        net_.send(PGN_TRANSFER, transfer.encode(), cf_, &dest_cf, Priority::Default);
                    } else {
                        net_.send(req.requested_pgn, response_data, cf_, &dest_cf, Priority::Default);
                    }
                }
            }
        }

        void handle_transfer(const Message &msg) {
            auto transfer = TransferMsg::decode(msg.data);
            echo::category("isobus.protocol.request2")
                .debug("Transfer received: pgn=", transfer.original_pgn, " from=", msg.source);
            on_transfer_received.emit(transfer, msg.source);
        }
    };
} // namespace agrobus::j1939
