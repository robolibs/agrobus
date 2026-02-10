#include <agrobus/net/network_manager.hpp>
#include <agrobus/nmea/interface.hpp>
#include <agrobus/nmea/position.hpp>
#include <echo/echo.hpp>
#include <cmath>

using namespace agrobus::net;
using namespace agrobus::nmea;

int main() {
    echo::info("=== GNSS Monitor Demo ===");

    IsoNet nm;
    Name name = Name::build()
        .set_identity_number(1)
        .set_manufacturer_code(42);

    auto cf_result = nm.create_internal(name, 0, 0x28);
    auto* cf = cf_result.value();

    NMEAInterface nmea(nm, cf);
    nmea.initialize();

    // Field reference point
    dp::Geo field_origin{48.1234, 11.5678, 450.0};
    echo::info("Field origin: ", field_origin.latitude, ", ", field_origin.longitude);

    nmea.on_position.subscribe([&](const GNSSPosition& pos) {
        echo::info("Position: ", pos.wgs.latitude, ", ", pos.wgs.longitude,
                   " alt=", pos.wgs.altitude);
        echo::info("  Fix: ", static_cast<int>(pos.fix_type),
                   " Sats: ", pos.satellites_used);

        // Convert to local ENU frame
        auto enu = pos.to_enu(field_origin);
        echo::info("  ENU: E=", enu.east(), " N=", enu.north(), " U=", enu.up());

        // Convert to ECEF
        auto ecf = pos.to_ecf();
        f64 dist = std::sqrt(ecf.x * ecf.x + ecf.y * ecf.y + ecf.z * ecf.z);
        echo::info("  ECEF distance from center: ", dist / 1000.0, " km");
    });

    nmea.on_cog.subscribe([](f64 cog) {
        echo::info("COG: ", cog * 180.0 / M_PI, " degrees");
    });

    nmea.on_sog.subscribe([](f64 sog) {
        echo::info("SOG: ", sog, " m/s (", sog * 3.6, " km/h)");
    });

    echo::info("GNSS monitor initialized. Waiting for data...");

    // Demonstrate batch conversion
    GNSSBatch batch;
    for (i32 i = 0; i < 10; ++i) {
        GNSSPosition pos;
        pos.wgs = concord::earth::WGS(
            48.1234 + i * 0.00001,
            11.5678 + i * 0.00002,
            450.0
        );
        pos.fix_type = GNSSFixType::RTKFixed;
        batch.positions.push_back(pos);
    }

    auto enu_batch = batch.to_enu_batch(field_origin);
    echo::info("\nBatch conversion (10 points):");
    for (usize i = 0; i < enu_batch.size(); ++i) {
        echo::info("  [", i, "] E=", enu_batch[i].east(),
                   " N=", enu_batch[i].north());
    }

    return 0;
}
