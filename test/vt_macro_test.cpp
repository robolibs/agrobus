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

TEST_CASE("VTClient - macro registration") {
    IsoNet nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x10).value();

    VTClient client(nm, cf);

    SUBCASE("Register and retrieve macro") {
        VTClient::VTMacro macro;
        macro.macro_id = 100;
        macro.commands.push_back({vt_cmd::HIDE_SHOW, 0x01, 0x00, 0x01, 0xFF, 0xFF, 0xFF, 0xFF});
        macro.commands.push_back({vt_cmd::CHANGE_NUMERIC_VALUE, 0x02, 0x00, 0xFF, 0x2A, 0x00, 0x00, 0x00});

        client.register_macro(std::move(macro));

        auto found = client.get_macro(100);
        REQUIRE(found.has_value());
        CHECK((*found)->macro_id == 100);
        CHECK((*found)->commands.size() == 2);
    }

    SUBCASE("Non-existent macro returns nullopt") {
        CHECK_FALSE(client.get_macro(999).has_value());
    }

    SUBCASE("Update existing macro") {
        VTClient::VTMacro macro1;
        macro1.macro_id = 50;
        macro1.commands.push_back({0x01});
        client.register_macro(macro1);

        VTClient::VTMacro macro2;
        macro2.macro_id = 50;
        macro2.commands.push_back({0x02});
        macro2.commands.push_back({0x03});
        client.register_macro(macro2);

        auto found = client.get_macro(50);
        REQUIRE(found.has_value());
        CHECK((*found)->commands.size() == 2);
    }

    SUBCASE("Multiple macros") {
        for (u16 i = 0; i < 5; ++i) {
            VTClient::VTMacro m;
            m.macro_id = i;
            client.register_macro(std::move(m));
        }
        CHECK(client.macros().size() == 5);
    }
}

TEST_CASE("VTClient - execute_macro requires connection") {
    IsoNet nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x10).value();

    VTClient client(nm, cf);
    CHECK(client.state() == VTState::Disconnected);

    auto result = client.execute_macro(100);
    CHECK_FALSE(result.is_ok());
}

TEST_CASE("VTClient - delete_pool requires connection") {
    IsoNet nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x10).value();

    VTClient client(nm, cf);
    auto result = client.delete_pool("test_pool");
    CHECK_FALSE(result.is_ok());
}

TEST_CASE("VTClient - VT version") {
    IsoNet nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x10).value();

    VTClient client(nm, cf);
    client.set_vt_version_preference(VTVersion::Version4);
    CHECK(client.get_vt_version() == 4);
}

TEST_CASE("VTClient - macro executed event") {
    IsoNet nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x10).value();

    VTClient client(nm, cf);

    bool event_fired = false;
    client.on_macro_executed.subscribe([&](ObjectID id) {
        event_fired = true;
        CHECK(id == 42);
    });

    // Event won't fire since we're not connected
    auto result = client.execute_macro(42);
    CHECK_FALSE(result.is_ok());
    CHECK_FALSE(event_fired); // Not connected, so execute_macro returns early
}
