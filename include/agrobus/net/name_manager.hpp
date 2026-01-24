#pragma once

#include "internal_cf.hpp"
#include "network_manager.hpp"
#include <agrobus/net/constants.hpp>
#include <agrobus/net/error.hpp>
#include <agrobus/net/event.hpp>
#include <agrobus/net/message.hpp>
#include <agrobus/net/types.hpp>
#include <datapod/datapod.hpp>
#include <echo/echo.hpp>

namespace agrobus::net {

    // ─── NAME Management Protocol (ISO 11783-5 Section 4.4.3) ─────────────────────
    // Provides runtime NAME modification, pending NAME storage, and NAME queries.
    // PGN: 37632 (0x9300), destination-specific, Priority 6
    // Also handles Commanded Address PGN (0xFED8) for address changes.

    enum class NameMgmtMode : u8 {
        SetPending = 0,
        RequestPendingResponse = 1,
        RequestCurrentResponse = 2,
        Acknowledge = 3,
        NegativeAcknowledge = 4,
        RequestPending = 5,
        RequestCurrent = 6,
        AdoptPending = 7,
        RequestAddressClaim = 8
    };

    enum class NameNackReason : u8 {
        Security = 0,
        InvalidItems = 1,
        Conflict = 2,
        Checksum = 3,
        PendingNotSet = 4,
        Other = 5
    };

    struct NameManagementMsg {
        NameMgmtMode mode = NameMgmtMode::RequestCurrent;
        dp::Array<u8, 8> name_data{}; // 8-byte NAME field
        NameNackReason nack_reason = NameNackReason::Other;

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(9 + 8, 0xFF);
            data[0] = static_cast<u8>(mode);
            for (usize i = 0; i < 8; ++i) {
                data[1 + i] = name_data[i];
            }
            if (mode == NameMgmtMode::NegativeAcknowledge) {
                data[9] = static_cast<u8>(nack_reason);
            }
            return data;
        }

        static NameManagementMsg decode(const dp::Vector<u8> &data) {
            NameManagementMsg msg;
            if (data.size() >= 9) {
                msg.mode = static_cast<NameMgmtMode>(data[0]);
                for (usize i = 0; i < 8 && (i + 1) < data.size(); ++i) {
                    msg.name_data[i] = data[1 + i];
                }
                if (msg.mode == NameMgmtMode::NegativeAcknowledge && data.size() > 9) {
                    msg.nack_reason = static_cast<NameNackReason>(data[9]);
                }
            }
            return msg;
        }
    };

    // ─── NAME Manager ─────────────────────────────────────────────────────────────
    class NameManager {
        IsoNet &net_;
        InternalCF *cf_;
        dp::Optional<Name> pending_name_;
        bool has_pending_ = false;

      public:
        NameManager(IsoNet &net, InternalCF *cf) : net_(net), cf_(cf) {}

        Result<void> initialize() {
            if (!cf_) {
                return Result<void>::err(Error::invalid_state("control function not set"));
            }
            // Register for NAME Management (PGN 37632 / 0x9300)
            net_.register_pgn_callback(PGN_NAME_MANAGEMENT,
                                       [this](const Message &msg) { handle_name_management(msg); });
            // Register for Commanded Address (transport protocol, >8 bytes)
            net_.register_pgn_callback(PGN_COMMANDED_ADDRESS,
                                       [this](const Message &msg) { handle_commanded_address(msg); });
            echo::category("isobus.network.name_mgr").debug("initialized");
            return {};
        }

        // Set a pending NAME that can later be adopted
        Result<void> set_pending(Name name) {
            // Identity number must not change (ISO 11783-5 requirement)
            if (name.identity_number() != cf_->name().identity_number()) {
                return Result<void>::err(Error::invalid_state("identity_number must not change"));
            }
            pending_name_ = name;
            has_pending_ = true;
            echo::category("isobus.network.name_mgr").debug("pending NAME set");
            return {};
        }

        // Adopt the pending NAME (triggers re-claim with new NAME)
        Result<void> adopt_pending() {
            if (!has_pending_) {
                return Result<void>::err(Error::invalid_state("no pending NAME set"));
            }
            auto new_name = *pending_name_;
            has_pending_ = false;
            pending_name_ = dp::nullopt;

            echo::category("isobus.network.name_mgr").info("adopting pending NAME, re-claiming");
            cf_->set_name(new_name);
            on_name_changed.emit(new_name);
            // Caller must trigger re-claim after NAME change
            return {};
        }

        // Query pending NAME
        bool has_pending() const noexcept { return has_pending_; }
        dp::Optional<Name> pending_name() const noexcept { return pending_name_; }

        // Get current NAME
        Name current_name() const noexcept { return cf_->name(); }

        // Handle an incoming NAME management message
        void handle_name_management(const Message &msg) {
            if (msg.data.size() < 9)
                return;

            auto nm_msg = NameManagementMsg::decode(msg.data);
            echo::category("isobus.network.name_mgr")
                .debug("received mode=", static_cast<u8>(nm_msg.mode), " from=", msg.source);

            switch (nm_msg.mode) {
            case NameMgmtMode::SetPending: {
                Name name;
                name.raw = 0;
                for (usize i = 0; i < 8; ++i) {
                    name.raw |= static_cast<u64>(nm_msg.name_data[i]) << (i * 8);
                }
                auto result = set_pending(name);
                if (result.is_ok()) {
                    send_ack(msg.source);
                } else {
                    send_nack(msg.source, NameNackReason::InvalidItems);
                }
                break;
            }
            case NameMgmtMode::RequestCurrent:
            case NameMgmtMode::RequestCurrentResponse: {
                send_current_response(msg.source);
                break;
            }
            case NameMgmtMode::RequestPending:
            case NameMgmtMode::RequestPendingResponse: {
                if (has_pending_) {
                    send_pending_response(msg.source);
                } else {
                    send_nack(msg.source, NameNackReason::PendingNotSet);
                }
                break;
            }
            case NameMgmtMode::AdoptPending: {
                auto result = adopt_pending();
                if (result.is_ok()) {
                    send_ack(msg.source);
                } else {
                    send_nack(msg.source, NameNackReason::PendingNotSet);
                }
                break;
            }
            default:
                break;
            }

            on_name_management.emit(nm_msg, msg.source);
        }

        // Events
        Event<Name> on_name_changed;
        Event<NameManagementMsg, Address> on_name_management;
        Event<Address> on_commanded_address; // Emitted when address change is commanded

      private:
        void handle_commanded_address(const Message &msg) {
            // Commanded Address message: 9 bytes (8 NAME + 1 new address)
            if (msg.data.size() < 9)
                return;

            // Check if this command targets our NAME
            u64 target_name_raw = 0;
            for (usize i = 0; i < 8; ++i) {
                target_name_raw |= static_cast<u64>(msg.data[i]) << (i * 8);
            }

            if (target_name_raw != cf_->name().raw) {
                return; // Not for us
            }

            Address new_address = msg.data[8];
            if (new_address > MAX_ADDRESS) {
                echo::category("isobus.network.name_mgr").warn("commanded address invalid: ", new_address);
                return;
            }

            echo::category("isobus.network.name_mgr")
                .info("commanded address change: ", cf_->address(), " -> ", new_address);

            cf_->set_address(new_address);
            on_commanded_address.emit(new_address);
            // Caller should trigger address claim with new address
        }

        void send_ack(Address destination) {
            NameManagementMsg reply;
            reply.mode = NameMgmtMode::Acknowledge;
            auto name_bytes = cf_->name().to_bytes();
            for (usize i = 0; i < 8; ++i) {
                reply.name_data[i] = name_bytes[i];
            }
            ControlFunction dest_cf;
            dest_cf.address = destination;
            net_.send(PGN_NAME_MANAGEMENT, reply.encode(), cf_, &dest_cf, Priority::Default);
        }

        void send_nack(Address destination, NameNackReason reason) {
            NameManagementMsg reply;
            reply.mode = NameMgmtMode::NegativeAcknowledge;
            reply.nack_reason = reason;
            auto name_bytes = cf_->name().to_bytes();
            for (usize i = 0; i < 8; ++i) {
                reply.name_data[i] = name_bytes[i];
            }
            ControlFunction dest_cf;
            dest_cf.address = destination;
            net_.send(PGN_NAME_MANAGEMENT, reply.encode(), cf_, &dest_cf, Priority::Default);
        }

        void send_current_response(Address destination) {
            NameManagementMsg reply;
            reply.mode = NameMgmtMode::RequestCurrentResponse;
            auto name_bytes = cf_->name().to_bytes();
            for (usize i = 0; i < 8; ++i) {
                reply.name_data[i] = name_bytes[i];
            }
            ControlFunction dest_cf;
            dest_cf.address = destination;
            net_.send(PGN_NAME_MANAGEMENT, reply.encode(), cf_, &dest_cf, Priority::Default);
        }

        void send_pending_response(Address destination) {
            NameManagementMsg reply;
            reply.mode = NameMgmtMode::RequestPendingResponse;
            auto pending_bytes = pending_name_->to_bytes();
            for (usize i = 0; i < 8; ++i) {
                reply.name_data[i] = pending_bytes[i];
            }
            ControlFunction dest_cf;
            dest_cf.address = destination;
            net_.send(PGN_NAME_MANAGEMENT, reply.encode(), cf_, &dest_cf, Priority::Default);
        }
    };
} // namespace agrobus::net
