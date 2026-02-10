#include <agrobus/net/network_manager.hpp>
#include <agrobus/j1939/heartbeat.hpp>
#include <echo/echo.hpp>

using namespace agrobus::net;
using namespace agrobus::j1939;

int main() {
    echo::info("=== Heartbeat Demo ===");

    IsoNet nm;
    Name name = Name::build()
        .set_identity_number(1)
        .set_manufacturer_code(42);

    auto cf_result = nm.create_internal(name, 0, 0x28);
    auto* cf = cf_result.value();

    HeartbeatProtocol hb(nm, cf, HeartbeatConfig{}.interval(100)); // 100ms interval
    hb.initialize();
    hb.enable();

    // Track another device
    hb.track(0x30);

    hb.on_heartbeat_received.subscribe([](Address src, u8 seq) {
        echo::info("Heartbeat from 0x", src, " seq=", seq);
    });

    hb.on_heartbeat_missed.subscribe([](Address src, u32 count) {
        echo::warn("Missed heartbeat from 0x", src, " (", count, " missed)");
    });

    echo::info("Heartbeat protocol enabled at ", hb.interval(), "ms interval");
    echo::info("Tracking device at address 0x30");

    // Simulate time passing without receiving heartbeat from tracked device
    for (i32 i = 0; i < 5; ++i) {
        hb.update(100);
    }

    return 0;
}
