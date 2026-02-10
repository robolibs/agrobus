#include <doctest/doctest.h>
#include <agrobus/net/bus_load.hpp>

using namespace agrobus::net;

// ═════════════════════════════════════════════════════════════════════════════
// Basic Functionality Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("BusLoad initialization") {
    BusLoad bus_load;

    SUBCASE("initial load is zero") {
        CHECK(bus_load.load_percent() == 0.0f);
    }
}

TEST_CASE("BusLoad frame addition") {
    BusLoad bus_load;

    SUBCASE("add single frame with default DLC") {
        bus_load.add_frame();
        bus_load.update(100); // One sample period

        f32 load = bus_load.load_percent();
        CHECK(load > 0.0f);
        CHECK(load < 100.0f);
    }

    SUBCASE("add frame with specific DLC") {
        bus_load.add_frame(8);
        bus_load.update(100);

        f32 load = bus_load.load_percent();
        CHECK(load > 0.0f);
    }

    SUBCASE("add multiple frames in one period") {
        for (int i = 0; i < 10; ++i) {
            bus_load.add_frame(8);
        }
        bus_load.update(100);

        f32 load = bus_load.load_percent();
        CHECK(load > 0.0f);
        CHECK(load < 100.0f);
    }
}

TEST_CASE("BusLoad time window") {
    BusLoad bus_load;

    SUBCASE("multiple sample periods") {
        // Add frames in first period
        for (int i = 0; i < 5; ++i) {
            bus_load.add_frame(8);
        }
        bus_load.update(100);
        f32 load1 = bus_load.load_percent();

        // Add frames in second period
        for (int i = 0; i < 5; ++i) {
            bus_load.add_frame(8);
        }
        bus_load.update(100);
        f32 load2 = bus_load.load_percent();

        // Both periods should contribute
        CHECK(load1 > 0.0f);
        CHECK(load2 > 0.0f);
    }

    SUBCASE("update without full period elapsed") {
        bus_load.add_frame(8);
        bus_load.update(50); // Half period

        // Should not have recorded yet
        f32 load = bus_load.load_percent();
        CHECK(load == 0.0f);

        // Complete the period
        bus_load.update(50);
        load = bus_load.load_percent();
        CHECK(load > 0.0f);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Load Calculation Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("BusLoad percentage calculation") {
    BusLoad bus_load;

    SUBCASE("low load scenario") {
        // Add 1 frame per 100ms period (10 frames/sec)
        for (int i = 0; i < 10; ++i) {
            bus_load.add_frame(8);
            bus_load.update(100);
        }

        f32 load = bus_load.load_percent();
        CHECK(load > 0.0f);
        CHECK(load < 10.0f); // Should be low load
    }

    SUBCASE("medium load scenario") {
        // Add 50 frames per 100ms period (500 frames/sec)
        for (int i = 0; i < 10; ++i) {
            for (int j = 0; j < 50; ++j) {
                bus_load.add_frame(8);
            }
            bus_load.update(100);
        }

        f32 load = bus_load.load_percent();
        CHECK(load > 10.0f);
        CHECK(load < 80.0f);
    }

    SUBCASE("varying DLC affects load") {
        BusLoad load_small, load_large;

        // Small frames (1 byte DLC)
        for (int i = 0; i < 10; ++i) {
            load_small.add_frame(1);
        }
        load_small.update(100);

        // Large frames (8 byte DLC)
        for (int i = 0; i < 10; ++i) {
            load_large.add_frame(8);
        }
        load_large.update(100);

        // Larger DLC should result in higher load
        CHECK(load_large.load_percent() > load_small.load_percent());
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Window Management Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("BusLoad sliding window") {
    BusLoad bus_load;

    SUBCASE("window fills correctly") {
        // Fill less than window size (100 samples)
        for (int i = 0; i < 50; ++i) {
            bus_load.add_frame(8);
            bus_load.update(100);
        }

        f32 load = bus_load.load_percent();
        CHECK(load > 0.0f);
    }

    SUBCASE("window wraps around") {
        // Fill more than window size
        for (int i = 0; i < 150; ++i) {
            bus_load.add_frame(8);
            bus_load.update(100);
        }

        f32 load = bus_load.load_percent();
        CHECK(load > 0.0f);
    }

    SUBCASE("old samples are discarded") {
        // Add high load initially
        for (int i = 0; i < 100; ++i) {
            for (int j = 0; j < 50; ++j) {
                bus_load.add_frame(8);
            }
            bus_load.update(100);
        }
        f32 high_load = bus_load.load_percent();

        // Add low load for another full window
        for (int i = 0; i < 100; ++i) {
            bus_load.add_frame(8);
            bus_load.update(100);
        }
        f32 low_load = bus_load.load_percent();

        // New low load should be significantly less
        CHECK(low_load < high_load);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Reset Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("BusLoad reset") {
    BusLoad bus_load;

    SUBCASE("reset clears load") {
        // Add some load
        for (int i = 0; i < 10; ++i) {
            bus_load.add_frame(8);
        }
        bus_load.update(100);

        CHECK(bus_load.load_percent() > 0.0f);

        // Reset
        bus_load.reset();
        CHECK(bus_load.load_percent() == 0.0f);
    }

    SUBCASE("can add frames after reset") {
        // Add load, reset, add load again
        bus_load.add_frame(8);
        bus_load.update(100);

        bus_load.reset();

        bus_load.add_frame(8);
        bus_load.update(100);

        CHECK(bus_load.load_percent() > 0.0f);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Edge Cases Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("BusLoad edge cases") {
    BusLoad bus_load;

    SUBCASE("zero DLC frame") {
        bus_load.add_frame(0);
        bus_load.update(100);

        f32 load = bus_load.load_percent();
        CHECK(load > 0.0f); // Still has overhead bits
    }

    SUBCASE("max DLC frame") {
        bus_load.add_frame(8);
        bus_load.update(100);

        CHECK(bus_load.load_percent() > 0.0f);
    }

    SUBCASE("no frames added") {
        bus_load.update(100);
        CHECK(bus_load.load_percent() == 0.0f);
    }

    SUBCASE("multiple updates without frames") {
        for (int i = 0; i < 10; ++i) {
            bus_load.update(100);
        }
        CHECK(bus_load.load_percent() == 0.0f);
    }

    SUBCASE("accumulating timer") {
        bus_load.add_frame(8);

        // Update with time that doesn't reach sample period (90ms < 100ms)
        bus_load.update(30);
        bus_load.update(30);
        bus_load.update(30);
        CHECK(bus_load.load_percent() == 0.0f); // Not yet recorded

        // Complete the period
        bus_load.update(10); // Total: 100ms
        CHECK(bus_load.load_percent() > 0.0f); // Now recorded
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Realistic Scenarios
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("BusLoad realistic scenarios") {
    BusLoad bus_load;

    SUBCASE("typical ISOBUS scenario") {
        // Simulate typical ISOBUS traffic: ~100 messages per second
        for (int second = 0; second < 5; ++second) {
            for (int sample = 0; sample < 10; ++sample) {
                for (int msg = 0; msg < 10; ++msg) {
                    bus_load.add_frame(8);
                }
                bus_load.update(100);
            }
        }

        f32 load = bus_load.load_percent();
        CHECK(load > 0.0f);
        CHECK(load < 50.0f); // Typical load should be moderate
    }

    SUBCASE("burst traffic pattern") {
        // Simulate burst: many messages then quiet period
        for (int i = 0; i < 5; ++i) {
            // Burst
            for (int j = 0; j < 100; ++j) {
                bus_load.add_frame(8);
            }
            bus_load.update(100);

            // Quiet
            for (int j = 0; j < 5; ++j) {
                bus_load.update(100);
            }
        }

        f32 load = bus_load.load_percent();
        CHECK(load > 0.0f);
    }
}
