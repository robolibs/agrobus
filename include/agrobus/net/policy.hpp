#pragma once

#include <agrobus/net/error.hpp>
#include <agrobus/net/event.hpp>
#include <agrobus/net/state_machine.hpp>
#include <agrobus/net/types.hpp>
#include <datapod/datapod.hpp>
#include <echo/echo.hpp>

namespace agrobus::net {

    // ─── Safe state levels ───────────────────────────────────────────────────────
    enum class SafeState : u8 {
        Normal,    // All systems operating normally
        Degraded,  // Partial function loss, safe defaults applied
        Emergency, // Critical failure, outputs disabled
        Shutdown   // System shutting down
    };

    // ─── Degraded mode actions ───────────────────────────────────────────────────
    enum class DegradedAction : u8 {
        HoldLast,  // Hold last known good value
        RampDown,  // Gradually reduce to zero/safe
        Immediate, // Immediately set to safe value
        Disable    // Disable output entirely
    };

    // ─── Freshness requirement for a data source ─────────────────────────────────
    struct FreshnessRequirement {
        dp::String source_name;
        u32 max_age_ms = 500;     // Max allowed age before Degraded
        u32 escalation_ms = 2000; // Time in Degraded before escalating to Emergency
        DegradedAction action = DegradedAction::HoldLast;
    };

    // ─── Safety configuration ────────────────────────────────────────────────────
    struct SafetyConfig {
        u32 heartbeat_timeout_ms = 500;
        u32 command_freshness_ms = 200;
        u32 escalation_delay_ms = 2000;
        DegradedAction default_action = DegradedAction::HoldLast;

        SafetyConfig &heartbeat_timeout(u32 ms) {
            heartbeat_timeout_ms = ms;
            return *this;
        }
        SafetyConfig &command_freshness(u32 ms) {
            command_freshness_ms = ms;
            return *this;
        }
        SafetyConfig &escalation_delay(u32 ms) {
            escalation_delay_ms = ms;
            return *this;
        }
        SafetyConfig &default_degraded_action(DegradedAction a) {
            default_action = a;
            return *this;
        }
    };

    // ─── Safety Policy Manager ───────────────────────────────────────────────────
    // Monitors data source freshness and escalates through safety states:
    //   Normal -> Degraded -> Emergency -> Shutdown
    class SafetyPolicy {
        SafetyConfig config_;
        StateMachine<SafeState> state_{SafeState::Normal};
        dp::Vector<FreshnessRequirement> requirements_;
        dp::Map<dp::String, u32> last_seen_ms_; // source -> last timestamp
        u32 current_time_ms_ = 0;
        u32 degraded_since_ms_ = 0;

      public:
        explicit SafetyPolicy(SafetyConfig config = {}) : config_(std::move(config)) {}

        // ─── Freshness requirements ──────────────────────────────────────────────
        SafetyPolicy &require_freshness(FreshnessRequirement req) {
            last_seen_ms_[req.source_name] = 0;
            requirements_.push_back(std::move(req));
            echo::category("isobus.safety").debug("freshness requirement added: ", requirements_.back().source_name);
            return *this;
        }

        // ─── Report that a source is alive ───────────────────────────────────────
        void report_alive(const dp::String &source) { last_seen_ms_[source] = current_time_ms_; }

        // ─── Periodic update (call every tick) ───────────────────────────────────
        void update(u32 elapsed_ms) {
            current_time_ms_ += elapsed_ms;

            // Already in Shutdown - no further processing
            if (state_.is(SafeState::Shutdown)) {
                return;
            }

            // Already in Emergency - check for shutdown escalation (not implemented,
            // Emergency is terminal unless reset)
            if (state_.is(SafeState::Emergency)) {
                return;
            }

            // Check freshness of all required sources
            bool any_stale = false;
            for (const auto &req : requirements_) {
                auto it = last_seen_ms_.find(req.source_name);
                if (it == last_seen_ms_.end()) {
                    // Source never seen - considered stale
                    any_stale = true;
                    continue;
                }

                u32 age = current_time_ms_ - it->second;
                if (age > req.max_age_ms) {
                    any_stale = true;

                    // Transition to Degraded if currently Normal
                    if (state_.is(SafeState::Normal)) {
                        degraded_since_ms_ = current_time_ms_;
                        state_.transition(SafeState::Degraded);
                        on_state_change.emit(SafeState::Normal, SafeState::Degraded);
                        on_source_timeout.emit(req.source_name);
                        echo::category("isobus.safety")
                            .warn("source '", req.source_name, "' stale (age=", age, "ms), entering Degraded");
                    } else if (state_.is(SafeState::Degraded)) {
                        // Already degraded - check escalation
                        u32 time_in_degraded = current_time_ms_ - degraded_since_ms_;
                        if (time_in_degraded > req.escalation_ms) {
                            state_.transition(SafeState::Emergency);
                            on_state_change.emit(SafeState::Degraded, SafeState::Emergency);
                            dp::String reason = "source '" + req.source_name + "' exceeded escalation timeout (" +
                                                dp::String(std::to_string(time_in_degraded)) + "ms)";
                            on_emergency.emit(reason);
                            echo::category("isobus.safety").error("EMERGENCY: ", reason);
                            return; // Emergency is terminal
                        }
                    }
                }
            }

            // If all sources are fresh and we were Degraded, return to Normal
            if (!any_stale && state_.is(SafeState::Degraded)) {
                state_.transition(SafeState::Normal);
                on_state_change.emit(SafeState::Degraded, SafeState::Normal);
                echo::category("isobus.safety").info("all sources fresh, returning to Normal");
            }
        }

        // ─── Manual state control ────────────────────────────────────────────────
        void trigger_emergency(const dp::String &reason) {
            SafeState prev = state_.state();
            state_.transition(SafeState::Emergency);
            if (prev != SafeState::Emergency) {
                on_state_change.emit(prev, SafeState::Emergency);
                on_emergency.emit(reason);
                echo::category("isobus.safety").error("manual emergency triggered: ", reason);
            }
        }

        void reset_to_normal() {
            SafeState prev = state_.state();
            state_.transition(SafeState::Normal);
            if (prev != SafeState::Normal) {
                on_state_change.emit(prev, SafeState::Normal);
                echo::category("isobus.safety").info("reset to Normal from state ", static_cast<u8>(prev));
            }
            // Reset freshness timestamps to current time
            for (auto &[source, ts] : last_seen_ms_) {
                ts = current_time_ms_;
            }
        }

        // ─── Query ───────────────────────────────────────────────────────────────
        SafeState state() const noexcept { return state_.state(); }
        bool is_safe() const noexcept { return state_.state() == SafeState::Normal; }
        bool is_degraded() const noexcept { return state_.state() == SafeState::Degraded; }

        DegradedAction current_action() const noexcept {
            if (state_.is(SafeState::Normal)) {
                return config_.default_action; // not really applicable in Normal
            }
            // Find the stale source with the most severe action
            DegradedAction worst = config_.default_action;
            for (const auto &req : requirements_) {
                auto it = last_seen_ms_.find(req.source_name);
                if (it == last_seen_ms_.end()) {
                    if (static_cast<u8>(req.action) > static_cast<u8>(worst)) {
                        worst = req.action;
                    }
                    continue;
                }
                u32 age = current_time_ms_ - it->second;
                if (age > req.max_age_ms) {
                    if (static_cast<u8>(req.action) > static_cast<u8>(worst)) {
                        worst = req.action;
                    }
                }
            }
            return worst;
        }

        // ─── Events ──────────────────────────────────────────────────────────────
        Event<SafeState, SafeState> on_state_change; // old, new
        Event<dp::String> on_source_timeout;         // which source timed out
        Event<dp::String> on_emergency;              // reason
    };
} // namespace agrobus::net
