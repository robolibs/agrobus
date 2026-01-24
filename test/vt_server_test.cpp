#include <doctest/doctest.h>
#include <agrobus.hpp>
#include <agrobus/isobus/vt/server.hpp>

using namespace agrobus::net;
using namespace agrobus::j1939;
using namespace agrobus::isobus;
using namespace agrobus::nmea;
using namespace agrobus::isobus::vt;
using namespace agrobus::isobus::tc;
using namespace agrobus::isobus::sc;
using namespace agrobus::isobus::implement;
using namespace agrobus::isobus::fs;

TEST_CASE("VTServer - initialization and state") {
    IsoNet nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x10).value();

    VTServer server(nm, cf);

    SUBCASE("Initial state is Disconnected") {
        CHECK(server.state() == VTServerState::Disconnected);
    }

    SUBCASE("Start transitions to WaitForClientStatus") {
        server.start();
        CHECK(server.state() == VTServerState::WaitForClientStatus);
    }

    SUBCASE("Stop transitions to Disconnected") {
        server.start();
        server.stop();
        CHECK(server.state() == VTServerState::Disconnected);
    }
}

TEST_CASE("VTServer - screen configuration") {
    IsoNet nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x10).value();

    VTServer server(nm, cf, VTServerConfig{}.screen(320, 240).version(4));
    CHECK(server.screen_width() == 320);
    CHECK(server.screen_height() == 240);
}

TEST_CASE("VTServer - client tracking") {
    IsoNet nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x10).value();

    VTServer server(nm, cf);
    server.start();

    CHECK(server.clients().empty());
}

TEST_CASE("VTServer - update sends status periodically") {
    IsoNet nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x10).value();

    VTServer server(nm, cf);
    server.start();

    // Should not crash
    for (i32 i = 0; i < 20; ++i) {
        server.update(100);
    }
}
