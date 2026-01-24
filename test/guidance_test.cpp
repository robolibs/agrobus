#include <doctest/doctest.h>
#include <agrobus/isobus/guidance.hpp>

using namespace agrobus::isobus;

TEST_CASE("GuidanceData") {
    GuidanceData gd;
    CHECK(!gd.curvature.has_value());
    CHECK(!gd.heading_rad.has_value());
    CHECK(!gd.cross_track_m.has_value());

    gd.curvature = 0.01; // 1/100m radius
    CHECK(*gd.curvature == doctest::Approx(0.01));
}

TEST_CASE("GuidanceInterface creation") {
    IsoNet nm;
    Name name;
    auto cf_result = nm.create_internal(name, 0, 0x28);
    auto* cf = cf_result.value();

    GuidanceInterface gi(nm, cf);
    gi.initialize();

    CHECK(!gi.latest_machine().has_value());
    CHECK(!gi.latest_system().has_value());
}
