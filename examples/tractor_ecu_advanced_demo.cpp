#include <agrobus.hpp>
#include <echo/echo.hpp>

using namespace agrobus::net;
using namespace agrobus::j1939;
using namespace agrobus::isobus;
using namespace agrobus::isobus::implement;

int main() {
    echo::info("=== Advanced Tractor ECU Demo ===");
    echo::info("Demonstrating ISO 11783-9 TECU with power management and safe-mode");

    // Create network manager
    IsoNet net;

    // Create control function for TECU
    Name tecu_name = Name::build()
                         .set_identity_number(100)
                         .set_manufacturer_code(1234)
                         .set_function_code(25) // Tractor ECU
                         .set_industry_group(2) // Agricultural
                         .set_self_configurable(true);

    auto cf_result = net.create_internal(tecu_name, 0, 0x80);
    if (!cf_result.is_ok()) {
        echo::error("Failed to create internal CF");
        return 1;
    }
    auto *cf = cf_result.value();

    // Configure TECU as Class 2 with Navigation and Front-mounted addenda
    TECUClassification classification;
    classification.base_class = TECUClass::Class2;
    classification.navigation = true;
    classification.front_mounted = true;
    classification.version = 1;
    classification.instance = 0; // Primary TECU

    TECUConfig tecu_config;
    tecu_config.set_classification(classification);
    tecu_config.power.shutdown_max_time_ms = 180000; // 3 minutes
    tecu_config.power.maintain_timeout_ms = 2000;    // 2 seconds
    tecu_config.broadcast_interval(2000);            // Facilities every 2s
    tecu_config.status_interval(100);                // Status every 100ms

    // Create Tractor ECU
    TractorECU tecu(net, cf, tecu_config);
    auto init_result = tecu.initialize();
    if (!init_result.is_ok()) {
        echo::error("Failed to initialize TECU");
        return 1;
    }

    echo::info("TECU initialized: ", classification.to_string());
    echo::info("  Version: ", static_cast<u32>(classification.version));
    echo::info("  Instance: ", static_cast<u32>(classification.instance));

    // Create and attach TIM server
    TimServer tim(net, cf);
    tim.initialize();
    tim.set_front_pto(false, true, 0);
    tim.set_rear_pto(false, true, 0);
    tim.set_front_hitch(false, 0);
    tim.set_rear_hitch(false, 0);

    // Configure 8 aux valves
    for (u8 i = 0; i < 8; ++i) {
        tim.set_aux_valve_capabilities(i, true, true);
    }

    tecu.attach_tim_server(std::move(tim));
    echo::info("TIM server attached");

    // Subscribe to power state changes
    tecu.on_power_state_changed.subscribe([](isobus::PowerState state) {
        echo::info("Power state changed: ", static_cast<u32>(state));
    });

    // Subscribe to safe mode events
    tecu.on_safe_mode_triggered.subscribe([](SafeModeTrigger trigger) {
        echo::warn("SAFE MODE TRIGGERED: ", static_cast<u32>(trigger));
    });

    tecu.on_shutdown_complete.subscribe([]() {
        echo::info("Shutdown complete");
    });

    // Subscribe to facilities requests
    tecu.on_facilities_request_received.subscribe([](const TractorFacilities &req) {
        echo::info("Facilities request received from implement");
    });

    // ═══════════════════════════════════════════════════════════════════════
    // SCENARIO 1: Normal startup and operation
    // ═══════════════════════════════════════════════════════════════════════

    echo::info("\n--- SCENARIO 1: Normal Startup ---");
    tecu.set_key_switch(true);
    echo::info("Key switch ON");
    echo::info("  ECU_PWR: ", tecu.get_ecu_pwr_enabled() ? "ON" : "OFF");
    echo::info("  PWR: ", tecu.get_pwr_enabled() ? "ON" : "OFF");

    // Simulate normal operation for 5 seconds
    echo::info("Operating for 5 seconds...");
    for (u32 i = 0; i < 50; ++i) {
        net.update(100);
        tecu.update(100);

        // Engage rear PTO after 1 second
        if (i == 10) {
            auto *tim_server = tecu.get_tim_server();
            if (tim_server) {
                tim_server->set_rear_pto(true, true, 540);
                tim_server->set_rear_hitch(true, 5000);
                echo::info("Rear PTO engaged at 540 RPM, hitch at 50%");
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // SCENARIO 2: Shutdown with maintain power requests
    // ═══════════════════════════════════════════════════════════════════════

    echo::info("\n--- SCENARIO 2: Shutdown with Maintain Power ---");
    tecu.set_key_switch(false);
    echo::info("Key switch OFF - shutdown initiated");

    // Simulate CF requesting power maintenance (e.g., TC saving data)
    Address requesting_cf = 0x42;
    u32 current_time = 0;

    echo::info("CF 0x42 requests ECU_PWR + PWR for data save...");
    tecu.receive_maintain_power_request(requesting_cf, true, true, current_time);

    // Update for 3 seconds with power maintained
    for (u32 i = 0; i < 30; ++i) {
        current_time += 100;
        net.update(100);
        tecu.update(100);

        // CF continues requesting power every 1.5 seconds
        if (i % 15 == 0) {
            tecu.receive_maintain_power_request(requesting_cf, true, true, current_time);
        }

        if (i == 15) {
            echo::info("  ECU_PWR: ", tecu.get_ecu_pwr_enabled() ? "ON" : "OFF");
            echo::info("  PWR: ", tecu.get_pwr_enabled() ? "ON" : "OFF");
        }
    }

    // CF finishes data save, only needs ECU_PWR now
    echo::info("CF 0x42 finished with PWR, only needs ECU_PWR...");
    tecu.receive_maintain_power_request(requesting_cf, true, false, current_time);

    for (u32 i = 0; i < 10; ++i) {
        current_time += 100;
        net.update(100);
        tecu.update(100);
    }

    echo::info("  ECU_PWR: ", tecu.get_ecu_pwr_enabled() ? "ON" : "OFF");
    echo::info("  PWR: ", tecu.get_pwr_enabled() ? "ON" : "OFF (expected)");

    // CF finishes completely, stop requesting power
    echo::info("CF 0x42 finished, stopping power requests...");

    // Wait for maintain timeout (2s) before final shutdown
    for (u32 i = 0; i < 25; ++i) {
        current_time += 100;
        net.update(100);
        tecu.update(100);
    }

    echo::info("Final shutdown should have occurred");
    echo::info("  Power state: ", static_cast<u32>(tecu.get_power_state()));
    echo::info("  ECU_PWR: ", tecu.get_ecu_pwr_enabled() ? "ON" : "OFF");
    echo::info("  PWR: ", tecu.get_pwr_enabled() ? "ON" : "OFF (expected)");

    // ═══════════════════════════════════════════════════════════════════════
    // SCENARIO 3: Safe mode trigger
    // ═══════════════════════════════════════════════════════════════════════

    echo::info("\n--- SCENARIO 3: Safe Mode ---");

    // Restart for safe mode demo
    tecu.set_key_switch(true);
    echo::info("Key switch ON for safe mode demo");

    for (u32 i = 0; i < 10; ++i) {
        net.update(100);
        tecu.update(100);
    }

    // Engage PTO and hitch
    auto *tim_server = tecu.get_tim_server();
    if (tim_server) {
        tim_server->set_rear_pto(true, true, 1000);
        tim_server->set_rear_hitch(true, 8000);
        tim_server->set_aux_valve(0, true, 5000);
        tim_server->set_aux_valve(1, true, 7000);
        echo::info("Equipment engaged: PTO=1000rpm, Hitch=80%, Valves 0/1=ON");
    }

    for (u32 i = 0; i < 5; ++i) {
        net.update(100);
        tecu.update(100);
    }

    // Trigger safe mode (simulating CAN bus failure)
    echo::warn("Simulating CAN bus failure...");
    tecu.trigger_safe_mode(SafeModeTrigger::CANBusFail);

    // Verify failsafe actions
    if (tim_server) {
        auto rear_pto = tim_server->get_rear_pto();
        auto rear_hitch = tim_server->get_rear_hitch();
        auto valve0 = tim_server->get_aux_valve(0);

        echo::info("Failsafe state:");
        echo::info("  Rear PTO engaged: ", rear_pto.engaged ? "YES (ERROR!)" : "NO (correct)");
        echo::info("  Rear hitch position: ", rear_hitch.position);
        echo::info("  Valve 0 state: ", valve0.state ? "ON (ERROR!)" : "OFF (correct)");
    }

    // Clear safe mode
    tecu.clear_safe_mode();
    echo::info("Safe mode cleared");

    echo::info("\n=== Advanced Tractor ECU Demo Complete ===");
    echo::info("Demonstrated:");
    echo::info("  ✓ TECU classification (Class 2NF)");
    echo::info("  ✓ Power management state machine");
    echo::info("  ✓ Maintain power requests with timeout");
    echo::info("  ✓ Safe mode triggers and failsafe actions");
    echo::info("  ✓ TIM server integration");

    return 0;
}
