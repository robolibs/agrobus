#include <doctest/doctest.h>
#include <agrobus/net/policy.hpp>

using namespace agrobus::net;

// ═════════════════════════════════════════════════════════════════════════════
// SafeState and DegradedAction Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("SafeState enumeration") {
    SUBCASE("state values") {
        CHECK(static_cast<u8>(SafeState::Normal) == 0);
        CHECK(static_cast<u8>(SafeState::Degraded) == 1);
        CHECK(static_cast<u8>(SafeState::Emergency) == 2);
        CHECK(static_cast<u8>(SafeState::Shutdown) == 3);
    }
}

TEST_CASE("DegradedAction enumeration") {
    SUBCASE("action values") {
        CHECK(static_cast<u8>(DegradedAction::HoldLast) == 0);
        CHECK(static_cast<u8>(DegradedAction::RampDown) == 1);
        CHECK(static_cast<u8>(DegradedAction::Immediate) == 2);
        CHECK(static_cast<u8>(DegradedAction::Disable) == 3);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// SafetyConfig Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("SafetyConfig defaults") {
    SafetyConfig config;

    CHECK(config.heartbeat_timeout_ms == 500);
    CHECK(config.command_freshness_ms == 200);
    CHECK(config.escalation_delay_ms == 2000);
    CHECK(config.default_action == DegradedAction::HoldLast);
}

TEST_CASE("SafetyConfig fluent configuration") {
    SafetyConfig config;
    config.heartbeat_timeout(1000).command_freshness(300).escalation_delay(3000).default_degraded_action(
        DegradedAction::RampDown);

    CHECK(config.heartbeat_timeout_ms == 1000);
    CHECK(config.command_freshness_ms == 300);
    CHECK(config.escalation_delay_ms == 3000);
    CHECK(config.default_action == DegradedAction::RampDown);
}

// ═════════════════════════════════════════════════════════════════════════════
// SafetyPolicy Basic Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("SafetyPolicy initialization") {
    SafetyPolicy policy;

    CHECK(policy.state() == SafeState::Normal);
    CHECK(policy.is_safe());
    CHECK_FALSE(policy.is_degraded());
}

TEST_CASE("SafetyPolicy with custom config") {
    SafetyConfig config;
    config.heartbeat_timeout(2000);

    SafetyPolicy policy(config);

    CHECK(policy.state() == SafeState::Normal);
}

// ═════════════════════════════════════════════════════════════════════════════
// Freshness Requirement Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("SafetyPolicy freshness requirements") {
    SafetyPolicy policy;

    SUBCASE("add single requirement") {
        FreshnessRequirement req;
        req.source_name = "vt_terminal";
        req.max_age_ms = 500;

        policy.require_freshness(req);

        // State remains normal initially
        CHECK(policy.state() == SafeState::Normal);
    }

    SUBCASE("add multiple requirements") {
        policy.require_freshness({.source_name = "source1", .max_age_ms = 500});
        policy.require_freshness({.source_name = "source2", .max_age_ms = 1000});
        policy.require_freshness({.source_name = "source3", .max_age_ms = 200});

        CHECK(policy.state() == SafeState::Normal);
    }
}

TEST_CASE("SafetyPolicy report_alive") {
    SafetyPolicy policy;

    FreshnessRequirement req;
    req.source_name = "test_source";
    req.max_age_ms = 500;
    policy.require_freshness(req);

    SUBCASE("reporting alive prevents timeout") {
        policy.report_alive("test_source");
        policy.update(100);
        policy.report_alive("test_source");
        policy.update(100);
        policy.report_alive("test_source");
        policy.update(100);

        CHECK(policy.state() == SafeState::Normal);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// State Transition Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("SafetyPolicy Normal to Degraded transition") {
    SafetyPolicy policy;

    FreshnessRequirement req;
    req.source_name = "critical_source";
    req.max_age_ms = 500;
    req.escalation_ms = 2000;
    policy.require_freshness(req);

    SUBCASE("source timeout transitions to Degraded") {
        policy.report_alive("critical_source");
        policy.update(100);

        // Let source time out
        policy.update(600); // Total: 700ms > 500ms max_age

        CHECK(policy.state() == SafeState::Degraded);
        CHECK(policy.is_degraded());
        CHECK_FALSE(policy.is_safe());
    }

    SUBCASE("source alive keeps Normal") {
        policy.report_alive("critical_source");
        policy.update(100);
        policy.report_alive("critical_source");
        policy.update(100);
        policy.report_alive("critical_source");
        policy.update(100);

        CHECK(policy.state() == SafeState::Normal);
    }
}

TEST_CASE("SafetyPolicy Degraded to Emergency escalation") {
    SafetyPolicy policy;

    FreshnessRequirement req;
    req.source_name = "critical_source";
    req.max_age_ms = 500;
    req.escalation_ms = 2000;
    policy.require_freshness(req);

    SUBCASE("prolonged degraded state escalates to Emergency") {
        policy.report_alive("critical_source");
        policy.update(100);

        // Timeout to Degraded
        policy.update(600); // 700ms total > 500ms
        CHECK(policy.state() == SafeState::Degraded);

        // Stay degraded long enough to escalate
        policy.update(2100); // > escalation_ms
        CHECK(policy.state() == SafeState::Emergency);
    }

    SUBCASE("recovery before escalation returns to Normal") {
        policy.report_alive("critical_source");
        policy.update(100);

        // Timeout to Degraded
        policy.update(600);
        CHECK(policy.state() == SafeState::Degraded);

        // Recover before escalation
        policy.report_alive("critical_source");
        policy.update(100);
        CHECK(policy.state() == SafeState::Normal);
    }
}

TEST_CASE("SafetyPolicy Emergency is terminal") {
    SafetyPolicy policy;

    FreshnessRequirement req;
    req.source_name = "critical_source";
    req.max_age_ms = 500;
    req.escalation_ms = 1000;
    policy.require_freshness(req);

    SUBCASE("Emergency state is terminal") {
        policy.report_alive("critical_source");
        policy.update(100);

        // Escalate to Emergency
        policy.update(600); // Degraded
        policy.update(1100); // Emergency

        CHECK(policy.state() == SafeState::Emergency);

        // Even if source comes back, stays Emergency
        policy.report_alive("critical_source");
        policy.update(100);

        CHECK(policy.state() == SafeState::Emergency);
    }

    SUBCASE("Shutdown is terminal") {
        policy.report_alive("critical_source");
        policy.trigger_emergency("test");

        policy.update(1000);
        CHECK(policy.state() == SafeState::Emergency);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Manual State Control Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("SafetyPolicy manual emergency trigger") {
    SafetyPolicy policy;

    SUBCASE("trigger emergency from Normal") {
        CHECK(policy.state() == SafeState::Normal);

        policy.trigger_emergency("manual test");

        CHECK(policy.state() == SafeState::Emergency);
    }

    SUBCASE("trigger emergency from Degraded") {
        policy.require_freshness({.source_name = "test", .max_age_ms = 100});
        policy.update(200); // Timeout to Degraded

        CHECK(policy.state() == SafeState::Degraded);

        policy.trigger_emergency("manual test");

        CHECK(policy.state() == SafeState::Emergency);
    }
}

TEST_CASE("SafetyPolicy reset to normal") {
    SafetyPolicy policy;

    policy.require_freshness({.source_name = "test", .max_age_ms = 100});

    SUBCASE("reset from Degraded") {
        policy.update(200); // Timeout to Degraded
        CHECK(policy.state() == SafeState::Degraded);

        policy.reset_to_normal();

        CHECK(policy.state() == SafeState::Normal);
        CHECK(policy.is_safe());
    }

    SUBCASE("reset from Emergency") {
        policy.trigger_emergency("test");
        CHECK(policy.state() == SafeState::Emergency);

        policy.reset_to_normal();

        CHECK(policy.state() == SafeState::Normal);
    }

    SUBCASE("reset updates freshness timestamps") {
        policy.update(200); // Timeout to Degraded
        CHECK(policy.state() == SafeState::Degraded);

        policy.reset_to_normal();

        // Should not timeout immediately
        policy.update(50);
        CHECK(policy.state() == SafeState::Normal);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Degraded Action Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("SafetyPolicy degraded action selection") {
    SafetyPolicy policy;

    SUBCASE("default action in Normal state") {
        CHECK(policy.current_action() == DegradedAction::HoldLast);
    }

    SUBCASE("specific action for stale source") {
        policy.require_freshness(
            {.source_name = "test", .max_age_ms = 100, .action = DegradedAction::RampDown});

        policy.update(200); // Timeout
        CHECK(policy.state() == SafeState::Degraded);
        CHECK(policy.current_action() == DegradedAction::RampDown);
    }

    SUBCASE("most severe action is selected") {
        policy.require_freshness(
            {.source_name = "source1", .max_age_ms = 100, .action = DegradedAction::HoldLast});
        policy.require_freshness(
            {.source_name = "source2", .max_age_ms = 100, .action = DegradedAction::Disable});
        policy.require_freshness(
            {.source_name = "source3", .max_age_ms = 100, .action = DegradedAction::RampDown});

        policy.update(200); // All timeout

        CHECK(policy.state() == SafeState::Degraded);
        CHECK(policy.current_action() == DegradedAction::Disable); // Most severe
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Event Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("SafetyPolicy event notifications") {
    SafetyPolicy policy;

    policy.require_freshness({.source_name = "test_source", .max_age_ms = 100});

    SUBCASE("state change event") {
        bool state_changed = false;
        SafeState old_state = SafeState::Normal;
        SafeState new_state = SafeState::Normal;

        policy.on_state_change.subscribe([&](SafeState old_s, SafeState new_s) {
            state_changed = true;
            old_state = old_s;
            new_state = new_s;
        });

        policy.update(200); // Timeout to Degraded

        CHECK(state_changed);
        CHECK(old_state == SafeState::Normal);
        CHECK(new_state == SafeState::Degraded);
    }

    SUBCASE("source timeout event") {
        bool timeout_occurred = false;
        dp::String timed_out_source;

        policy.on_source_timeout.subscribe([&](const dp::String &source) {
            timeout_occurred = true;
            timed_out_source = source;
        });

        policy.update(200); // Timeout

        CHECK(timeout_occurred);
        CHECK(timed_out_source == "test_source");
    }

    SUBCASE("emergency event") {
        bool emergency_triggered = false;
        dp::String emergency_reason;

        policy.on_emergency.subscribe([&](const dp::String &reason) {
            emergency_triggered = true;
            emergency_reason = reason;
        });

        policy.trigger_emergency("test emergency");

        CHECK(emergency_triggered);
        CHECK(emergency_reason == "test emergency");
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Complex Scenarios
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("SafetyPolicy multiple source tracking") {
    SafetyPolicy policy;

    policy.require_freshness({.source_name = "vt", .max_age_ms = 500});
    policy.require_freshness({.source_name = "tcu", .max_age_ms = 300});
    policy.require_freshness({.source_name = "ecu", .max_age_ms = 1000});

    SUBCASE("all sources healthy") {
        policy.report_alive("vt");
        policy.report_alive("tcu");
        policy.report_alive("ecu");

        policy.update(200);

        CHECK(policy.state() == SafeState::Normal);
    }

    SUBCASE("one source times out") {
        policy.report_alive("vt");
        policy.report_alive("tcu");
        policy.report_alive("ecu");

        policy.update(100);

        // TCU times out (max_age_ms = 300)
        policy.report_alive("vt");
        policy.report_alive("ecu");
        policy.update(300);

        CHECK(policy.state() == SafeState::Degraded);
    }

    SUBCASE("recovery after partial timeout") {
        policy.report_alive("vt");
        policy.report_alive("tcu");
        policy.report_alive("ecu");

        policy.update(100);

        // TCU times out
        policy.report_alive("vt");
        policy.report_alive("ecu");
        policy.update(300);
        CHECK(policy.state() == SafeState::Degraded);

        // TCU recovers
        policy.report_alive("tcu");
        policy.update(100);
        CHECK(policy.state() == SafeState::Normal);
    }
}

TEST_CASE("SafetyPolicy realistic scenario") {
    SafetyPolicy policy;

    // Typical ISOBUS implement with VT and TCU
    policy.require_freshness({.source_name = "vt_terminal",
                              .max_age_ms = 500,
                              .escalation_ms = 2000,
                              .action = DegradedAction::HoldLast});

    policy.require_freshness({.source_name = "tcu_commands",
                              .max_age_ms = 200,
                              .escalation_ms = 1000,
                              .action = DegradedAction::Disable});

    SUBCASE("normal operation") {
        for (int i = 0; i < 20; ++i) {
            policy.report_alive("vt_terminal");
            policy.report_alive("tcu_commands");
            policy.update(100);
        }

        CHECK(policy.state() == SafeState::Normal);
    }

    SUBCASE("VT connection lost") {
        policy.report_alive("vt_terminal");
        policy.report_alive("tcu_commands");
        policy.update(100);

        // VT stops responding
        for (int i = 0; i < 10; ++i) {
            policy.report_alive("tcu_commands");
            policy.update(100);
        }

        CHECK(policy.state() == SafeState::Degraded);
        CHECK(policy.current_action() == DegradedAction::HoldLast);
    }

    SUBCASE("TCU timeout is more severe") {
        policy.report_alive("vt_terminal");
        policy.report_alive("tcu_commands");
        policy.update(100);

        // TCU stops responding
        for (int i = 0; i < 5; ++i) {
            policy.report_alive("vt_terminal");
            policy.update(100);
        }

        CHECK(policy.state() == SafeState::Degraded);
        CHECK(policy.current_action() == DegradedAction::Disable);
    }
}
