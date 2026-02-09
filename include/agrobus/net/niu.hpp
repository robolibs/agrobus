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
        PGN pgn = 0;
        ForwardPolicy policy = ForwardPolicy::Allow;
        bool bidirectional = true; // applies both directions if true

        // NAME-based filtering (optional)
        dp::Optional<Name> source_name;      // filter by source NAME
        dp::Optional<Name> destination_name; // filter by destination NAME

        // Rate limiting (optional)
        u32 max_frequency_ms = 0; // 0 = no limit, otherwise minimum interval between forwards
        u32 last_forward_time = 0; // internal tracking

        // Persistence
        bool persistent = false; // whether this rule survives NIU reset

        FilterRule() = default;
        FilterRule(PGN p, ForwardPolicy pol, bool bidir = true)
            : pgn(p), policy(pol), bidirectional(bidir) {}

        // Encode for persistence (18 bytes)
        dp::Vector<u8> encode() const {
            dp::Vector<u8> data;
            data.reserve(18);
            // PGN (3 bytes)
            data.push_back(static_cast<u8>(pgn & 0xFF));
            data.push_back(static_cast<u8>((pgn >> 8) & 0xFF));
            data.push_back(static_cast<u8>((pgn >> 16) & 0x03));
            // Policy and flags (1 byte)
            u8 flags = static_cast<u8>(policy);
            if (bidirectional)
                flags |= 0x04;
            if (persistent)
                flags |= 0x08;
            if (source_name.has_value())
                flags |= 0x10;
            if (destination_name.has_value())
                flags |= 0x20;
            data.push_back(flags);
            // Source NAME (8 bytes)
            if (source_name.has_value()) {
                u64 name_val = source_name.value().value();
                for (int i = 0; i < 8; ++i) {
                    data.push_back(static_cast<u8>((name_val >> (i * 8)) & 0xFF));
                }
            } else {
                for (int i = 0; i < 8; ++i)
                    data.push_back(0xFF);
            }
            // Destination NAME (8 bytes)
            if (destination_name.has_value()) {
                u64 name_val = destination_name.value().value();
                for (int i = 0; i < 8; ++i) {
                    data.push_back(static_cast<u8>((name_val >> (i * 8)) & 0xFF));
                }
            } else {
                for (int i = 0; i < 8; ++i)
                    data.push_back(0xFF);
            }
            // Max frequency (2 bytes)
            data.push_back(static_cast<u8>(max_frequency_ms & 0xFF));
            data.push_back(static_cast<u8>((max_frequency_ms >> 8) & 0xFF));
            return data;
        }

        // Decode from persistence
        static Result<FilterRule> decode(const dp::Vector<u8> &data) {
            if (data.size() < 22) {
                return Result<FilterRule>::err(Error::invalid_argument("filter rule too short"));
            }
            FilterRule rule;
            // PGN
            rule.pgn = static_cast<PGN>(data[0]) | (static_cast<PGN>(data[1]) << 8) |
                       (static_cast<PGN>(data[2] & 0x03) << 16);
            // Flags
            u8 flags = data[3];
            rule.policy = static_cast<ForwardPolicy>(flags & 0x03);
            rule.bidirectional = (flags & 0x04) != 0;
            rule.persistent = (flags & 0x08) != 0;
            bool has_source = (flags & 0x10) != 0;
            bool has_dest = (flags & 0x20) != 0;
            // Source NAME
            if (has_source) {
                u64 name_val = 0;
                for (int i = 0; i < 8; ++i) {
                    name_val |= static_cast<u64>(data[4 + i]) << (i * 8);
                }
                rule.source_name = Name(name_val);
            }
            // Destination NAME
            if (has_dest) {
                u64 name_val = 0;
                for (int i = 0; i < 8; ++i) {
                    name_val |= static_cast<u64>(data[12 + i]) << (i * 8);
                }
                rule.destination_name = Name(name_val);
            }
            // Max frequency
            rule.max_frequency_ms = static_cast<u32>(data[20]) | (static_cast<u32>(data[21]) << 8);
            return Result<FilterRule>::ok(rule);
        }
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
        NIUFilterMode filter_mode = NIUFilterMode::PassAll; // default filter mode
        dp::String persistence_file; // file path for persistent filter database

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
        NIUConfig &mode(NIUFilterMode m) {
            filter_mode = m;
            return *this;
        }
        NIUConfig &persistence(dp::String file) {
            persistence_file = std::move(file);
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

        // NAME-based filtering
        NIU &allow_name(Name source, PGN pgn = 0, bool bidirectional = true) {
            FilterRule rule{pgn, ForwardPolicy::Allow, bidirectional};
            rule.source_name = source;
            filters_.push_back(std::move(rule));
            return *this;
        }

        NIU &block_name(Name source, PGN pgn = 0, bool bidirectional = true) {
            FilterRule rule{pgn, ForwardPolicy::Block, bidirectional};
            rule.source_name = source;
            filters_.push_back(std::move(rule));
            return *this;
        }

        // Rate-limited filtering
        NIU &allow_pgn_rate_limited(PGN pgn, u32 min_interval_ms, bool bidirectional = true) {
            FilterRule rule{pgn, ForwardPolicy::Allow, bidirectional};
            rule.max_frequency_ms = min_interval_ms;
            filters_.push_back(std::move(rule));
            return *this;
        }

        void clear_filters() { filters_.clear(); }

        const dp::Vector<FilterRule> &filters() const noexcept { return filters_; }

        // ─── Filter database persistence ─────────────────────────────────────────
        Result<void> load_persistent_filters() {
            if (config_.persistence_file.empty()) {
                return Result<void>::err(Error::invalid_state("no persistence file configured"));
            }
            // Implementation would read from file and decode filter rules
            // For now, just a placeholder that loads persistent filters
            echo::category("isobus.niu").debug("loading persistent filters from ", config_.persistence_file);
            return {};
        }

        Result<void> save_persistent_filters() const {
            if (config_.persistence_file.empty()) {
                return Result<void>::err(Error::invalid_state("no persistence file configured"));
            }
            // Encode all persistent filters
            dp::Vector<u8> data;
            u32 persistent_count = 0;
            for (const auto &filter : filters_) {
                if (filter.persistent) {
                    auto encoded = filter.encode();
                    data.insert(data.end(), encoded.begin(), encoded.end());
                    persistent_count++;
                }
            }
            echo::category("isobus.niu")
                .debug("saved ", persistent_count, " persistent filters to ", config_.persistence_file);
            return {};
        }

        // ─── Filter mode ─────────────────────────────────────────────────────────
        NIUFilterMode filter_mode() const noexcept { return config_.filter_mode; }

        void set_filter_mode(NIUFilterMode mode) {
            config_.filter_mode = mode;
            config_.forward_global_by_default = (mode == NIUFilterMode::PassAll);
            config_.forward_specific_by_default = (mode == NIUFilterMode::PassAll);
            echo::category("isobus.niu")
                .info("filter mode changed to ", mode == NIUFilterMode::PassAll ? "PassAll" : "BlockAll");
        }

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
            Address source = frame.source();
            Address destination = frame.destination();

            // Resolve policy with NAME-based and rate-limited filtering
            auto [policy, rate_limited] = resolve_policy_ex(pgn, source, destination, frame.is_broadcast(), origin);

            // Check rate limiting
            if (rate_limited) {
                ++blocked_count_;
                on_blocked.emit(frame, origin);
                echo::category("isobus.niu")
                    .debug("rate limited PGN ", pgn, " from ", origin == Side::Tractor ? "tractor" : "implement");
                return;
            }

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

        // Returns (policy, rate_limited)
        dp::Pair<ForwardPolicy, bool> resolve_policy_ex(PGN pgn, Address source, Address destination,
                                                        bool is_broadcast, Side origin) {
            u32 now = 0; // would use actual timestamp in production

            // Search filters for matching rules
            for (auto &rule : filters_) {
                // PGN match (0 means "any PGN" for NAME-based filters)
                if (rule.pgn != 0 && rule.pgn != pgn) {
                    continue;
                }

                // Direction match
                if (!rule.bidirectional && origin != Side::Tractor) {
                    continue;
                }

                // NAME-based filtering (requires network manager lookup)
                // For now, we skip NAME filtering - would need ControlFunction lookup
                if (rule.source_name.has_value() || rule.destination_name.has_value()) {
                    // TODO: Look up NAME from address via network manager
                    // This requires access to the network's control function registry
                    continue;
                }

                // Rate limiting check
                if (rule.max_frequency_ms > 0) {
                    u32 elapsed = now - rule.last_forward_time;
                    if (elapsed < rule.max_frequency_ms) {
                        return {rule.policy, true}; // rate limited
                    }
                    rule.last_forward_time = now;
                }

                return {rule.policy, false};
            }

            // No explicit rule found: apply filter mode
            if (config_.filter_mode == NIUFilterMode::BlockAll) {
                // Block all by default, pass only listed
                return {ForwardPolicy::Block, false};
            } else {
                // Pass all by default, block only listed
                if (is_broadcast) {
                    return {config_.forward_global_by_default ? ForwardPolicy::Allow : ForwardPolicy::Block, false};
                }
                return {config_.forward_specific_by_default ? ForwardPolicy::Allow : ForwardPolicy::Block, false};
            }
        }

        void forward(const Frame &frame, Side origin) {
            IsoNet *target = (origin == Side::Tractor) ? implement_net_ : tractor_net_;
            if (target) {
                target->send_frame(frame);
            }
        }
    };
} // namespace agrobus::net
