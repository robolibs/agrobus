#include <doctest/doctest.h>
#include <agrobus/nmea/position.hpp>
#include <cmath>

using namespace agrobus::nmea;

TEST_CASE("WGS84 to ENU roundtrip") {
    GNSSPosition pos;
    pos.wgs = concord::earth::WGS(48.1234, 11.5678, 520.0);

    dp::Geo ref{48.1230, 11.5670, 515.0};
    auto enu = pos.to_enu(ref);

    // At ~48deg latitude:
    // 0.0004 deg lat ≈ 44.5m north
    // 0.0008 deg lon ≈ 59.6m east (cos(48deg) factor)
    CHECK(std::abs(enu.east()) > 10.0);
    CHECK(std::abs(enu.north()) > 10.0);
}

TEST_CASE("WGS84 to NED conversion") {
    GNSSPosition pos;
    pos.wgs = concord::earth::WGS(48.0001, 11.0001, 500.0);

    dp::Geo ref{48.0, 11.0, 500.0};
    auto ned = pos.to_ned(ref);

    // Small offset should give small NED values
    CHECK(ned.north() > 0.0);
    CHECK(ned.east() > 0.0);
    CHECK(std::abs(ned.down()) < 1.0);
}

TEST_CASE("WGS84 to ECEF conversion") {
    GNSSPosition pos;
    pos.wgs = concord::earth::WGS(0.0, 0.0, 0.0); // equator, prime meridian

    auto ecf = pos.to_ecf();
    // At equator/prime meridian, ECEF X should be ~6378137m (Earth radius)
    CHECK(ecf.x > 6.3e6);
    CHECK(ecf.x < 6.4e6);
    CHECK(std::abs(ecf.y) < 1.0);
    CHECK(std::abs(ecf.z) < 1.0);
}

TEST_CASE("GNSSBatch to ENU batch") {
    GNSSBatch batch;

    dp::Geo ref{48.0, 11.0, 500.0};

    for (i32 i = 0; i < 5; ++i) {
        GNSSPosition pos;
        pos.wgs = concord::earth::WGS(48.0 + i * 0.0001, 11.0 + i * 0.0001, 500.0);
        batch.positions.push_back(pos);
    }

    auto enu_results = batch.to_enu_batch(ref);
    CHECK(enu_results.size() == 5);

    // First point at reference should be near zero
    CHECK(std::abs(enu_results[0].east()) < 0.1);
    CHECK(std::abs(enu_results[0].north()) < 0.1);

    // Subsequent points should be further away
    CHECK(enu_results[4].north() > enu_results[1].north());
}

TEST_CASE("GNSSBatch to NED batch") {
    GNSSBatch batch;
    dp::Geo ref{48.0, 11.0, 500.0};

    GNSSPosition p1;
    p1.wgs = concord::earth::WGS(48.001, 11.001, 500.0);
    batch.positions.push_back(p1);

    GNSSPosition p2;
    p2.wgs = concord::earth::WGS(48.002, 11.002, 500.0);
    batch.positions.push_back(p2);

    auto ned_results = batch.to_ned_batch(ref);
    CHECK(ned_results.size() == 2);
    CHECK(ned_results[1].north() > ned_results[0].north());
}

TEST_CASE("GNSSBatch to ECEF batch") {
    GNSSBatch batch;

    GNSSPosition p1;
    p1.wgs = concord::earth::WGS(0.0, 0.0, 0.0);
    batch.positions.push_back(p1);

    GNSSPosition p2;
    p2.wgs = concord::earth::WGS(90.0, 0.0, 0.0); // North pole
    batch.positions.push_back(p2);

    auto ecf_results = batch.to_ecf_batch();
    CHECK(ecf_results.size() == 2);
    // Equator point has large X
    CHECK(ecf_results[0].x > 6.3e6);
    // North pole has large Z
    CHECK(ecf_results[1].z > 6.3e6);
}
