#include <doctest/doctest.h>
#include <agrobus/isobus/tc/geo.hpp>
#include <agrobus.hpp>

using namespace agrobus::net;
using namespace agrobus::j1939;
using namespace agrobus::isobus;
using namespace agrobus::nmea;
using namespace agrobus::isobus::tc;
using namespace agrobus::isobus::vt;
using namespace agrobus::isobus::sc;
using namespace agrobus::isobus::implement;
using namespace agrobus::isobus::fs;

TEST_CASE("TCGEOInterface - initialization") {
    IsoNet nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x10).value();

    TCGEOInterface geo(nm, cf);
    geo.initialize();

    CHECK(geo.prescription_maps().empty());
    CHECK_FALSE(geo.current_position().has_value());
}

TEST_CASE("TCGEOInterface - prescription map management") {
    IsoNet nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x10).value();

    TCGEOInterface geo(nm, cf);

    SUBCASE("Add prescription map") {
        PrescriptionMap map;
        map.structure_label = "FIELD_A";
        map.zones.push_back({
            {{concord::earth::WGS(48.0, 11.0, 0), concord::earth::WGS(48.1, 11.0, 0),
              concord::earth::WGS(48.1, 11.1, 0), concord::earth::WGS(48.0, 11.1, 0)}},
            200
        });

        geo.add_prescription_map(std::move(map));
        CHECK(geo.prescription_maps().size() == 1);
        CHECK(geo.prescription_maps()[0].structure_label == "FIELD_A");
    }

    SUBCASE("Clear prescription maps") {
        PrescriptionMap map;
        map.structure_label = "TEST";
        geo.add_prescription_map(std::move(map));
        geo.clear_prescription_maps();
        CHECK(geo.prescription_maps().empty());
    }
}

TEST_CASE("TCGEOInterface - position update") {
    IsoNet nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x10).value();

    TCGEOInterface geo(nm, cf);

    bool position_updated = false;
    geo.on_position_update.subscribe([&](const GeoPoint& pt) {
        position_updated = true;
        CHECK(pt.position.latitude == doctest::Approx(48.05));
    });

    GeoPoint point;
    point.position = concord::earth::WGS(48.05, 11.05, 400);
    point.timestamp_us = 1000000;

    geo.set_position(point);
    CHECK(position_updated);
    CHECK(geo.current_position().has_value());
}

TEST_CASE("TCGEOInterface - rate at position") {
    IsoNet nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x10).value();

    TCGEOInterface geo(nm, cf);

    // Create a simple square zone
    PrescriptionMap map;
    map.structure_label = "ZONE_TEST";
    PrescriptionZone zone;
    zone.boundary = {
        concord::earth::WGS(48.0, 11.0, 0),
        concord::earth::WGS(48.1, 11.0, 0),
        concord::earth::WGS(48.1, 11.1, 0),
        concord::earth::WGS(48.0, 11.1, 0)
    };
    zone.application_rate = 300;
    map.zones.push_back(zone);
    geo.add_prescription_map(std::move(map));

    SUBCASE("Point inside zone") {
        concord::earth::WGS inside(48.05, 11.05, 0);
        auto rate = geo.get_rate_at_position(inside);
        CHECK(rate.has_value());
        CHECK(*rate == 300);
    }

    SUBCASE("Point outside zone") {
        concord::earth::WGS outside(49.0, 12.0, 0);
        auto rate = geo.get_rate_at_position(outside);
        CHECK_FALSE(rate.has_value());
    }
}
