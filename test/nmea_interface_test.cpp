#include <doctest/doctest.h>
#include <agrobus/nmea/interface.hpp>

using namespace agrobus::nmea;

TEST_CASE("NMEAInterface construction") {
    IsoNet nm;
    Name name;
    auto cf_result = nm.create_internal(name, 0, 0x28);
    auto* cf = cf_result.value();

    NMEAInterface nmea_if(nm, cf);
    nmea_if.initialize();

    CHECK(!nmea_if.latest_position().has_value());
}

TEST_CASE("NMEAInterface event subscription") {
    IsoNet nm;
    Name name;
    auto cf_result = nm.create_internal(name, 0, 0x28);
    auto* cf = cf_result.value();

    NMEAInterface nmea_if(nm, cf);
    nmea_if.initialize();

    bool pos_received = false;
    bool cog_received = false;
    bool sog_received = false;

    nmea_if.on_position.subscribe([&](const GNSSPosition&) {
        pos_received = true;
    });
    nmea_if.on_cog.subscribe([&](f64) {
        cog_received = true;
    });
    nmea_if.on_sog.subscribe([&](f64) {
        sog_received = true;
    });

    // Events registered but not triggered
    CHECK(!pos_received);
    CHECK(!cog_received);
    CHECK(!sog_received);
}

TEST_CASE("NMEAInterface send_position") {
    IsoNet nm;
    Name name;
    auto cf_result = nm.create_internal(name, 0, 0x28);
    auto* cf = cf_result.value();

    NMEAInterface nmea_if(nm, cf);
    nmea_if.initialize();

    GNSSPosition pos;
    pos.wgs = concord::earth::WGS(48.123, 11.456, 500.0);
    pos.fix_type = GNSSFixType::RTKFixed;

    // Will fail because no driver set, but validates encoding path
    auto result = nmea_if.send_position(pos);
    CHECK(result.is_err()); // No driver
}

TEST_CASE("NMEAInterface send_cog_sog") {
    IsoNet nm;
    Name name;
    auto cf_result = nm.create_internal(name, 0, 0x28);
    auto* cf = cf_result.value();

    NMEAInterface nmea_if(nm, cf);
    nmea_if.initialize();

    auto result = nmea_if.send_cog_sog(1.5708, 5.0); // 90deg, 5 m/s
    CHECK(result.is_err()); // No driver
}
