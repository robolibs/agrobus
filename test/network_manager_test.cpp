#include <doctest/doctest.h>
#include <agrobus/net/network_manager.hpp>

using namespace agrobus::net;

TEST_CASE("IsoNet creation") {
    SUBCASE("default config") {
        IsoNet nm;
        CHECK(nm.internal_cfs().empty());
        CHECK(nm.partner_cfs().empty());
    }

    SUBCASE("custom config") {
        NetworkConfig cfg;
        cfg.num_ports = 2;
        cfg.enable_bus_load = false;
        IsoNet nm(cfg);
        CHECK(nm.bus_load(0) == 0.0f);
    }
}

TEST_CASE("Internal CF creation") {
    IsoNet nm;
    Name name;
    name.set_identity_number(42);

    auto result = nm.create_internal(name, 0, 0x28);
    CHECK(result.is_ok());
    CHECK(nm.internal_cfs().size() == 1);
    CHECK(result.value()->name().identity_number() == 42);
    CHECK(result.value()->port() == 0);
}

TEST_CASE("Partner CF creation") {
    IsoNet nm;
    dp::Vector<NameFilter> filters = {
        {NameFilterField::FunctionCode, 100},
        {NameFilterField::IndustryGroup, 2}
    };

    auto result = nm.create_partner(0, std::move(filters));
    CHECK(result.is_ok());
    CHECK(nm.partner_cfs().size() == 1);
}

TEST_CASE("PGN callback registration") {
    IsoNet nm;
    bool called = false;
    nm.register_pgn_callback(PGN_VEHICLE_SPEED, [&](const Message&) {
        called = true;
    });
    // Can't easily test dispatch without a driver, but registration should work
    CHECK(!called);
}

TEST_CASE("Send without driver fails") {
    IsoNet nm;
    Name name;
    auto cf_result = nm.create_internal(name, 0, 0x28);
    auto* cf = cf_result.value();
    cf->set_address(0x28);
    cf->set_state(CFState::Online);

    dp::Vector<u8> data = {0x01, 0x02};
    auto result = nm.send(PGN_VEHICLE_SPEED, data, cf);
    CHECK(result.is_err()); // No driver set
}

TEST_CASE("Control functions list") {
    IsoNet nm;
    Name n1, n2;
    nm.create_internal(n1, 0, 0x10);
    dp::Vector<NameFilter> filters = {{NameFilterField::FunctionCode, 1}};
    nm.create_partner(0, std::move(filters));

    auto cfs = nm.control_functions();
    CHECK(cfs.size() == 2);
}
