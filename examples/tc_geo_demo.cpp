#include <agrobus.hpp>
#include <wirebit/can/socketcan_link.hpp>
#include <wirebit/can/can_endpoint.hpp>
#include <agrobus/isobus/tc/geo.hpp>
#include <echo/echo.hpp>

using namespace agrobus::net;
using namespace agrobus::j1939;
using namespace agrobus::isobus;
using namespace agrobus::nmea;
using namespace agrobus::isobus::tc;
using namespace agrobus::isobus::vt;
using namespace agrobus::isobus::sc;
using namespace agrobus::isobus::implement;
using namespace agrobus::isobus::fs;

int main() {
    echo::info("=== TC-GEO Demo ===");

    auto link_result = wirebit::SocketCanLink::create({
        .interface_name = "vcan_tcgeo",
        .create_if_missing = true
    });
    if (!link_result.is_ok()) { echo::error("SocketCanLink failed"); return 1; }
    auto link = std::make_shared<wirebit::SocketCanLink>(std::move(link_result.value()));
    wirebit::CanEndpoint endpoint(std::static_pointer_cast<wirebit::Link>(link),
                                  wirebit::CanConfig{.bitrate = 250000}, 1);

    IsoNet nm;
    nm.set_endpoint(0, &endpoint);

    auto* cf = nm.create_internal(
        Name::build().set_identity_number(1).set_manufacturer_code(100).set_self_configurable(true),
        0, 0x28).value();

    TCGEOInterface geo(nm, cf);
    geo.initialize();

    // Create a prescription map with two zones
    PrescriptionMap map;
    map.structure_label = "FIELD_NORTH";

    // Zone 1: Low rate area
    PrescriptionZone zone1;
    zone1.boundary = {
        concord::earth::WGS(48.00, 11.00, 0),
        concord::earth::WGS(48.05, 11.00, 0),
        concord::earth::WGS(48.05, 11.05, 0),
        concord::earth::WGS(48.00, 11.05, 0)
    };
    zone1.application_rate = 150; // Low rate
    map.zones.push_back(zone1);

    // Zone 2: High rate area
    PrescriptionZone zone2;
    zone2.boundary = {
        concord::earth::WGS(48.05, 11.00, 0),
        concord::earth::WGS(48.10, 11.00, 0),
        concord::earth::WGS(48.10, 11.05, 0),
        concord::earth::WGS(48.05, 11.05, 0)
    };
    zone2.application_rate = 400; // High rate
    map.zones.push_back(zone2);

    geo.add_prescription_map(std::move(map));
    echo::info("Added prescription map with 2 zones");

    // Subscribe to rate changes
    geo.on_application_rate_changed.subscribe([](i32 rate) {
        echo::info("Application rate changed to: ", rate);
    });

    geo.on_position_update.subscribe([](const GeoPoint& pt) {
        echo::info("Position: ", pt.position.latitude, ", ", pt.position.longitude);
    });

    // Simulate position updates along a path
    dp::Vector<concord::earth::WGS> path = {
        concord::earth::WGS(48.02, 11.02, 400),  // Inside zone 1
        concord::earth::WGS(48.04, 11.03, 400),  // Inside zone 1
        concord::earth::WGS(48.06, 11.02, 400),  // Inside zone 2
        concord::earth::WGS(48.08, 11.03, 400),  // Inside zone 2
        concord::earth::WGS(48.15, 11.02, 400),  // Outside both zones
    };

    for (const auto& pos : path) {
        GeoPoint pt;
        pt.position = pos;
        pt.timestamp_us = 0;
        geo.set_position(pt);

        auto rate = geo.get_rate_at_position(pos);
        if (rate.has_value()) {
            echo::info("  Rate at (", pos.latitude, ", ", pos.longitude, "): ", *rate);
        } else {
            echo::info("  No prescription at (", pos.latitude, ", ", pos.longitude, ")");
        }

        geo.update(100);
    }

    echo::info("=== TC-GEO Demo Complete ===");
    return 0;
}
