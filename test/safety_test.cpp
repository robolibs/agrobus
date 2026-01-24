#include <doctest/doctest.h>
#include <agrobus/net/policy.hpp>

using namespace agrobus::net;

TEST_CASE("SafetyPolicy construction") {
    SUBCASE("default configuration") {
        SafetyPolicy policy;
        CHECK(policy.state() == SafeState::Normal);
        CHECK(policy.is_safe());
        CHECK_FALSE(policy.is_degraded());
    }

    SUBCASE("custom configuration") {
        SafetyConfig cfg;
        cfg.heartbeat_timeout(1000)
           .command_freshness(500)
           .escalation_delay(5000)
           .default_degraded_action(DegradedAction::RampDown);
        SafetyPolicy policy(cfg);
        CHECK(policy.is_safe());
    }
}

TEST_CASE("SafetyPolicy freshness requirements") {
    SafetyPolicy policy;
    policy.require_freshness(FreshnessRequirement{
        "heartbeat", 500, 2000, DegradedAction::HoldLast
    });

    SUBCASE("stays normal when source reports alive within timeout") {
        policy.report_alive("heartbeat");
        policy.update(100);
        CHECK(policy.is_safe());
        policy.update(100);
        CHECK(policy.is_safe());
        policy.update(100);
        CHECK(policy.is_safe());
    }

    SUBCASE("transitions to degraded when source is stale") {
        policy.report_alive("heartbeat");
        // Advance time past max_age_ms (500ms)
        policy.update(600);
        CHECK(policy.is_degraded());
        CHECK_FALSE(policy.is_safe());
    }

    SUBCASE("never-seen source triggers degraded immediately") {
        // Source never reported alive, initial timestamp is 0
        // After any time > max_age_ms, it should be stale
        policy.update(600);
        CHECK(policy.is_degraded());
    }
}

TEST_CASE("SafetyPolicy escalation to emergency") {
    SafetyConfig cfg;
    cfg.escalation_delay(2000);
    SafetyPolicy policy(cfg);
    policy.require_freshness(FreshnessRequirement{
        "command", 200, 1000, DegradedAction::Immediate
    });

    SUBCASE("escalates to emergency after escalation timeout") {
        policy.report_alive("command");
        // Let the source go stale
        policy.update(300); // age = 300 > 200 -> Degraded
        CHECK(policy.is_degraded());

        // Stay in Degraded for escalation_ms (1000)
        policy.update(500);
        CHECK(policy.is_degraded()); // 500ms in degraded, not yet escalated

        policy.update(600); // 1100ms in degraded > 1000 escalation
        CHECK(policy.state() == SafeState::Emergency);
    }

    SUBCASE("recovers from degraded if source comes back") {
        policy.report_alive("command");
        policy.update(300); // Degraded
        CHECK(policy.is_degraded());

        // Source comes back alive
        policy.report_alive("command");
        policy.update(10); // Small tick, source is now fresh
        CHECK(policy.is_safe());
    }
}

TEST_CASE("SafetyPolicy multiple sources") {
    SafetyPolicy policy;
    policy.require_freshness(FreshnessRequirement{
        "heartbeat", 500, 2000, DegradedAction::HoldLast
    });
    policy.require_freshness(FreshnessRequirement{
        "speed", 300, 1500, DegradedAction::RampDown
    });

    SUBCASE("both fresh keeps normal") {
        policy.report_alive("heartbeat");
        policy.report_alive("speed");
        policy.update(200);
        CHECK(policy.is_safe());
    }

    SUBCASE("one stale triggers degraded") {
        policy.report_alive("heartbeat");
        policy.report_alive("speed");
        // Only heartbeat stays alive
        policy.update(400); // speed age = 400 > 300
        CHECK(policy.is_degraded());
    }

    SUBCASE("recovery requires all sources fresh") {
        policy.report_alive("heartbeat");
        policy.report_alive("speed");
        policy.update(400); // speed stale
        CHECK(policy.is_degraded());

        // Only speed comes back but heartbeat is still within timeout
        policy.report_alive("speed");
        policy.update(10);
        CHECK(policy.is_safe()); // heartbeat age = 410 < 500, speed fresh
    }
}

TEST_CASE("SafetyPolicy manual state control") {
    SafetyPolicy policy;

    SUBCASE("trigger_emergency from normal") {
        policy.trigger_emergency("sensor failure");
        CHECK(policy.state() == SafeState::Emergency);
    }

    SUBCASE("trigger_emergency from degraded") {
        policy.require_freshness(FreshnessRequirement{"src", 100, 500, DegradedAction::HoldLast});
        policy.update(200); // Degraded
        policy.trigger_emergency("operator override");
        CHECK(policy.state() == SafeState::Emergency);
    }

    SUBCASE("reset_to_normal from emergency") {
        policy.trigger_emergency("test");
        CHECK(policy.state() == SafeState::Emergency);
        policy.reset_to_normal();
        CHECK(policy.is_safe());
    }

    SUBCASE("reset_to_normal resets timestamps") {
        policy.require_freshness(FreshnessRequirement{"src", 100, 500, DegradedAction::HoldLast});
        policy.update(200); // Degraded (src stale)
        CHECK(policy.is_degraded());
        policy.reset_to_normal();
        CHECK(policy.is_safe());
        // Small update should stay normal since timestamps were reset
        policy.update(50);
        CHECK(policy.is_safe());
    }
}

TEST_CASE("SafetyPolicy events") {
    SafetyPolicy policy;
    policy.require_freshness(FreshnessRequirement{
        "heartbeat", 200, 1000, DegradedAction::Immediate
    });

    SUBCASE("on_state_change fires on degradation") {
        u32 change_count = 0;
        SafeState captured_old = SafeState::Normal;
        SafeState captured_new = SafeState::Normal;
        policy.on_state_change.subscribe([&](SafeState old_s, SafeState new_s) {
            ++change_count;
            captured_old = old_s;
            captured_new = new_s;
        });

        policy.report_alive("heartbeat");
        policy.update(300); // Triggers Degraded
        CHECK(change_count == 1);
        CHECK(captured_old == SafeState::Normal);
        CHECK(captured_new == SafeState::Degraded);
    }

    SUBCASE("on_source_timeout fires with source name") {
        dp::String timed_out_source;
        policy.on_source_timeout.subscribe([&](dp::String src) {
            timed_out_source = src;
        });

        policy.report_alive("heartbeat");
        policy.update(300);
        CHECK(timed_out_source == "heartbeat");
    }

    SUBCASE("on_emergency fires with reason") {
        dp::String emergency_reason;
        policy.on_emergency.subscribe([&](dp::String reason) {
            emergency_reason = reason;
        });

        policy.trigger_emergency("critical fault");
        CHECK_FALSE(emergency_reason.empty());
        CHECK(emergency_reason == "critical fault");
    }

    SUBCASE("on_emergency fires on escalation") {
        dp::String emergency_reason;
        policy.on_emergency.subscribe([&](dp::String reason) {
            emergency_reason = reason;
        });

        policy.report_alive("heartbeat");
        policy.update(300);  // Degraded
        policy.update(800);  // Still in degraded, 800ms
        policy.update(300);  // 1100ms in degraded > 1000 escalation
        CHECK(policy.state() == SafeState::Emergency);
        CHECK_FALSE(emergency_reason.empty());
    }
}

TEST_CASE("SafetyPolicy current_action") {
    SafetyPolicy policy;
    policy.require_freshness(FreshnessRequirement{
        "sensor_a", 200, 1000, DegradedAction::HoldLast
    });
    policy.require_freshness(FreshnessRequirement{
        "sensor_b", 300, 1000, DegradedAction::Disable
    });

    SUBCASE("returns default when normal") {
        policy.report_alive("sensor_a");
        policy.report_alive("sensor_b");
        policy.update(100);
        CHECK(policy.current_action() == DegradedAction::HoldLast);
    }

    SUBCASE("returns worst action when degraded") {
        policy.report_alive("sensor_a");
        policy.report_alive("sensor_b");
        // Let both go stale
        policy.update(400);
        CHECK(policy.is_degraded());
        // Disable > HoldLast, so worst should be Disable
        CHECK(policy.current_action() == DegradedAction::Disable);
    }

    SUBCASE("returns specific source action when only one stale") {
        policy.report_alive("sensor_a");
        policy.report_alive("sensor_b");
        // Only sensor_a goes stale (age > 200 but < 300)
        policy.update(250);
        CHECK(policy.is_degraded());
        CHECK(policy.current_action() == DegradedAction::HoldLast);
    }
}

TEST_CASE("SafetyPolicy emergency is sticky") {
    SafetyPolicy policy;
    policy.require_freshness(FreshnessRequirement{
        "src", 100, 500, DegradedAction::HoldLast
    });

    // Go to emergency
    policy.update(200); // Degraded
    policy.update(600); // Emergency (600ms in degraded > 500 escalation)
    CHECK(policy.state() == SafeState::Emergency);

    // Further updates should not change state
    policy.report_alive("src");
    policy.update(10);
    CHECK(policy.state() == SafeState::Emergency);

    // Only explicit reset can recover
    policy.reset_to_normal();
    CHECK(policy.is_safe());
}
