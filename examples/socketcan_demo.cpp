#include <agrobus/net/network_manager.hpp>
#include <agrobus/j1939/speed_distance.hpp>
#include <agrobus/j1939/heartbeat.hpp>
#include <wirebit/can/socketcan_link.hpp>
#include <wirebit/can/can_endpoint.hpp>
#include <echo/echo.hpp>
#include <csignal>
#include <atomic>

using namespace agrobus::net;
using namespace agrobus::j1939;

static std::atomic<bool> running{true};

void signal_handler(int) { running = false; }

int main(int argc, char* argv[]) {
    echo::info("=== SocketCAN Full Demo ===");

    dp::String interface = "vcan0";
    if (argc > 1) interface = argv[1];

    // Create wirebit SocketCAN link
    auto link_result = wirebit::SocketCanLink::create({
        .interface_name = interface,
        .create_if_missing = true
    });
    if (!link_result.is_ok()) {
        echo::error("Failed to create SocketCAN link on ", interface, ": ", link_result.error().message);
        echo::info("Usage: ", argv[0], " [interface]");
        return 1;
    }
    auto link = std::make_shared<wirebit::SocketCanLink>(std::move(link_result.value()));
    echo::info("Opened CAN interface: ", interface);

    // Create CAN endpoint
    wirebit::CanEndpoint can(link, wirebit::CanConfig{.bitrate = 250000}, 1);

    // Create network
    IsoNet nm(NetworkConfig{}.bus_load(true));
    nm.set_endpoint(0, &can);

    // Create our device
    Name name = Name::build()
        .set_identity_number(1000)
        .set_manufacturer_code(42)
        .set_function_code(25)
        .set_device_class(7)
        .set_industry_group(2)
        .set_self_configurable(true);

    auto cf_result = nm.create_internal(name, 0, 0x28);
    auto* cf = cf_result.value();

    // Set up protocols
    SpeedDistanceInterface speed(nm, cf);
    speed.initialize();
    speed.on_speed_update.subscribe([](const SpeedData& sd) {
        if (sd.wheel_speed_mps) {
            echo::info("Speed: ", *sd.wheel_speed_mps * 3.6, " km/h");
        }
    });

    HeartbeatProtocol hb(nm, cf, HeartbeatConfig{}.interval(100));
    hb.initialize();
    hb.enable();

    // Message handler
    nm.on_message.subscribe([](const Message& msg) {
        echo::trace("RX: PGN=0x", msg.pgn, " src=0x", msg.source, " len=", msg.data.size());
    });

    // Start address claiming
    nm.start_address_claiming();

    // Main loop
    signal(SIGINT, signal_handler);
    echo::info("Running... (Ctrl+C to stop)");

    while (running) {
        nm.update(10);
        hb.update(10);

        usleep(10000); // 10ms
    }

    echo::info("\nBus load: ", nm.bus_load(0), "%");
    echo::info("Done.");

    return 0;
}
