#include <agrobus.hpp>
#include <wirebit/shm/shm_link.hpp>
#include <wirebit/can/can_endpoint.hpp>
#include <wirebit/model.hpp>
#include <echo/echo.hpp>
#include <csignal>
#include <atomic>
#include <thread>

using namespace agrobus::net;
using namespace agrobus::j1939;
using namespace agrobus::isobus;
using namespace agrobus::nmea;
using namespace agrobus::isobus::vt;
using namespace agrobus::isobus::tc;
using namespace agrobus::isobus::sc;
using namespace agrobus::isobus::implement;
using namespace agrobus::isobus::fs;

static std::atomic<bool> running{true};
void signal_handler(int) { running = false; }

// Simulates two ISOBUS ECUs communicating over shared memory
// No real CAN hardware needed - everything runs in userspace.
int main() {
    echo::info("=== ISOBUS Shared Memory Simulation ===");

    // --- ECU 1: Tractor ECU (server side) ---
    auto server_result = wirebit::ShmLink::create("isobus_sim", 65536);
    if (!server_result.is_ok()) {
        echo::error("Failed to create ShmLink server");
        return 1;
    }
    auto server_link = std::make_shared<wirebit::ShmLink>(std::move(server_result.value()));
    wirebit::CanEndpoint endpoint1(std::static_pointer_cast<wirebit::Link>(server_link),
                                   wirebit::CanConfig{.bitrate = 250000}, 1);

    IsoNet nm1;
    nm1.set_endpoint(0, &endpoint1);

    Name tractor_name = Name::build()
        .set_identity_number(1)
        .set_manufacturer_code(100)
        .set_function_code(25)
        .set_industry_group(2)
        .set_self_configurable(true);
    auto* tractor_cf = nm1.create_internal(tractor_name, 0, 0x28).value();

    SpeedDistanceInterface speed(nm1, tractor_cf);
    speed.initialize();
    HeartbeatProtocol hb1(nm1, tractor_cf, HeartbeatConfig{}.interval(100));
    hb1.initialize();
    hb1.enable();

    // --- ECU 2: Implement ECU (client side) ---
    auto client_result = wirebit::ShmLink::attach("isobus_sim");
    if (!client_result.is_ok()) {
        echo::error("Failed to attach ShmLink client");
        return 1;
    }
    auto client_link = std::make_shared<wirebit::ShmLink>(std::move(client_result.value()));
    wirebit::CanEndpoint endpoint2(std::static_pointer_cast<wirebit::Link>(client_link),
                                   wirebit::CanConfig{.bitrate = 250000}, 2);

    IsoNet nm2;
    nm2.set_endpoint(0, &endpoint2);

    Name implement_name = Name::build()
        .set_identity_number(2)
        .set_manufacturer_code(200)
        .set_function_code(30)
        .set_industry_group(2)
        .set_self_configurable(true);
    auto* impl_cf = nm2.create_internal(implement_name, 0, 0x30).value();

    HeartbeatProtocol hb2(nm2, impl_cf, HeartbeatConfig{}.interval(100));
    hb2.initialize();
    hb2.enable();

    // Subscribe to speed messages on the implement
    nm2.on_message.subscribe([](const Message& msg) {
        echo::info("ECU2 RX: PGN=0x", msg.pgn, " from=0x", msg.source);
    });

    // --- Start ---
    nm1.start_address_claiming();
    nm2.start_address_claiming();

    signal(SIGINT, signal_handler);
    echo::info("Simulation running with 2 ECUs over shared memory... (Ctrl+C to stop)");
    echo::info("Server stats: frames_sent=", server_link->stats().frames_sent,
               " frames_recv=", server_link->stats().frames_received);

    u32 cycle = 0;
    while (running && cycle < 100) {
        nm1.update(10);
        nm2.update(10);
        hb1.update(10);
        hb2.update(10);

        // Every 500ms, send a speed message from tractor
        if (cycle % 50 == 0) {
            speed.send_wheel_speed(2.5, 0.0);
            echo::info("[cycle ", cycle, "] Tractor sending speed: 2.5 m/s");
        }

        usleep(10000);
        cycle++;
    }

    echo::info("\n=== Simulation Complete ===");
    echo::info("Server: sent=", server_link->stats().frames_sent,
               " recv=", server_link->stats().frames_received);
    echo::info("Client: sent=", client_link->stats().frames_sent,
               " recv=", client_link->stats().frames_received);

    // --- Demonstrate impairment model ---
    echo::info("\n=== Impairment Demo ===");
    wirebit::LinkModel model(1000000, 500000, 0.1, 0.0, 0.0, 250000, 42); // 10% packet loss
    server_link->set_model(model);
    echo::info("Applied 10% packet loss to server link");

    for (i32 i = 0; i < 20; ++i) {
        nm1.update(10);
        nm2.update(10);
        hb1.update(10);
    }

    echo::info("After impairment: dropped=", server_link->stats().frames_dropped);

    return 0;
}
