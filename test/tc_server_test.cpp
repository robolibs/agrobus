#include <doctest/doctest.h>
#include <agrobus.hpp>
#include <agrobus/isobus/tc/server.hpp>

using namespace agrobus::net;
using namespace agrobus::j1939;
using namespace agrobus::isobus;
using namespace agrobus::nmea;
using namespace agrobus::isobus::tc;
using namespace agrobus::isobus::vt;
using namespace agrobus::isobus::sc;
using namespace agrobus::isobus::implement;
using namespace agrobus::isobus::fs;

TEST_CASE("TaskControllerServer - initialization and state") {
    IsoNet nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x10).value();

    TaskControllerServer server(nm, cf);

    SUBCASE("Initial state is Disconnected") {
        CHECK(server.state() == TCServerState::Disconnected);
    }

    SUBCASE("Start transitions to WaitForClients") {
        server.start();
        CHECK(server.state() == TCServerState::WaitForClients);
    }

    SUBCASE("Stop transitions to Disconnected") {
        server.start();
        server.stop();
        CHECK(server.state() == TCServerState::Disconnected);
    }
}

TEST_CASE("TaskControllerServer - configuration") {
    IsoNet nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x10).value();

    TaskControllerServer server(nm, cf, TCServerConfig{}
        .number(1)
        .version(4)
        .booms(3)
        .sections(16)
        .channels(8)
        .options(static_cast<u8>(ServerOptions::SupportsDocumentation) |
                 static_cast<u8>(ServerOptions::SupportsImplementSectionControl)));
}

TEST_CASE("TaskControllerServer - callbacks") {
    IsoNet nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x10).value();

    TaskControllerServer server(nm, cf);

    bool value_requested = false;
    server.on_value_request([&](ElementNumber, DDI, TCClientInfo*) -> Result<i32> {
        value_requested = true;
        return Result<i32>::ok(42);
    });

    bool value_received = false;
    server.on_value_received([&](ElementNumber, DDI, i32, TCClientInfo*) -> Result<ProcessDataAcknowledgeErrorCodes> {
        value_received = true;
        return Result<ProcessDataAcknowledgeErrorCodes>::ok(ProcessDataAcknowledgeErrorCodes::NoError);
    });

    // Callbacks set, nothing triggered yet
    CHECK_FALSE(value_requested);
    CHECK_FALSE(value_received);
}

TEST_CASE("TaskControllerServer - update sends status") {
    IsoNet nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x10).value();

    TaskControllerServer server(nm, cf);
    server.start();

    // Should not crash
    for (i32 i = 0; i < 30; ++i) {
        server.update(100);
    }
}
