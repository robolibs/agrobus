#include <doctest/doctest.h>
#include <agrobus.hpp>

using namespace agrobus::net;
using namespace agrobus::j1939;
using namespace agrobus::isobus;
using namespace agrobus::nmea;
using namespace agrobus::isobus::vt;
using namespace agrobus::isobus::tc;
using namespace agrobus::isobus::sc;
using namespace agrobus::isobus::implement;
using namespace agrobus::isobus::fs;

TEST_CASE("TimServer - PTO control") {
    IsoNet nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x10).value();

    TimServer server(nm, cf);
    server.initialize();

    SUBCASE("Set and get front PTO") {
        server.set_front_pto(true, true, 540);
        auto pto = server.get_front_pto();
        CHECK(pto.engaged);
        CHECK(pto.cw_direction);
        CHECK(pto.speed == 540);
    }

    SUBCASE("Set and get rear PTO") {
        server.set_rear_pto(false, false, 1000);
        auto pto = server.get_rear_pto();
        CHECK_FALSE(pto.engaged);
        CHECK_FALSE(pto.cw_direction);
        CHECK(pto.speed == 1000);
    }

    SUBCASE("PTO event fires") {
        bool fired = false;
        server.on_front_pto_changed.subscribe([&](const PTOState& state) {
            fired = true;
            CHECK(state.speed == 750);
        });
        server.set_front_pto(true, true, 750);
        CHECK(fired);
    }
}

TEST_CASE("TimServer - Hitch control") {
    IsoNet nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x10).value();

    TimServer server(nm, cf);

    SUBCASE("Set and get front hitch") {
        server.set_front_hitch(true, 5000);
        auto hitch = server.get_front_hitch();
        CHECK(hitch.motion_enabled);
        CHECK(hitch.position == 5000);
    }

    SUBCASE("Set and get rear hitch") {
        server.set_rear_hitch(false, 10000);
        auto hitch = server.get_rear_hitch();
        CHECK_FALSE(hitch.motion_enabled);
        CHECK(hitch.position == 10000);
    }
}

TEST_CASE("TimServer - Aux valve control") {
    IsoNet nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x10).value();

    TimServer server(nm, cf);

    SUBCASE("Set and get aux valve") {
        server.set_aux_valve_capabilities(0, true, true);
        server.set_aux_valve(0, true, 500);
        auto valve = server.get_aux_valve(0);
        CHECK(valve.state_supported);
        CHECK(valve.flow_supported);
        CHECK(valve.state);
        CHECK(valve.flow == 500);
    }

    SUBCASE("Out of range valve returns error") {
        auto result = server.set_aux_valve(32, true, 100);
        CHECK_FALSE(result.is_ok());
    }

    SUBCASE("Valve event fires") {
        bool fired = false;
        server.on_aux_valve_changed.subscribe([&](u8 idx, const AuxValve& v) {
            fired = true;
            CHECK(idx == 5);
            CHECK(v.flow == 250);
        });
        server.set_aux_valve(5, false, 250);
        CHECK(fired);
    }
}

TEST_CASE("TimClient - initialization") {
    IsoNet nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x10).value();

    TimClient client(nm, cf);
    client.initialize();

    CHECK(client.get_front_pto().speed == 0);
    CHECK(client.get_rear_hitch().position == 0);
}

TEST_CASE("TimServer - update loop") {
    IsoNet nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x10).value();

    TimServer server(nm, cf);
    server.set_front_pto(true, true, 540);
    server.set_rear_hitch(false, 7500);

    // Should not crash
    for (i32 i = 0; i < 20; ++i) {
        server.update(50);
    }
}
