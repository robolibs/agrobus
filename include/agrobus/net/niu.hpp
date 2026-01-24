#pragma once

#include <agrobus/net/constants.hpp>
#include <agrobus/net/error.hpp>
#include <agrobus/net/event.hpp>
#include <agrobus/net/frame.hpp>
#include <agrobus/net/identifier.hpp>
#include <agrobus/net/message.hpp>
#include <agrobus/net/network_manager.hpp>
#include <agrobus/net/state_machine.hpp>
#include <agrobus/net/types.hpp>
#include <datapod/datapod.hpp>
#include <echo/echo.hpp>

namespace agrobus::net {

    // ─── Forward policy ──────────────────────────────────────────────────────────
    enum class ForwardPolicy : u8 {
        Allow,  // Forward this PGN
        Block,  // Block this PGN
        Monitor // Forward but also emit event
    };

    // ─── Filter rule ─────────────────────────────────────────────────────────────
    struct FilterRule {
        PGN pgn;
        ForwardPolicy policy = ForwardPolicy::Allow;
        bool bidirectional = true; // applies both directions if true
    };

    // ─── NIU state ───────────────────────────────────────────────────────────────
    enum class NIUState : u8 { Inactive, Active, Error };

    // ─── Side identifier ─────────────────────────────────────────────────────────
    enum class Side : u8 { Tractor, Implement };

    // ─── NIU Network Message Function Codes (ISO 11783-4, Section 6.5) ──────────
    enum class NIUFunction : u8 {
        RequestFilterDB = 1,
        AddFilterEntry = 2,
        DeleteFilterEntry = 3,
        DeleteAllEntries = 4,
        RequestFilterMode = 5,
        SetFilterMode = 6,
        RequestPortConfig = 9,
        PortConfigResponse = 10,
        FilterDBResponse = 11,
        RequestPortStats = 12,
        PortStatsResponse = 13,
        OpenConnection = 14,
        CloseConnection = 15
    };

    // ─── Filter mode (ISO 11783-4, Section 6.5.6) ──────────────────────────────
    enum class NIUFilterMode : u8 {
        BlockAll = 0, // Block all, pass only listed PGNs
        PassAll = 1   // Pass all, block only listed PGNs
    };

    // ─── NIU Network Message ─────────────────────────────────────────────────────
    struct NIUNetworkMsg {
        NIUFunction function = NIUFunction::RequestFilterDB;
        u8 port_number = 0;
        PGN filter_pgn = 0;
        NIUFilterMode filter_mode = NIUFilterMode::PassAll;
        u32 msgs_forwarded = 0;
        u32 msgs_blocked = 0;

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = static_cast<u8>(function);
            data[1] = port_number;
            switch (function) {
            case NIUFunction::AddFilterEntry:
            case NIUFunction::DeleteFilterEntry:
            case NIUFunction::FilterDBResponse:
                data[2] = static_cast<u8>(filter_pgn & 0xFF);
                data[3] = static_cast<u8>((filter_pgn >> 8) & 0xFF);
                data[4] = static_cast<u8>((filter_pgn >> 16) & 0x03);
                break;
            case NIUFunction::SetFilterMode:
            case NIUFunction::RequestFilterMode:
                data[2] = static_cast<u8>(filter_mode);
                break;
            case NIUFunction::PortStatsResponse:
                data[2] = static_cast<u8>(msgs_forwarded & 0xFF);
                data[3] = static_cast<u8>((msgs_forwarded >> 8) & 0xFF);
                data[4] = static_cast<u8>(msgs_blocked & 0xFF);
                data[5] = static_cast<u8>((msgs_blocked >> 8) & 0xFF);
                break;
            default:
                break;
            }
            return data;
        }

        static NIUNetworkMsg decode(const dp::Vector<u8> &data) {
            NIUNetworkMsg msg;
            if (data.size() < 2)
                return msg;
            msg.function = static_cast<NIUFunction>(data[0]);
            msg.port_number = data[1];
            switch (msg.function) {
            case NIUFunction::AddFilterEntry:
            case NIUFunction::DeleteFilterEntry:
            case NIUFunction::FilterDBResponse:
                if (data.size() >= 5) {
                    msg.filter_pgn = static_cast<PGN>(data[2]) | (static_cast<PGN>(data[3]) << 8) |
                                     (static_cast<PGN>(data[4] & 0x03) << 16);
                }
                break;
            case NIUFunction::SetFilterMode:
            case NIUFunction::RequestFilterMode:
                if (data.size() >= 3) {
                    msg.filter_mode = static_cast<NIUFilterMode>(data[2]);
                }
                break;
            case NIUFunction::PortStatsResponse:
                if (data.size() >= 6) {
                    msg.msgs_forwarded = static_cast<u32>(data[2]) | (static_cast<u32>(data[3]) << 8);
                    msg.msgs_blocked = static_cast<u32>(data[4]) | (static_cast<u32>(data[5]) << 8);
                }
                break;
            default:
                break;
            }
            return msg;
        }
    };

    // ─── NIU configuration ───────────────────────────────────────────────────────
    struct NIUConfig {
        dp::String name = "NIU";
        bool forward_global_by_default = true;   // forward broadcast PGNs not in filter
        bool forward_specific_by_default = true; // forward destination-specific PGNs not in filter

        NIUConfig &set_name(dp::String n) {
            name = std::move(n);
            return *this;
        }
        NIUConfig &global_default(bool allow) {
            forward_global_by_default = allow;
            return *this;
        }
        NIUConfig &specific_default(bool allow) {
            forward_specific_by_default = allow;
            return *this;
        }
    };

    // ─── Network Interconnect Unit (ISO 11783-4) ─────────────────────────────────
    // Routes CAN frames between tractor-side and implement-side networks.
    class NIU {
        IsoNet *tractor_net_ = nullptr;
        IsoNet *implement_net_ = nullptr;
        dp::Vector<FilterRule> filters_;
        NIUConfig config_;
        StateMachine<NIUState> state_{NIUState::Inactive};
        u32 forwarded_count_ = 0;
        u32 blocked_count_ = 0;

      public:
        explicit NIU(NIUConfig config = {}) : config_(std::move(config)) {}

        // ─── Attach networks ─────────────────────────────────────────────────────
        Result<void> attach_tractor(IsoNet *net) {
            if (!net) {
                return Result<void>::err(Error::invalid_state("null tractor network"));
            }
            tractor_net_ = net;
            echo::category("isobus.niu").debug("tractor network attached");
            return {};
        }

        Result<void> attach_implement(IsoNet *net) {
            if (!net) {
                return Result<void>::err(Error::invalid_state("null implement network"));
            }
            implement_net_ = net;
            echo::category("isobus.niu").debug("implement network attached");
            return {};
        }

        // ─── Filter management ───────────────────────────────────────────────────
        NIU &add_filter(FilterRule rule) {
            filters_.push_back(std::move(rule));
            return *this;
        }

        NIU &allow_pgn(PGN pgn, bool bidirectional = true) {
            filters_.push_back(FilterRule{pgn, ForwardPolicy::Allow, bidirectional});
            return *this;
        }

        NIU &block_pgn(PGN pgn, bool bidirectional = true) {
            filters_.push_back(FilterRule{pgn, ForwardPolicy::Block, bidirectional});
            return *this;
        }

        NIU &monitor_pgn(PGN pgn, bool bidirectional = true) {
            filters_.push_back(FilterRule{pgn, ForwardPolicy::Monitor, bidirectional});
            return *this;
        }

        void clear_filters() { filters_.clear(); }

        // ─── Start/stop ──────────────────────────────────────────────────────────
        Result<void> start() {
            if (!tractor_net_ || !implement_net_) {
                state_.transition(NIUState::Error);
                return Result<void>::err(Error::invalid_state("both networks must be attached before starting"));
            }
            state_.transition(NIUState::Active);
            echo::category("isobus.niu").info("NIU '", config_.name, "' started");
            return {};
        }

        void stop() {
            state_.transition(NIUState::Inactive);
            echo::category("isobus.niu").info("NIU '", config_.name, "' stopped");
        }

        // ─── Frame processing ────────────────────────────────────────────────────
        // Called when a frame arrives on the tractor side
        void process_tractor_frame(const Frame &frame) { process_frame(frame, Side::Tractor); }

        // Called when a frame arrives on the implement side
        void process_implement_frame(const Frame &frame) { process_frame(frame, Side::Implement); }

        // ─── Statistics ──────────────────────────────────────────────────────────
        u32 forwarded() const noexcept { return forwarded_count_; }
        u32 blocked() const noexcept { return blocked_count_; }
        NIUState state() const noexcept { return state_.state(); }

        // ─── NIU Network Message handling (PGN 0xED00) ──────────────────────────
        void handle_niu_message(const Message &msg) {
            if (msg.data.size() < 2)
                return;
            auto niu_msg = NIUNetworkMsg::decode(msg.data);
            echo::category("isobus.niu")
                .debug("NIU msg func=", static_cast<u8>(niu_msg.function), " port=", niu_msg.port_number);

            switch (niu_msg.function) {
            case NIUFunction::AddFilterEntry:
                filters_.push_back(FilterRule{niu_msg.filter_pgn, ForwardPolicy::Allow, true});
                on_niu_message.emit(niu_msg, msg.source);
                break;
            case NIUFunction::DeleteFilterEntry:
                for (auto it = filters_.begin(); it != filters_.end(); ++it) {
                    if (it->pgn == niu_msg.filter_pgn) {
                        filters_.erase(it);
                        break;
                    }
                }
                on_niu_message.emit(niu_msg, msg.source);
                break;
            case NIUFunction::DeleteAllEntries:
                filters_.clear();
                on_niu_message.emit(niu_msg, msg.source);
                break;
            case NIUFunction::SetFilterMode:
                config_.forward_global_by_default = (niu_msg.filter_mode == NIUFilterMode::PassAll);
                config_.forward_specific_by_default = (niu_msg.filter_mode == NIUFilterMode::PassAll);
                on_niu_message.emit(niu_msg, msg.source);
                break;
            case NIUFunction::RequestPortStats: {
                NIUNetworkMsg reply;
                reply.function = NIUFunction::PortStatsResponse;
                reply.port_number = niu_msg.port_number;
                reply.msgs_forwarded = forwarded_count_;
                reply.msgs_blocked = blocked_count_;
                on_niu_message.emit(reply, msg.source);
                break;
            }
            default:
                on_niu_message.emit(niu_msg, msg.source);
                break;
            }
        }

        // ─── Events ──────────────────────────────────────────────────────────────
        Event<Frame, Side> on_forwarded;              // frame, which side it came from
        Event<Frame, Side> on_blocked;                // frame, which side it came from
        Event<Frame, Side> on_monitored;              // frame forwarded but also reported
        Event<NIUNetworkMsg, Address> on_niu_message; // NIU protocol messages

      private:
        void process_frame(const Frame &frame, Side origin) {
            if (!state_.is(NIUState::Active)) {
                return;
            }

            PGN pgn = frame.pgn();
            ForwardPolicy policy = resolve_policy(pgn, frame.is_broadcast(), origin);

            switch (policy) {
            case ForwardPolicy::Allow:
                forward(frame, origin);
                ++forwarded_count_;
                on_forwarded.emit(frame, origin);
                break;

            case ForwardPolicy::Block:
                ++blocked_count_;
                on_blocked.emit(frame, origin);
                echo::category("isobus.niu")
                    .debug("blocked PGN ", pgn, " from ", origin == Side::Tractor ? "tractor" : "implement");
                break;

            case ForwardPolicy::Monitor:
                forward(frame, origin);
                ++forwarded_count_;
                on_forwarded.emit(frame, origin);
                on_monitored.emit(frame, origin);
                echo::category("isobus.niu")
                    .debug("monitored PGN ", pgn, " from ", origin == Side::Tractor ? "tractor" : "implement");
                break;
            }
        }

        ForwardPolicy resolve_policy(PGN pgn, bool is_broadcast, Side origin) const {
            // Search filters for a matching rule
            for (const auto &rule : filters_) {
                if (rule.pgn != pgn) {
                    continue;
                }
                // If bidirectional, always match; otherwise only match tractor-originated
                if (rule.bidirectional || origin == Side::Tractor) {
                    return rule.policy;
                }
            }

            // No explicit rule found: apply default policy
            if (is_broadcast) {
                return config_.forward_global_by_default ? ForwardPolicy::Allow : ForwardPolicy::Block;
            }
            return config_.forward_specific_by_default ? ForwardPolicy::Allow : ForwardPolicy::Block;
        }

        void forward(const Frame &frame, Side origin) {
            IsoNet *target = (origin == Side::Tractor) ? implement_net_ : tractor_net_;
            if (target) {
                target->send_frame(frame);
            }
        }
    };
} // namespace agrobus::net
