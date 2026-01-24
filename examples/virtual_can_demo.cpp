#include <agrobus.hpp>
#include <wirebit/can/socketcan_link.hpp>
#include <wirebit/can/can_endpoint.hpp>
#include <echo/echo.hpp>

using namespace agrobus::net;
using namespace agrobus::j1939;
using namespace agrobus::isobus;
using namespace agrobus::nmea;
using namespace agrobus::isobus::vt;
using namespace agrobus::isobus::tc;
using namespace agrobus::isobus::sc;
using namespace agrobus::isobus::implement;
using namespace agrobus::isobus::fs;

int main() {
    echo::info("=== Virtual CAN Demo ===");

    // Create a virtual CAN interface and two endpoints on it
    auto link1_result = wirebit::SocketCanLink::create({
        .interface_name = "vcan_demo",
        .create_if_missing = true
    });
    if (!link1_result.is_ok()) {
        echo::error("Failed to create SocketCanLink");
        return 1;
    }
    auto link1 = std::make_shared<wirebit::SocketCanLink>(std::move(link1_result.value()));
    wirebit::CanEndpoint endpoint1(std::static_pointer_cast<wirebit::Link>(link1),
                                   wirebit::CanConfig{.bitrate = 250000}, 1);

    auto link2_result = wirebit::SocketCanLink::attach("vcan_demo");
    if (!link2_result.is_ok()) {
        echo::error("Failed to attach SocketCanLink");
        return 1;
    }
    auto link2 = std::make_shared<wirebit::SocketCanLink>(std::move(link2_result.value()));
    wirebit::CanEndpoint endpoint2(std::static_pointer_cast<wirebit::Link>(link2),
                                   wirebit::CanConfig{.bitrate = 250000}, 2);

    echo::info("Created virtual CAN endpoints on vcan_demo (no hardware needed)");

    // Setup two network managers
    IsoNet nm1;
    nm1.set_endpoint(0, &endpoint1);

    IsoNet nm2;
    nm2.set_endpoint(0, &endpoint2);

    auto* ecu1 = nm1.create_internal(
        Name::build().set_identity_number(1).set_manufacturer_code(100).set_self_configurable(true),
        0, 0x28).value();

    auto* ecu2 = nm2.create_internal(
        Name::build().set_identity_number(2).set_manufacturer_code(200).set_self_configurable(true),
        0, 0x30).value();

    // Subscribe to messages on ECU2
    nm2.on_message.subscribe([](const Message& msg) {
        echo::info("ECU2 received: PGN=0x", msg.pgn, " from=0x", msg.source);
    });

    // Subscribe to messages on ECU1
    nm1.on_message.subscribe([](const Message& msg) {
        echo::info("ECU1 received: PGN=0x", msg.pgn, " from=0x", msg.source);
    });

    // Start address claiming
    nm1.start_address_claiming();
    nm2.start_address_claiming();

    // Run update loops to exchange address claim messages
    for (u32 i = 0; i < 20; ++i) {
        nm1.update(10);
        nm2.update(10);
    }

    // Send a heartbeat from ECU1
    HeartbeatProtocol hb1(nm1, ecu1, HeartbeatConfig{}.interval(100));
    hb1.initialize();
    hb1.enable();

    for (u32 i = 0; i < 10; ++i) {
        hb1.update(10);
        nm1.update(10);
        nm2.update(10);
    }

    // Show stats
    echo::info("ECU1 link stats: sent=", link1->stats().frames_sent,
               " recv=", link1->stats().frames_received);
    echo::info("ECU2 link stats: sent=", link2->stats().frames_sent,
               " recv=", link2->stats().frames_received);

    echo::info("=== Virtual CAN Demo Complete ===");
    return 0;
}
