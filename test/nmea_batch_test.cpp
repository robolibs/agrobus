#include <doctest/doctest.h>
#include <agrobus/nmea/position.hpp>
#include <cmath>

using namespace agrobus::nmea;

TEST_CASE("GNSSBatch empty") {
    GNSSBatch batch;
    dp::Geo ref{0.0, 0.0, 0.0};

    auto enu = batch.to_enu_batch(ref);
    CHECK(enu.empty());

    auto ned = batch.to_ned_batch(ref);
    CHECK(ned.empty());

    auto ecf = batch.to_ecf_batch();
    CHECK(ecf.empty());
}

TEST_CASE("GNSSBatch single position") {
    GNSSBatch batch;
    GNSSPosition pos;
    pos.wgs = concord::earth::WGS(48.0, 11.0, 500.0);
    batch.positions.push_back(pos);

    dp::Geo ref{48.0, 11.0, 500.0};
    auto enu = batch.to_enu_batch(ref);
    CHECK(enu.size() == 1);
    // Position at reference should be near zero
    CHECK(std::abs(enu[0].east()) < 0.01);
    CHECK(std::abs(enu[0].north()) < 0.01);
}

TEST_CASE("GNSSBatch preserves order") {
    GNSSBatch batch;
    dp::Geo ref{48.0, 11.0, 500.0};

    for (i32 i = 0; i < 10; ++i) {
        GNSSPosition pos;
        pos.wgs = concord::earth::WGS(48.0 + i * 0.001, 11.0, 500.0);
        batch.positions.push_back(pos);
    }

    auto enu = batch.to_enu_batch(ref);
    CHECK(enu.size() == 10);

    // Each successive point should be further north
    for (usize i = 1; i < enu.size(); ++i) {
        CHECK(enu[i].north() > enu[i - 1].north());
    }
}

TEST_CASE("GNSSBatch large batch") {
    GNSSBatch batch;
    dp::Geo ref{48.0, 11.0, 500.0};

    for (i32 i = 0; i < 100; ++i) {
        GNSSPosition pos;
        pos.wgs = concord::earth::WGS(
            48.0 + i * 0.0001,
            11.0 + i * 0.0001,
            500.0 + i * 0.1
        );
        batch.positions.push_back(pos);
    }

    auto enu = batch.to_enu_batch(ref);
    CHECK(enu.size() == 100);

    auto ned = batch.to_ned_batch(ref);
    CHECK(ned.size() == 100);

    auto ecf = batch.to_ecf_batch();
    CHECK(ecf.size() == 100);
}

TEST_CASE("GNSSBatch ENU vs NED consistency") {
    GNSSBatch batch;
    GNSSPosition pos;
    pos.wgs = concord::earth::WGS(48.001, 11.001, 500.0);
    batch.positions.push_back(pos);

    dp::Geo ref{48.0, 11.0, 500.0};
    auto enu = batch.to_enu_batch(ref);
    auto ned = batch.to_ned_batch(ref);

    // ENU north ≈ NED north, ENU east ≈ NED east
    CHECK(enu[0].north() == doctest::Approx(ned[0].north()).epsilon(0.01));
    CHECK(enu[0].east() == doctest::Approx(ned[0].east()).epsilon(0.01));
}
