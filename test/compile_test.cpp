#include <doctest/doctest.h>

// Include the master header to verify all headers compile together
#include <agrobus.hpp>

TEST_CASE("All headers compile") {
    // If this compiles, all headers are syntactically valid
    CHECK(true);
}

TEST_CASE("Core types available") {
    agrobus::net::Address addr = 0x28;
    agrobus::net::PGN pgn = agrobus::net::PGN_DM1;
    agrobus::net::Priority prio = agrobus::net::Priority::Default;
    CHECK(addr == 0x28);
    CHECK(pgn == 0xFECA);
    CHECK(prio == agrobus::net::Priority::Default);
}

TEST_CASE("Name constructible") {
    agrobus::net::Name name;
    name.set_identity_number(42);
    name.set_manufacturer_code(100);
    CHECK(name.identity_number() == 42);
}

TEST_CASE("Frame constructible") {
    agrobus::net::Frame f;
    CHECK(f.length == 8);
}

TEST_CASE("Message constructible") {
    agrobus::net::Message msg;
    msg.pgn = agrobus::net::PGN_VEHICLE_SPEED;
    msg.data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    CHECK(msg.get_u8(0) == 0x01);
}

TEST_CASE("IsoNet constructible") {
    agrobus::net::IsoNet nm;
    CHECK(nm.internal_cfs().empty());
}

TEST_CASE("TransportProtocol constructible") {
    agrobus::net::TransportProtocol tp;
    CHECK(tp.active_sessions().empty());
}

TEST_CASE("DiagnosticProtocol constructible") {
    agrobus::net::IsoNet nm;
    agrobus::net::Name name;
    auto cf = nm.create_internal(name, 0);
    agrobus::j1939::DiagnosticProtocol diag(nm, cf.value());
    CHECK(diag.active_dtcs().empty());
}

TEST_CASE("NMEA position constructible") {
    agrobus::nmea::GNSSPosition pos;
    pos.wgs = concord::earth::WGS(48.0, 11.0, 500.0);
    CHECK(pos.wgs.latitude == doctest::Approx(48.0));
}
