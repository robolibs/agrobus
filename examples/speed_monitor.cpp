#include <agrobus/net/network_manager.hpp>
#include <agrobus/j1939/speed_distance.hpp>
#include <echo/echo.hpp>

using namespace agrobus::net;
using namespace agrobus::j1939;

int main() {
    echo::info("=== Speed Monitor Demo ===");

    IsoNet nm;
    Name name = Name::build()
        .set_identity_number(1)
        .set_manufacturer_code(42)
        .set_function_code(25)
        .set_industry_group(2);

    auto cf_result = nm.create_internal(name, 0, 0x28);
    auto* cf = cf_result.value();

    SpeedDistanceInterface speed(nm, cf);
    speed.initialize();

    // Subscribe to speed updates
    speed.on_speed_update.subscribe([](const SpeedData& sd) {
        if (sd.wheel_speed_mps) {
            echo::info("Wheel speed: ", *sd.wheel_speed_mps, " m/s (",
                       *sd.wheel_speed_mps * 3.6, " km/h)");
        }
        if (sd.ground_speed_mps) {
            echo::info("Ground speed: ", *sd.ground_speed_mps, " m/s");
        }
        if (sd.distance_m) {
            echo::info("Distance: ", *sd.distance_m, " m");
        }
    });

    echo::info("Speed monitor initialized. Waiting for data...");
    echo::info("(In real usage, connect a CAN driver and call nm.update() in a loop)");

    // Simulate receiving a speed message
    Message msg;
    msg.pgn = PGN_WHEEL_SPEED;
    msg.source = 0x30;
    // 5.0 m/s = 5000 raw units (0.001 m/s per bit)
    msg.data = {0x88, 0x13, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF};

    echo::info("\nSimulated wheel speed message (5.0 m/s):");
    // In a real system, the network manager dispatches this automatically

    return 0;
}
