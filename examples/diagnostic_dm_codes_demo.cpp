/**
 * @file diagnostic_dm_codes_demo.cpp
 * @brief Comprehensive demonstration of J1939-73 diagnostic messages (DM4-DM23)
 *
 * This demo showcases all newly implemented DM codes:
 * - DM4: Driver's Information (active DTCs visible to driver)
 * - DM6: Pending DTCs (faults detected but not yet confirmed)
 * - DM7/DM8: Test Command and Results (diagnostic test execution)
 * - DM12: Emissions-related DTCs (OBD compliance)
 * - DM21: Diagnostic Readiness 2 (OBD monitor status)
 * - DM23: Previously MIL-OFF DTCs (historical fault tracking)
 *
 * ISO 11783-12 / J1939-73 compliance demonstration
 */

#include <agrobus.hpp>
#include <echo/echo.hpp>
#include <thread>
#include <chrono>

using namespace agrobus::j1939;
using namespace agrobus::net;
using namespace std::chrono_literals;

// ═════════════════════════════════════════════════════════════════════════════
// Demo Scenario: Agricultural Tractor with Engine Issues
// ═════════════════════════════════════════════════════════════════════════════

class TractorDiagnosticDemo {
private:
    IsoNet &net_;
    DiagnosticProtocol diag_;
    u32 simulation_time_ms_ = 0;

public:
    TractorDiagnosticDemo(IsoNet &net, InternalCF *cf) : net_(net), diag_(net, cf) {
        echo::info("═══════════════════════════════════════════════════════════════");
        echo::info("  Tractor Diagnostic Messages Demo (ISO 11783-12)");
        echo::info("═══════════════════════════════════════════════════════════════");

        // Setup event handlers
        diag_.on_dm4_received.subscribe([this](const DM4Message &msg, Address src) {
            echo::info("\n[DM4 Received] Driver's Information from address 0x", std::hex, (int)src);
            display_dm4(msg);
        });

        diag_.on_dm6_received.subscribe([this](const DM6Message &msg, Address src) {
            echo::info("\n[DM6 Received] Pending DTCs from address 0x", std::hex, (int)src);
            display_dm6(msg);
        });

        diag_.on_dm12_received.subscribe([this](const DM12Message &msg, Address src) {
            echo::info("\n[DM12 Received] Emissions DTCs from address 0x", std::hex, (int)src);
            display_dm12(msg);
        });

        diag_.on_dm21_received.subscribe([this](const DM21Readiness &msg, Address src) {
            echo::info("\n[DM21 Received] Diagnostic Readiness from address 0x", std::hex, (int)src);
            display_dm21(msg);
        });

        diag_.on_dm23_received.subscribe([this](const DM23Message &msg, Address src) {
            echo::info("\n[DM23 Received] Previously MIL-OFF DTCs from address 0x", std::hex, (int)src);
            display_dm23(msg);
        });

        diag_.initialize();
    }

    void run_demo() {
        echo::info("\n\n📋 SCENARIO: Tractor develops engine coolant temperature issue");
        echo::info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

        // Phase 1: Normal operation - no faults
        echo::info("\n⏱️  T+0s: Normal Operation");
        echo::info("   Engine running normally, all systems OK\n");
        demonstrate_dm21_readiness();
        std::this_thread::sleep_for(1s);

        // Phase 2: Pending fault detected
        echo::info("\n⏱️  T+5s: Coolant Temperature Rising (Pending)");
        echo::info("   Engine coolant temp sensor reading 105°C");
        echo::info("   Threshold: 100°C - Fault pending confirmation\n");

        DTC pending_dtc{.spn = 110, .fmi = FMI::AboveNormal, .occurrence_count = 1};
        demonstrate_dm6_pending(pending_dtc);
        std::this_thread::sleep_for(1s);

        // Phase 3: Fault confirmed - becomes active
        echo::info("\n⏱️  T+10s: Fault Confirmed (Active)");
        echo::info("   Coolant temp sustained at 110°C");
        echo::info("   DTC confirmed - Activating MIL lamp\n");

        diag_.set_active(pending_dtc);
        demonstrate_dm4_active();
        std::this_thread::sleep_for(1s);

        // Phase 4: Emissions impact detected
        echo::info("\n⏱️  T+15s: Emissions Impact Detected");
        echo::info("   High coolant temp affecting emissions");
        echo::info("   Catalyst efficiency degraded\n");

        DTC emissions_dtc{.spn = 3058, .fmi = FMI::AboveNormal, .occurrence_count = 1};
        demonstrate_dm12_emissions(emissions_dtc);
        std::this_thread::sleep_for(1s);

        // Phase 5: Diagnostic test requested
        echo::info("\n⏱️  T+20s: Diagnostic Test Requested");
        echo::info("   Technician requests coolant sensor test\n");

        demonstrate_dm7_dm8_test();
        std::this_thread::sleep_for(1s);

        // Phase 6: Fault cleared
        echo::info("\n⏱️  T+25s: Fault Repaired");
        echo::info("   Coolant system serviced");
        echo::info("   Temperature back to normal (85°C)\n");

        diag_.clear_active(110, FMI::AboveNormal);
        demonstrate_dm23_historical();
        std::this_thread::sleep_for(1s);

        // Phase 7: Final readiness check
        echo::info("\n⏱️  T+30s: Post-Repair Readiness Check\n");
        demonstrate_dm21_readiness();

        echo::info("\n\n✅ Demo Complete!");
        echo::info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    }

private:
    void demonstrate_dm4_active() {
        echo::info("🔴 DM4: Driver's Information Message");
        echo::info("   Purpose: Show active faults to driver\n");

        DM4Message dm4;
        dm4.mil_status = LampStatus::On;
        dm4.red_stop_lamp = LampStatus::Off;
        dm4.amber_warning = LampStatus::On;
        dm4.protect_lamp = LampStatus::Off;
        dm4.dtcs = diag_.active_dtcs();

        auto result = diag_.send_dm4();
        if (result.is_ok()) {
            display_dm4(dm4);
            echo::info("   ✓ DM4 broadcast to all displays\n");
        }
    }

    void demonstrate_dm6_pending(const DTC &dtc) {
        echo::info("🟡 DM6: Pending DTCs Message");
        echo::info("   Purpose: Report faults under evaluation\n");

        DM6Message dm6;
        dm6.lamps.malfunction = LampStatus::Off;
        dm6.lamps.amber_warning = LampStatus::On;
        dm6.pending_dtcs.push_back(dtc);

        auto result = diag_.send_dm6();
        if (result.is_ok()) {
            display_dm6(dm6);
            echo::info("   ✓ DM6 broadcast - Pending fault logged\n");
        }
    }

    void demonstrate_dm7_dm8_test() {
        echo::info("🔧 DM7/DM8: Diagnostic Test Execution");
        echo::info("   Purpose: Execute component self-tests\n");

        // DM7: Send test command
        echo::info("   DM7 → Test Command:");
        echo::info("      Test ID: 247 (Coolant Sensor Self-Test)");
        echo::info("      SPN: 110 (Engine Coolant Temperature)");
        echo::info("      Test Limits: 80-100°C\n");

        auto dm7_result = diag_.send_dm7(110, 247, BROADCAST_ADDRESS);

        // DM8: Simulated test result
        std::this_thread::sleep_for(500ms);

        DM8TestResult dm8;
        dm8.test_id = 247;
        dm8.spn = 110;
        dm8.test_result = 0; // Pass
        dm8.test_value = 95;
        dm8.test_limit_min = 80;
        dm8.test_limit_max = 100;

        echo::info("   DM8 ← Test Result:");
        echo::info("      Test ID: 247");
        echo::info("      Result: PASS ✓");
        echo::info("      Measured Value: 95°C");
        echo::info("      Limits: 80-100°C");
        echo::info("      Sensor functioning correctly\n");

        diag_.send_dm8(dm8);
    }

    void demonstrate_dm12_emissions(const DTC &dtc) {
        echo::info("🌍 DM12: Emissions-Related DTCs");
        echo::info("   Purpose: OBD/EPA compliance reporting\n");

        DM12Message dm12;
        dm12.lamps.malfunction = LampStatus::On;
        dm12.emissions_dtcs.push_back(dtc);

        auto result = diag_.send_dm12();
        if (result.is_ok()) {
            display_dm12(dm12);
            echo::info("   ✓ DM12 broadcast - Emissions fault logged\n");
        }
    }

    void demonstrate_dm21_readiness() {
        echo::info("📊 DM21: Diagnostic Readiness 2");
        echo::info("   Purpose: OBD monitor status tracking\n");

        DM21Readiness dm21;
        dm21.distance_with_mil_on_km = 0;
        dm21.distance_since_codes_cleared_km = 450;
        dm21.minutes_with_mil_on = 0;
        dm21.comprehensive_component = 1; // Complete
        dm21.fuel_system = 1;
        dm21.misfire = 1;

        auto result = diag_.send_dm21();
        if (result.is_ok()) {
            display_dm21(dm21);
            echo::info("   ✓ DM21 broadcast - Readiness status OK\n");
        }
    }

    void demonstrate_dm23_historical() {
        echo::info("📜 DM23: Previously MIL-OFF DTCs");
        echo::info("   Purpose: Track historical faults\n");

        DM23Message dm23;
        dm23.lamps.malfunction = LampStatus::Off;

        auto prev_dtcs = diag_.previously_active_dtcs();
        for (const auto &prev : prev_dtcs) {
            dm23.previously_mil_off_dtcs.push_back({
                .spn = prev.dtc.spn,
                .fmi = prev.dtc.fmi,
                .occurrence_count = prev.occurrence_count
            });
        }

        auto result = diag_.send_dm23();
        if (result.is_ok()) {
            display_dm23(dm23);
            echo::info("   ✓ DM23 broadcast - Historical fault logged\n");
        }
    }

    // Display helpers
    void display_dm4(const DM4Message &msg) {
        echo::info("   Lamp Status:");
        echo::info("      MIL: ", lamp_status_str(msg.mil_status));
        echo::info("      Red Stop: ", lamp_status_str(msg.red_stop_lamp));
        echo::info("      Amber Warning: ", lamp_status_str(msg.amber_warning));
        echo::info("   Active DTCs: ", msg.dtcs.size());
        for (const auto &dtc : msg.dtcs) {
            echo::info("      • SPN ", dtc.spn, " FMI ", (int)dtc.fmi,
                       " (", spn_description(dtc.spn), ")");
        }
    }

    void display_dm6(const DM6Message &msg) {
        echo::info("   Lamps: MIL=", lamp_status_str(msg.lamps.malfunction),
                   ", Amber=", lamp_status_str(msg.lamps.amber_warning));
        echo::info("   Pending DTCs: ", msg.pending_dtcs.size());
        for (const auto &dtc : msg.pending_dtcs) {
            echo::info("      • SPN ", dtc.spn, " FMI ", (int)dtc.fmi,
                       " (", spn_description(dtc.spn), ")");
        }
    }

    void display_dm12(const DM12Message &msg) {
        echo::info("   Emissions Impact: Yes");
        echo::info("   MIL Status: ", lamp_status_str(msg.lamps.malfunction));
        echo::info("   Emissions DTCs: ", msg.emissions_dtcs.size());
        for (const auto &dtc : msg.emissions_dtcs) {
            echo::info("      • SPN ", dtc.spn, " FMI ", (int)dtc.fmi,
                       " (", spn_description(dtc.spn), ")");
        }
    }

    void display_dm21(const DM21Readiness &msg) {
        echo::info("   Distance with MIL ON: ", msg.distance_with_mil_on_km, " km");
        echo::info("   Distance since clear: ", msg.distance_since_codes_cleared_km, " km");
        echo::info("   Monitor Status:");
        echo::info("      Comprehensive: ", monitor_status_str(msg.comprehensive_component));
        echo::info("      Fuel System: ", monitor_status_str(msg.fuel_system));
        echo::info("      Misfire: ", monitor_status_str(msg.misfire));
    }

    void display_dm23(const DM23Message &msg) {
        echo::info("   Current MIL: ", lamp_status_str(msg.lamps.malfunction));
        echo::info("   Historical DTCs: ", msg.previously_mil_off_dtcs.size());
        for (const auto &dtc : msg.previously_mil_off_dtcs) {
            echo::info("      • SPN ", dtc.spn, " FMI ", (int)dtc.fmi,
                       " Count: ", dtc.occurrence_count,
                       " (", spn_description(dtc.spn), ")");
        }
    }

    // Helper functions
    static const char* lamp_status_str(LampStatus status) {
        switch (status) {
            case LampStatus::Off: return "OFF";
            case LampStatus::On: return "ON";
            case LampStatus::Error: return "ERROR";
            case LampStatus::NotAvailable: return "N/A";
            default: return "UNKNOWN";
        }
    }

    static const char* monitor_status_str(u8 status) {
        switch (status) {
            case 0: return "Not Complete";
            case 1: return "Complete ✓";
            case 0xFF: return "Not Supported";
            default: return "Unknown";
        }
    }

    static const char* spn_description(u32 spn) {
        switch (spn) {
            case 110: return "Engine Coolant Temperature";
            case 3058: return "NOx Sensor 2";
            case 3216: return "NOx Intake";
            case 3226: return "NOx Outlet";
            default: return "Unknown Parameter";
        }
    }
};

// ═════════════════════════════════════════════════════════════════════════════
// Main Demo Entry Point
// ═════════════════════════════════════════════════════════════════════════════

int main() {
    echo::info("\n");
    echo::info("╔════════════════════════════════════════════════════════════════╗");
    echo::info("║  J1939-73 / ISO 11783-12 Diagnostic Messages Demo            ║");
    echo::info("║  Demonstrating: DM4, DM6, DM7/8, DM12, DM21, DM23            ║");
    echo::info("╚════════════════════════════════════════════════════════════════╝");
    echo::info("\n");

    // Setup virtual CAN network
    IsoNet net;

    // Create tractor ECU
    Name tractor_name = Name::build()
        .set_identity_number(1234)
        .set_manufacturer_code(123)
        .set_function_code(0) // Engine
        .set_industry_group(2); // Agricultural

    auto cf_result = net.create_internal(tractor_name, 0, 0x00);
    if (cf_result.is_err()) {
        echo::error("Failed to create control function");
        return 1;
    }

    // Run demonstration
    TractorDiagnosticDemo demo(net, cf_result.value());
    demo.run_demo();

    echo::info("\n📚 Summary of Demonstrated Features:");
    echo::info("   ✓ DM4  - Driver's Information (active fault display)");
    echo::info("   ✓ DM6  - Pending DTCs (early fault detection)");
    echo::info("   ✓ DM7  - Test Command (diagnostic test request)");
    echo::info("   ✓ DM8  - Test Results (diagnostic test response)");
    echo::info("   ✓ DM12 - Emissions DTCs (OBD compliance)");
    echo::info("   ✓ DM21 - Diagnostic Readiness (monitor status)");
    echo::info("   ✓ DM23 - Historical DTCs (fault history tracking)");
    echo::info("\n");

    return 0;
}
