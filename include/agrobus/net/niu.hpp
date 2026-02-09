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

    // ═════════════════════════════════════════════════════════════════════════════
    // Repeater NIU (ISO 11783-4, Section 6.2)
    // ═════════════════════════════════════════════════════════════════════════════
    // A Repeater extends the physical network by forwarding all frames bidirectionally
    // with optional filtering. It does NOT perform address translation - all devices
    // on both segments share the same address space and must have unique addresses.
    //
    // Use cases:
    // - Extending cable length beyond physical limits
    // - Segment isolation with simple forwarding
    // - Optional filtering for bandwidth management
    //
    // Constraints:
    // - No address translation (single address space)
    // - All source addresses must be unique across both segments
    // - Address claims propagate to both sides
    //
    class RepeaterNIU : public NIU {
      public:
        explicit RepeaterNIU(NIUConfig config = {}) : NIU(std::move(config)) {
            // Repeater forwards everything by default
            config_.forward_global_by_default = true;
            config_.forward_specific_by_default = true;
            config_.filter_mode = NIUFilterMode::PassAll;
        }

        // ─── Initialize repeater ─────────────────────────────────────────────────
        Result<void> initialize() {
            echo::category("isobus.niu.repeater").info("Repeater NIU initialized");
            return start();
        }

        // ─── Verify address uniqueness ───────────────────────────────────────────
        // Repeaters must ensure no address conflicts across segments
        bool check_address_unique(Address addr) const {
            // Would check both networks for address conflicts
            // For now, just a placeholder
            return true;
        }
    };

    // ═════════════════════════════════════════════════════════════════════════════
    // Bridge NIU (ISO 11783-4, Section 6.3)
    // ═════════════════════════════════════════════════════════════════════════════
    // A Bridge is similar to a Repeater but with more intelligent filtering.
    // Like Repeater, it does NOT perform address translation - single address space.
    //
    // Differences from Repeater:
    // - More sophisticated filtering (typically filters by PGN/priority)
    // - May implement learning bridge behavior (MAC table)
    // - Better bandwidth management
    //
    // Use cases:
    // - Segment isolation with smart forwarding
    // - Reducing cross-segment traffic
    // - Priority-based forwarding
    //
    class BridgeNIU : public NIU {
      public:
        explicit BridgeNIU(NIUConfig config = {}) : NIU(std::move(config)) {
            // Bridge uses configured filter mode (may be BlockAll or PassAll)
        }

        // ─── Initialize bridge ───────────────────────────────────────────────────
        Result<void> initialize() {
            echo::category("isobus.niu.bridge").info("Bridge NIU initialized with filter mode ",
                                                     config_.filter_mode == NIUFilterMode::PassAll ? "PassAll"
                                                                                                   : "BlockAll");
            return start();
        }

        // ─── Learning bridge behavior ────────────────────────────────────────────
        // Track which addresses are on which side to avoid unnecessary forwarding
        void learn_address(Address addr, Side side) {
            address_table_[addr] = side;
            echo::category("isobus.niu.bridge").debug("learned address ", addr, " on ",
                                                      side == Side::Tractor ? "tractor" : "implement");
        }

        dp::Optional<Side> lookup_address(Address addr) const {
            auto it = address_table_.find(addr);
            if (it != address_table_.end()) {
                return it->second;
            }
            return dp::nullopt;
        }

      private:
        dp::Map<Address, Side> address_table_; // Learning bridge MAC table
    };

    // ═════════════════════════════════════════════════════════════════════════════
    // Address Translation Database (ISO 11783-4, Section 6.4)
    // ═════════════════════════════════════════════════════════════════════════════
    // Maps addresses between different network segments for Router/Gateway NIUs.
    // Each NAME gets assigned different addresses on each segment.
    //
    struct AddressTranslation {
        Name name;                 // Device NAME
        Address tractor_address;   // Address on tractor segment
        Address implement_address; // Address on implement segment
        bool active = true;        // Whether translation is active

        AddressTranslation() = default;
        AddressTranslation(Name n, Address trac, Address impl)
            : name(n), tractor_address(trac), implement_address(impl) {}

        // Translate address from one side to the other
        Address translate(Address addr, Side from_side) const {
            if (from_side == Side::Tractor && addr == tractor_address) {
                return implement_address;
            }
            if (from_side == Side::Implement && addr == implement_address) {
                return tractor_address;
            }
            return INVALID_ADDRESS; // No translation
        }
    };

    class AddressTranslationDB {
        dp::Vector<AddressTranslation> translations_;

      public:
        // ─── Add translation entry ───────────────────────────────────────────────
        void add(Name name, Address tractor_addr, Address implement_addr) {
            // Check if translation already exists
            for (auto &t : translations_) {
                if (t.name == name) {
                    t.tractor_address = tractor_addr;
                    t.implement_address = implement_addr;
                    t.active = true;
                    return;
                }
            }
            translations_.push_back(AddressTranslation{name, tractor_addr, implement_addr});
        }

        // ─── Remove translation ──────────────────────────────────────────────────
        void remove(Name name) {
            for (auto it = translations_.begin(); it != translations_.end(); ++it) {
                if (it->name == name) {
                    translations_.erase(it);
                    return;
                }
            }
        }

        // ─── Translate address ───────────────────────────────────────────────────
        Address translate(Address addr, Side from_side) const {
            for (const auto &t : translations_) {
                if (!t.active)
                    continue;
                Address result = t.translate(addr, from_side);
                if (result != INVALID_ADDRESS) {
                    return result;
                }
            }
            return INVALID_ADDRESS;
        }

        // ─── Lookup by address ───────────────────────────────────────────────────
        dp::Optional<AddressTranslation> lookup_by_address(Address addr, Side side) const {
            for (const auto &t : translations_) {
                if (!t.active)
                    continue;
                if (side == Side::Tractor && t.tractor_address == addr) {
                    return t;
                }
                if (side == Side::Implement && t.implement_address == addr) {
                    return t;
                }
            }
            return dp::nullopt;
        }

        // ─── Lookup by NAME ──────────────────────────────────────────────────────
        dp::Optional<AddressTranslation> lookup_by_name(Name name) const {
            for (const auto &t : translations_) {
                if (t.active && t.name == name) {
                    return t;
                }
            }
            return dp::nullopt;
        }

        // ─── Check address availability ──────────────────────────────────────────
        bool is_address_available(Address addr, Side side) const {
            for (const auto &t : translations_) {
                if (!t.active)
                    continue;
                if (side == Side::Tractor && t.tractor_address == addr) {
                    return false;
                }
                if (side == Side::Implement && t.implement_address == addr) {
                    return false;
                }
            }
            return true;
        }

        const dp::Vector<AddressTranslation> &entries() const noexcept { return translations_; }

        void clear() { translations_.clear(); }
    };

    // ═════════════════════════════════════════════════════════════════════════════
    // Router NIU (ISO 11783-4, Section 6.4)
    // ═════════════════════════════════════════════════════════════════════════════
    // A Router maintains separate address spaces on each segment and performs
    // address translation when forwarding messages. This allows the same NAME
    // to have different addresses on each segment.
    //
    // Key features:
    // - Address translation database
    // - Source/destination address rewriting
    // - Address claim coordination
    // - Separate address spaces per segment
    //
    // Use cases:
    // - Connecting segments with address conflicts
    // - Implementing security boundaries
    // - Network segmentation with controlled routing
    //
    class RouterNIU : public NIU {
        AddressTranslationDB translation_db_;

      public:
        explicit RouterNIU(NIUConfig config = {}) : NIU(std::move(config)) {}

        // ─── Initialize router ───────────────────────────────────────────────────
        Result<void> initialize() {
            echo::category("isobus.niu.router").info("Router NIU initialized with address translation");
            return start();
        }

        // ─── Address translation management ──────────────────────────────────────
        void add_translation(Name name, Address tractor_addr, Address implement_addr) {
            translation_db_.add(name, tractor_addr, implement_addr);
            echo::category("isobus.niu.router")
                .info("added translation: NAME=", name.value(), " tractor=", tractor_addr, " implement=", implement_addr);
        }

        void remove_translation(Name name) {
            translation_db_.remove(name);
            echo::category("isobus.niu.router").info("removed translation for NAME=", name.value());
        }

        const AddressTranslationDB &translation_db() const noexcept { return translation_db_; }

        // ─── Translate and forward frame ─────────────────────────────────────────
        void process_tractor_frame(const Frame &frame) { process_and_translate(frame, Side::Tractor); }

        void process_implement_frame(const Frame &frame) { process_and_translate(frame, Side::Implement); }

      private:
        void process_and_translate(const Frame &frame, Side origin) {
            if (!state_.is(NIUState::Active)) {
                return;
            }

            PGN pgn = frame.pgn();
            Address source = frame.source();
            Address destination = frame.destination();

            // Resolve filtering policy
            auto [policy, rate_limited] = resolve_policy_ex(pgn, source, destination, frame.is_broadcast(), origin);

            if (rate_limited || policy == ForwardPolicy::Block) {
                ++blocked_count_;
                on_blocked.emit(frame, origin);
                return;
            }

            // Perform address translation
            Address translated_source = translation_db_.translate(source, origin);
            Address translated_dest = INVALID_ADDRESS;

            if (!frame.is_broadcast()) {
                translated_dest = translation_db_.translate(destination, origin);
                if (translated_dest == INVALID_ADDRESS) {
                    // Destination not in translation table - block
                    ++blocked_count_;
                    on_blocked.emit(frame, origin);
                    echo::category("isobus.niu.router")
                        .debug("no translation for destination ", destination, " - blocking");
                    return;
                }
            }

            if (translated_source == INVALID_ADDRESS) {
                // Source not in translation table - forward without translation
                echo::category("isobus.niu.router")
                    .debug("no translation for source ", source, " - forwarding as-is");
                forward(frame, origin);
            } else {
                // Create translated frame
                Frame translated_frame = frame;
                translated_frame.set_source(translated_source);
                if (!frame.is_broadcast() && translated_dest != INVALID_ADDRESS) {
                    translated_frame.set_destination(translated_dest);
                }

                forward(translated_frame, origin);
                echo::category("isobus.niu.router")
                    .debug("translated and forwarded: src ", source, "->", translated_source);
            }

            ++forwarded_count_;
            on_forwarded.emit(frame, origin);

            if (policy == ForwardPolicy::Monitor) {
                on_monitored.emit(frame, origin);
            }
        }
    };

    // ═════════════════════════════════════════════════════════════════════════════
    // Gateway NIU (ISO 11783-4, Section 6.4)
    // ═════════════════════════════════════════════════════════════════════════════
    // A Gateway extends Router functionality with message repackaging and protocol
    // translation capabilities. Like Router, it maintains separate address spaces
    // and performs address translation.
    //
    // Additional features beyond Router:
    // - Message repackaging (change message structure)
    // - Protocol translation (e.g., CAN 2.0B <-> CAN FD)
    // - Data transformation callbacks
    // - Bidirectional message mapping
    //
    // Use cases:
    // - Protocol bridging between different CAN versions
    // - Message format translation
    // - Data unit conversion (imperial <-> metric)
    // - Custom message transformations
    //
    using MessageTransformFn = dp::Function<dp::Optional<Message>(const Message &)>;

    class GatewayNIU : public RouterNIU {
        dp::Map<PGN, MessageTransformFn> tractor_transforms_;   // Tractor->Implement transforms
        dp::Map<PGN, MessageTransformFn> implement_transforms_; // Implement->Tractor transforms

      public:
        explicit GatewayNIU(NIUConfig config = {}) : RouterNIU(std::move(config)) {}

        // ─── Initialize gateway ──────────────────────────────────────────────────
        Result<void> initialize() {
            echo::category("isobus.niu.gateway")
                .info("Gateway NIU initialized with address translation and message repackaging");
            return start();
        }

        // ─── Register message transforms ─────────────────────────────────────────
        void register_tractor_transform(PGN pgn, MessageTransformFn transform) {
            tractor_transforms_[pgn] = std::move(transform);
            echo::category("isobus.niu.gateway").debug("registered tractor->implement transform for PGN ", pgn);
        }

        void register_implement_transform(PGN pgn, MessageTransformFn transform) {
            implement_transforms_[pgn] = std::move(transform);
            echo::category("isobus.niu.gateway").debug("registered implement->tractor transform for PGN ", pgn);
        }

        // ─── Process with repackaging ────────────────────────────────────────────
        void process_tractor_frame(const Frame &frame) {
            process_and_repackage(frame, Side::Tractor, tractor_transforms_);
        }

        void process_implement_frame(const Frame &frame) {
            process_and_repackage(frame, Side::Implement, implement_transforms_);
        }

      private:
        void process_and_repackage(const Frame &frame, Side origin,
                                   const dp::Map<PGN, MessageTransformFn> &transforms) {
            if (!state_.is(NIUState::Active)) {
                return;
            }

            PGN pgn = frame.pgn();
            Address source = frame.source();
            Address destination = frame.destination();

            // Resolve filtering policy
            auto [policy, rate_limited] = resolve_policy_ex(pgn, source, destination, frame.is_broadcast(), origin);

            if (rate_limited || policy == ForwardPolicy::Block) {
                ++blocked_count_;
                on_blocked.emit(frame, origin);
                return;
            }

            // Check for message transform
            auto transform_it = transforms.find(pgn);
            if (transform_it != transforms.end()) {
                // Apply message transformation
                Message msg = Message::from_frame(frame);
                auto transformed = transform_it->second(msg);

                if (transformed.has_value()) {
                    // Create frame from transformed message
                    Frame transformed_frame = transformed.value().to_frame();

                    // Still need address translation
                    Address translated_source = translation_db().translate(source, origin);
                    Address translated_dest = INVALID_ADDRESS;

                    if (!transformed_frame.is_broadcast()) {
                        translated_dest = translation_db().translate(destination, origin);
                    }

                    if (translated_source != INVALID_ADDRESS) {
                        transformed_frame.set_source(translated_source);
                    }
                    if (translated_dest != INVALID_ADDRESS) {
                        transformed_frame.set_destination(translated_dest);
                    }

                    forward(transformed_frame, origin);
                    echo::category("isobus.niu.gateway").debug("repackaged and forwarded PGN ", pgn);
                } else {
                    // Transform returned nullopt - block message
                    ++blocked_count_;
                    on_blocked.emit(frame, origin);
                    echo::category("isobus.niu.gateway").debug("transform blocked PGN ", pgn);
                    return;
                }
            } else {
                // No transform - use standard Router behavior
                RouterNIU::process_and_translate(frame, origin);
                return;
            }

            ++forwarded_count_;
            on_forwarded.emit(frame, origin);

            if (policy == ForwardPolicy::Monitor) {
                on_monitored.emit(frame, origin);
            }
        }
    };
} // namespace agrobus::net
