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
#include <functional>

namespace agrobus::isobus {
    using namespace agrobus::net;

    // ─── Group Function Protocol (ISO 11783-3 Section 5.4.6) ──────────────────────
    // Group Functions provide a standardized mechanism for grouping related commands.
    // They use the Acknowledgment PGN with group_function field to identify the operation.

    enum class GroupFunctionType : u8 {
        Request = 0,     // Request group function values
        Command = 1,     // Command group function values
        Acknowledge = 2, // Acknowledge group function
        ReadReply = 3,   // Read reply
        Reserved = 0xFF
    };

    struct GroupFunctionMsg {
        PGN target_pgn = 0;
        GroupFunctionType function_type = GroupFunctionType::Request;
        dp::Vector<u8> parameters; // Function-specific parameter data

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = static_cast<u8>(function_type);
            data[1] = static_cast<u8>(target_pgn & 0xFF);
            data[2] = static_cast<u8>((target_pgn >> 8) & 0xFF);
            data[3] = static_cast<u8>((target_pgn >> 16) & 0xFF);
            for (usize i = 0; i < parameters.size() && i < 4; ++i) {
                data[4 + i] = parameters[i];
            }
            return data;
        }

        static GroupFunctionMsg decode(const dp::Vector<u8> &data) {
            GroupFunctionMsg msg;
            if (data.size() >= 4) {
                msg.function_type = static_cast<GroupFunctionType>(data[0]);
                msg.target_pgn =
                    static_cast<PGN>(data[1]) | (static_cast<PGN>(data[2]) << 8) | (static_cast<PGN>(data[3]) << 16);
                for (usize i = 4; i < data.size() && data[i] != 0xFF; ++i) {
                    msg.parameters.push_back(data[i]);
                }
            }
            return msg;
        }
    };

    // Group function error codes for acknowledgment
    enum class GroupFunctionError : u8 {
        NoError = 0,
        UnsupportedPGN = 1,
        UnsupportedFunction = 2,
        InvalidParameter = 3,
        PermissionDenied = 4,
        Busy = 5
    };

    // ─── Group Function Handler ──────────────────────────────────────────────────
    class GroupFunctionHandler {
        IsoNet &net_;
        InternalCF *cf_;

        using HandlerFn = std::function<Result<dp::Vector<u8>>(const GroupFunctionMsg &)>;
        dp::Map<PGN, HandlerFn> handlers_;

      public:
        GroupFunctionHandler(IsoNet &net, InternalCF *cf) : net_(net), cf_(cf) {}

        Result<void> initialize() {
            if (!cf_) {
                return Result<void>::err(Error::invalid_state("control function not set"));
            }
            echo::category("isobus.protocol.group_fn").debug("initialized");
            return {};
        }

        // Register a handler for group function messages targeting a specific PGN
        Result<void> register_handler(PGN pgn, HandlerFn handler) {
            if (!handler) {
                return Result<void>::err(Error::invalid_state("null handler"));
            }
            handlers_[pgn] = std::move(handler);
            return {};
        }

        // Send a group function request
        Result<void> send_request(PGN target_pgn, const dp::Vector<u8> &params = {},
                                  Address destination = BROADCAST_ADDRESS) {
            GroupFunctionMsg msg;
            msg.function_type = GroupFunctionType::Request;
            msg.target_pgn = target_pgn;
            msg.parameters = params;
            return send_group_function(msg, destination);
        }

        // Send a group function command
        Result<void> send_command(PGN target_pgn, const dp::Vector<u8> &params = {},
                                  Address destination = BROADCAST_ADDRESS) {
            GroupFunctionMsg msg;
            msg.function_type = GroupFunctionType::Command;
            msg.target_pgn = target_pgn;
            msg.parameters = params;
            return send_group_function(msg, destination);
        }

        // Process an incoming group function message
        void handle_message(const Message &msg) {
            if (msg.data.size() < 4)
                return;

            auto gf = GroupFunctionMsg::decode(msg.data);
            echo::category("isobus.protocol.group_fn")
                .debug("received: type=", static_cast<u8>(gf.function_type), " pgn=", gf.target_pgn,
                       " from=", msg.source);

            on_group_function.emit(gf, msg.source);

            auto it = handlers_.find(gf.target_pgn);
            if (it != handlers_.end()) {
                auto result = it->second(gf);
                if (result.is_ok() && !result.value().empty()) {
                    ControlFunction dest_cf;
                    dest_cf.address = msg.source;
                    GroupFunctionMsg reply;
                    reply.function_type = GroupFunctionType::Acknowledge;
                    reply.target_pgn = gf.target_pgn;
                    reply.parameters = result.value();
                    net_.send(PGN_ACKNOWLEDGMENT, reply.encode(), cf_, &dest_cf);
                }
            }
        }

        // Events
        Event<GroupFunctionMsg, Address> on_group_function;

      private:
        Result<void> send_group_function(const GroupFunctionMsg &msg, Address destination) {
            if (destination == BROADCAST_ADDRESS) {
                return net_.send(PGN_ACKNOWLEDGMENT, msg.encode(), cf_, nullptr, Priority::Default);
            }
            ControlFunction dest_cf;
            dest_cf.address = destination;
            return net_.send(PGN_ACKNOWLEDGMENT, msg.encode(), cf_, &dest_cf, Priority::Default);
        }
    };
} // namespace agrobus::isobus
