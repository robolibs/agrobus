#include <doctest/doctest.h>
#include <agrobus/net/scheduler.hpp>

using namespace agrobus::net;

// ═════════════════════════════════════════════════════════════════════════════
// PeriodicTask Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("PeriodicTask structure") {
    PeriodicTask task;

    SUBCASE("default values") {
        CHECK(task.interval_ms == 0);
        CHECK(task.elapsed_ms == 0);
        CHECK(task.enabled);
        CHECK(task.max_retries == 0);
        CHECK(task.retry_count == 0);
    }

    SUBCASE("due() method") {
        task.interval_ms = 100;
        task.elapsed_ms = 50;
        CHECK_FALSE(task.due());

        task.elapsed_ms = 100;
        CHECK(task.due());

        task.elapsed_ms = 150;
        CHECK(task.due());

        task.enabled = false;
        CHECK_FALSE(task.due());
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Scheduler Basic Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("Scheduler construction") {
    Scheduler sched;

    CHECK(sched.count() == 0);
}

TEST_CASE("Scheduler add tasks") {
    Scheduler sched;

    SUBCASE("add single task") {
        int call_count = 0;
        auto index = sched.add("test_task", 100, [&]() {
            call_count++;
            return true;
        });

        CHECK(index == 0);
        CHECK(sched.count() == 1);
        CHECK(sched.is_enabled(index));
    }

    SUBCASE("add multiple tasks") {
        auto idx0 = sched.add("task0", 100, []() { return true; });
        auto idx1 = sched.add("task1", 200, []() { return true; });
        auto idx2 = sched.add("task2", 300, []() { return true; });

        CHECK(idx0 == 0);
        CHECK(idx1 == 1);
        CHECK(idx2 == 2);
        CHECK(sched.count() == 3);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Scheduler Task Execution Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("Scheduler task execution") {
    Scheduler sched;

    SUBCASE("task executes after interval") {
        int call_count = 0;
        sched.add("test", 100, [&]() {
            call_count++;
            return true;
        });

        sched.update(50);
        CHECK(call_count == 0);

        sched.update(50); // Total: 100ms
        CHECK(call_count == 1);
    }

    SUBCASE("task executes multiple times") {
        int call_count = 0;
        sched.add("test", 100, [&]() {
            call_count++;
            return true;
        });

        sched.update(100);
        CHECK(call_count == 1);

        sched.update(100);
        CHECK(call_count == 2);

        sched.update(100);
        CHECK(call_count == 3);
    }

    SUBCASE("multiple tasks execute independently") {
        int count1 = 0, count2 = 0;

        sched.add("task1", 100, [&]() {
            count1++;
            return true;
        });
        sched.add("task2", 200, [&]() {
            count2++;
            return true;
        });

        sched.update(100);
        CHECK(count1 == 1);
        CHECK(count2 == 0);

        sched.update(100); // Total: 200ms
        CHECK(count1 == 2);
        CHECK(count2 == 1);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Scheduler Enable/Disable Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("Scheduler enable/disable") {
    Scheduler sched;

    int call_count = 0;
    auto idx = sched.add("test", 100, [&]() {
        call_count++;
        return true;
    });

    SUBCASE("disabled task does not execute") {
        sched.disable(idx);
        CHECK_FALSE(sched.is_enabled(idx));

        sched.update(100);
        CHECK(call_count == 0);
    }

    SUBCASE("re-enable task") {
        sched.disable(idx);
        sched.update(100);
        CHECK(call_count == 0);

        sched.enable(idx);
        CHECK(sched.is_enabled(idx));

        sched.update(100);
        CHECK(call_count == 1);
    }

    SUBCASE("enable resets elapsed time") {
        sched.update(50); // Halfway through interval

        sched.enable(idx); // Resets elapsed time

        sched.update(50);
        CHECK(call_count == 0); // Should not execute yet

        sched.update(50); // Total 100ms from enable
        CHECK(call_count == 1);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Scheduler Trigger Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("Scheduler trigger") {
    Scheduler sched;

    int call_count = 0;
    auto idx = sched.add("test", 1000, [&]() {
        call_count++;
        return true;
    });

    SUBCASE("trigger immediate execution") {
        sched.trigger(idx);
        sched.update(1); // Any small update

        CHECK(call_count == 1);
    }

    SUBCASE("trigger works before natural interval") {
        sched.update(100); // Still waiting for 1000ms

        sched.trigger(idx);
        sched.update(1);

        CHECK(call_count == 1);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Scheduler Retry Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("Scheduler retry mechanism") {
    Scheduler sched;

    SUBCASE("successful task resets retry count") {
        int call_count = 0;
        sched.add(
            "test", 100,
            [&]() {
                call_count++;
                return true; // Always succeeds
            },
            3);

        for (int i = 0; i < 5; ++i) {
            sched.update(100);
        }

        CHECK(call_count == 5); // Should execute every time
    }

    SUBCASE("failed task retries") {
        int call_count = 0;
        sched.add(
            "test", 100,
            [&]() {
                call_count++;
                return false; // Always fails
            },
            3);

        sched.update(100);
        CHECK(call_count == 1); // First attempt

        sched.update(100);
        CHECK(call_count == 2); // Second attempt (retry 1)

        sched.update(100);
        CHECK(call_count == 3); // Third attempt (retry 2)

        sched.update(100);
        CHECK(call_count == 3); // Disabled after max retries
    }

    SUBCASE("zero max_retries means unlimited") {
        int call_count = 0;
        auto idx = sched.add(
            "test", 100,
            [&]() {
                call_count++;
                return false; // Always fails
            },
            0); // Unlimited retries

        for (int i = 0; i < 20; ++i) {
            sched.update(100);
        }

        CHECK(call_count == 20); // Should retry indefinitely
        CHECK(sched.is_enabled(idx));
    }

    SUBCASE("successful execution after failures resets retry count") {
        int call_count = 0;
        int success_count = 0;
        sched.add(
            "test", 100,
            [&]() {
                call_count++;
                success_count++;
                return success_count >= 2; // Fail first, succeed second
            },
            5);

        sched.update(100);
        CHECK(call_count == 1); // Fails

        sched.update(100);
        CHECK(call_count == 2); // Succeeds, resets retry count

        sched.update(100);
        CHECK(call_count == 3); // Should continue executing
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Scheduler Clear Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("Scheduler clear") {
    Scheduler sched;

    sched.add("task1", 100, []() { return true; });
    sched.add("task2", 200, []() { return true; });
    sched.add("task3", 300, []() { return true; });

    CHECK(sched.count() == 3);

    sched.clear();

    CHECK(sched.count() == 0);
}

// ═════════════════════════════════════════════════════════════════════════════
// ProcessingFlags Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("ProcessingFlags basic operations") {
    ProcessingFlags flags;

    SUBCASE("initial state") {
        CHECK(flags.pending() == 0);
        CHECK_FALSE(flags.any_pending());
    }

    SUBCASE("set and check flag") {
        flags.set(5);

        CHECK(flags.is_set(5));
        CHECK(flags.any_pending());
        CHECK(flags.pending() == (1U << 5));
    }

    SUBCASE("set multiple flags") {
        flags.set(0);
        flags.set(5);
        flags.set(10);

        CHECK(flags.is_set(0));
        CHECK(flags.is_set(5));
        CHECK(flags.is_set(10));
        CHECK_FALSE(flags.is_set(1));
    }

    SUBCASE("clear flag") {
        flags.set(5);
        CHECK(flags.is_set(5));

        flags.clear(5);
        CHECK_FALSE(flags.is_set(5));
        CHECK_FALSE(flags.any_pending());
    }
}

TEST_CASE("ProcessingFlags handler registration and execution") {
    ProcessingFlags flags;

    SUBCASE("register and execute handler") {
        int call_count = 0;
        flags.register_flag(3, [&]() { call_count++; });

        flags.set(3);
        flags.process();

        CHECK(call_count == 1);
        CHECK_FALSE(flags.is_set(3)); // Should be cleared after processing
    }

    SUBCASE("multiple handlers") {
        int count1 = 0, count2 = 0, count3 = 0;

        flags.register_flag(0, [&]() { count1++; });
        flags.register_flag(1, [&]() { count2++; });
        flags.register_flag(2, [&]() { count3++; });

        flags.set(0);
        flags.set(2);

        flags.process();

        CHECK(count1 == 1);
        CHECK(count2 == 0); // Not set
        CHECK(count3 == 1);
    }

    SUBCASE("process clears flags") {
        int call_count = 0;
        flags.register_flag(5, [&]() { call_count++; });

        flags.set(5);
        flags.process();
        CHECK(call_count == 1);

        flags.process(); // Process again
        CHECK(call_count == 1); // Should not execute again
    }
}

TEST_CASE("ProcessingFlags edge cases") {
    ProcessingFlags flags;

    SUBCASE("set flag without handler") {
        flags.set(10);
        flags.process(); // Should not crash

        CHECK_FALSE(flags.is_set(10)); // Flag cleared even without handler
    }

    SUBCASE("max flags boundary") {
        flags.register_flag(31, []() {});
        flags.set(31);

        CHECK(flags.is_set(31));

        // Beyond max flags should be ignored
        flags.set(32);
        CHECK_FALSE(flags.is_set(32));
    }

    SUBCASE("register handler for same flag multiple times") {
        int count = 0;

        flags.register_flag(5, [&]() { count++; });
        flags.register_flag(5, [&]() { count += 10; }); // Overwrites

        flags.set(5);
        flags.process();

        CHECK(count == 10); // Second handler executed
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Combined Usage Scenarios
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("Scheduler practical scenario") {
    Scheduler sched;

    SUBCASE("periodic message broadcasting") {
        int vt_status_count = 0;
        int heartbeat_count = 0;
        int diagnostic_count = 0;

        sched.add("vt_status", 1000, [&]() {
            vt_status_count++;
            return true;
        });
        sched.add("heartbeat", 500, [&]() {
            heartbeat_count++;
            return true;
        });
        sched.add("diagnostic", 250, [&]() {
            diagnostic_count++;
            return true;
        });

        // Simulate 2 seconds
        for (int i = 0; i < 20; ++i) {
            sched.update(100);
        }

        CHECK(vt_status_count == 2);   // Every 1000ms
        CHECK(heartbeat_count == 4);   // Every 500ms
        CHECK(diagnostic_count == 8);  // Every 250ms
    }

    SUBCASE("request with retry") {
        int attempt_count = 0;
        bool response_received = false;

        auto idx = sched.add(
            "pgn_request", 250,
            [&]() {
                attempt_count++;
                if (attempt_count >= 3) {
                    response_received = true;
                    return true; // Success after 3 attempts
                }
                return false; // Retry
            },
            5);

        // Run until success
        for (int i = 0; i < 10; ++i) {
            sched.update(250);
            if (response_received)
                break;
        }

        CHECK(attempt_count == 3);
        CHECK(response_received);
    }
}

TEST_CASE("ProcessingFlags practical scenario") {
    ProcessingFlags flags;

    SUBCASE("event-driven message handling") {
        bool address_claimed = false;
        bool transport_complete = false;
        bool error_occurred = false;

        const u8 FLAG_ADDRESS_CLAIM = 0;
        const u8 FLAG_TRANSPORT_DONE = 1;
        const u8 FLAG_ERROR = 2;

        flags.register_flag(FLAG_ADDRESS_CLAIM, [&]() { address_claimed = true; });
        flags.register_flag(FLAG_TRANSPORT_DONE, [&]() { transport_complete = true; });
        flags.register_flag(FLAG_ERROR, [&]() { error_occurred = true; });

        // Simulate events
        flags.set(FLAG_ADDRESS_CLAIM);
        flags.set(FLAG_TRANSPORT_DONE);

        flags.process();

        CHECK(address_claimed);
        CHECK(transport_complete);
        CHECK_FALSE(error_occurred);
    }
}
