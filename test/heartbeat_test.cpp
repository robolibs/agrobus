#include <doctest/doctest.h>
#include <agrobus/j1939/heartbeat.hpp>

using namespace agrobus::j1939;

TEST_CASE("HeartbeatProtocol") {
    IsoNet nm;
    Name name;
    auto cf_result = nm.create_internal(name, 0, 0x28);
    auto* cf = cf_result.value();

    HeartbeatProtocol hb(nm, cf, HeartbeatConfig{}.interval(100));

    SUBCASE("initially disabled") {
        CHECK(!hb.is_enabled());
    }

    SUBCASE("enable/disable") {
        hb.enable();
        CHECK(hb.is_enabled());
        hb.disable();
        CHECK(!hb.is_enabled());
    }

    SUBCASE("interval") {
        CHECK(hb.interval() == 100);
        hb.set_interval(200);
        CHECK(hb.interval() == 200);
    }

    SUBCASE("missed heartbeat detection") {
        hb.track(0x30);

        bool missed = false;
        u32 miss_count = 0;
        hb.on_heartbeat_missed.subscribe([&](Address, u32 count) {
            missed = true;
            miss_count = count;
        });

        // Update for 3x interval without receiving heartbeat
        hb.update(301);
        CHECK(missed);
        CHECK(miss_count == 1);
    }
}
