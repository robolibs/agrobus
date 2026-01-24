#include <doctest/doctest.h>
#include <agrobus/nmea/position.hpp>

using namespace agrobus::nmea;

TEST_CASE("GNSSPosition") {
    SUBCASE("default has no fix") {
        GNSSPosition pos;
        CHECK(!pos.has_fix());
        CHECK(!pos.is_rtk());
    }

    SUBCASE("fix type detection") {
        GNSSPosition pos;
        pos.fix_type = GNSSFixType::RTKFixed;
        CHECK(pos.has_fix());
        CHECK(pos.is_rtk());

        pos.fix_type = GNSSFixType::RTKFloat;
        CHECK(pos.is_rtk());

        pos.fix_type = GNSSFixType::DGNSSFix;
        CHECK(pos.has_fix());
        CHECK(!pos.is_rtk());
    }

    SUBCASE("to_enu conversion") {
        GNSSPosition pos;
        pos.wgs = concord::earth::WGS(48.0001, 11.0001, 500.0);

        dp::Geo ref{48.0, 11.0, 500.0};
        auto enu = pos.to_enu(ref);
        // Should be a small offset from origin
        CHECK(enu.east() != 0.0);
        CHECK(enu.north() != 0.0);
    }

    SUBCASE("to_ned conversion") {
        GNSSPosition pos;
        pos.wgs = concord::earth::WGS(48.001, 11.001, 500.0);

        dp::Geo ref{48.0, 11.0, 500.0};
        auto ned = pos.to_ned(ref);
        CHECK(ned.north() != 0.0);
        CHECK(ned.east() != 0.0);
    }

    SUBCASE("to_ecf conversion") {
        GNSSPosition pos;
        pos.wgs = concord::earth::WGS(48.0, 11.0, 500.0);
        auto ecf = pos.to_ecf();
        // ECEF should be in the range of earth's radius (~6.37e6 m)
        f64 dist = std::sqrt(ecf.x * ecf.x + ecf.y * ecf.y + ecf.z * ecf.z);
        CHECK(dist > 6.3e6);
        CHECK(dist < 6.4e6);
    }
}

TEST_CASE("GNSSBatch") {
    GNSSBatch batch;
    dp::Geo ref{48.0, 11.0, 500.0};

    for (i32 i = 0; i < 5; ++i) {
        GNSSPosition pos;
        pos.wgs = concord::earth::WGS(48.0 + i * 0.0001, 11.0 + i * 0.0001, 500.0);
        batch.positions.push_back(pos);
    }

    SUBCASE("batch to_enu") {
        auto enu_vec = batch.to_enu_batch(ref);
        CHECK(enu_vec.size() == 5);
        // First point should be near origin
        CHECK(std::abs(enu_vec[0].east()) < 20.0);
        CHECK(std::abs(enu_vec[0].north()) < 20.0);
        // Last point should be further away
        CHECK(std::abs(enu_vec[4].east()) > std::abs(enu_vec[0].east()));
    }

    SUBCASE("batch to_ned") {
        auto ned_vec = batch.to_ned_batch(ref);
        CHECK(ned_vec.size() == 5);
    }

    SUBCASE("batch to_ecf") {
        auto ecf_vec = batch.to_ecf_batch();
        CHECK(ecf_vec.size() == 5);
    }
}
