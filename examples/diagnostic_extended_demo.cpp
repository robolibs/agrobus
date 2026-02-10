#include <agrobus.hpp>
#include <echo/echo.hpp>

using namespace agrobus::net;
using namespace agrobus::j1939;

int main() {
    echo::info("=== Diagnostic Extended Features Demo (ISO 11783-12 Phase 4) ===");
    echo::info("Demonstrating DM25 Freeze Frames, DM20 Performance Ratios, and complete diagnostic lifecycle");

    // Create network manager
    IsoNet net;

    // ═════════════════════════════════════════════════════════════════════════
    // Create ECU with diagnostic protocol
    // ═════════════════════════════════════════════════════════════════════════

    Name ecu_name = Name::build()
                        .set_identity_number(100)
                        .set_manufacturer_code(1234)
                        .set_function_code(0) // Engine
                        .set_industry_group(2)
                        .set_self_configurable(true);

    auto ecu_cf = net.create_internal(ecu_name, 0, 0x20).value();

    // Configure diagnostics with freeze frames
    DiagnosticConfig diag_config;
    diag_config.interval(1000)
        .enable_auto_send(true)
        .freeze_frame_depth(5)
        .auto_capture_freeze_frames_enabled(true);

    DiagnosticProtocol diag(net, ecu_cf, diag_config);
    diag.initialize();

    echo::info("ECU initialized at address 0x20");

    // ═════════════════════════════════════════════════════════════════════════
    // SCENARIO 1: Freeze Frame Automatic Capture
    // ═════════════════════════════════════════════════════════════════════════

    echo::info("\n--- SCENARIO 1: Freeze Frame Automatic Capture ---");

    // Set up product and software identification
    ProductIdentification product;
    product.make = "Demo Manufacturer";
    product.model = "Engine ECU v2";
    product.serial_number = "SN123456";
    diag.set_product_id(product);

    SoftwareIdentification software;
    software.versions.push_back("Firmware:1.2.3");
    software.versions.push_back("Application:2.0.1");
    software.versions.push_back("Calibration:CAL-456");
    diag.set_software_id(software);

    echo::info("Product ID: ", product.make, " ", product.model);
    echo::info("Software: Firmware 1.2.3, App 2.0.1");

    // Activate a DTC - freeze frame will be captured automatically
    DTC coolant_dtc;
    coolant_dtc.spn = 110; // Engine coolant temperature
    coolant_dtc.fmi = FMI::AboveNormal;

    echo::info("\nActivating DTC: SPN=110 (Coolant Temp), FMI=AboveNormal");
    diag.set_active(coolant_dtc);

    // Check that freeze frame was captured
    auto ff = diag.get_freeze_frame(110, FMI::AboveNormal, 0);
    if (ff.has_value()) {
        echo::info("✓ Freeze frame captured automatically");
        echo::info("  Timestamp: ", ff.value().timestamp_ms, " ms");
        echo::info("  Snapshots: ", ff.value().snapshots.size());
    } else {
        echo::error("✗ Freeze frame not captured");
    }

    // ═════════════════════════════════════════════════════════════════════════
    // SCENARIO 2: Manual Freeze Frame Capture with SPN Snapshots
    // ═════════════════════════════════════════════════════════════════════════

    echo::info("\n--- SCENARIO 2: Manual Freeze Frame Capture ---");

    // Activate another DTC with manual freeze frame capture
    DTC fuel_dtc;
    fuel_dtc.spn = 94; // Fuel delivery pressure
    fuel_dtc.fmi = FMI::BelowNormal;

    echo::info("Activating DTC: SPN=94 (Fuel Pressure), FMI=BelowNormal");

    // Manually capture freeze frame with specific SPN snapshots
    dp::Vector<SPNSnapshot> snapshots;
    snapshots.push_back({.spn = 91, .value = 2500});   // Accelerator pedal position (25.0%)
    snapshots.push_back({.spn = 94, .value = 15000});  // Fuel pressure (150 bar)
    snapshots.push_back({.spn = 110, .value = 95});    // Coolant temp (95°C)
    snapshots.push_back({.spn = 190, .value = 1800});  // Engine speed (1800 RPM)
    snapshots.push_back({.spn = 512, .value = 45});    // Driver's demand torque (45%)

    diag.capture_freeze_frame(fuel_dtc, snapshots, 12345);

    echo::info("✓ Manual freeze frame captured with ", snapshots.size(), " SPN snapshots:");
    echo::info("  SPN 91 (Accel Pedal): 2500 (25.0%)");
    echo::info("  SPN 94 (Fuel Press): 15000 (150 bar)");
    echo::info("  SPN 110 (Coolant): 95°C");
    echo::info("  SPN 190 (Engine RPM): 1800");
    echo::info("  SPN 512 (Torque Demand): 45%");

    // ═════════════════════════════════════════════════════════════════════════
    // SCENARIO 3: Multiple Freeze Frames per DTC (Depth Testing)
    // ═════════════════════════════════════════════════════════════════════════

    echo::info("\n--- SCENARIO 3: Multiple Freeze Frames (Depth Testing) ---");

    DTC egr_dtc;
    egr_dtc.spn = 412; // EGR temperature
    egr_dtc.fmi = FMI::AboveNormal;

    // Capture multiple freeze frames for the same DTC
    for (u32 i = 0; i < 7; ++i) {
        dp::Vector<SPNSnapshot> frame_data;
        frame_data.push_back({.spn = 412, .value = 650 + i * 10}); // Temperature increasing
        frame_data.push_back({.spn = 190, .value = 1500 + i * 100}); // RPM increasing

        diag.capture_freeze_frame(egr_dtc, frame_data, i * 1000);
        echo::info("Captured frame ", i + 1, ": EGR temp=", 650 + i * 10, "°C");
    }

    // Check depth limiting (max 5 frames configured)
    auto ff0 = diag.get_freeze_frame(412, FMI::AboveNormal, 0); // Most recent
    auto ff1 = diag.get_freeze_frame(412, FMI::AboveNormal, 1);
    auto ff2 = diag.get_freeze_frame(412, FMI::AboveNormal, 2);
    auto ff3 = diag.get_freeze_frame(412, FMI::AboveNormal, 3);
    auto ff4 = diag.get_freeze_frame(412, FMI::AboveNormal, 4);
    auto ff5 = diag.get_freeze_frame(412, FMI::AboveNormal, 5); // Should be null (beyond depth)

    echo::info("\nFreeze frame availability:");
    echo::info("  Frame 0 (most recent): ", ff0.has_value() ? "available" : "not available");
    echo::info("  Frame 1: ", ff1.has_value() ? "available" : "not available");
    echo::info("  Frame 2: ", ff2.has_value() ? "available" : "not available");
    echo::info("  Frame 3: ", ff3.has_value() ? "available" : "not available");
    echo::info("  Frame 4: ", ff4.has_value() ? "available" : "not available");
    echo::info("  Frame 5 (beyond depth): ", ff5.has_value() ? "available" : "not available");

    if (ff0.has_value() && !ff5.has_value()) {
        echo::info("✓ Depth limiting works correctly (max 5 frames)");
    }

    // ═════════════════════════════════════════════════════════════════════════
    // SCENARIO 4: Monitor Performance Ratios (DM20)
    // ═════════════════════════════════════════════════════════════════════════

    echo::info("\n--- SCENARIO 4: Monitor Performance Ratios (DM20) ---");

    // Initialize OBD counters
    diag.set_ignition_cycles(25);
    diag.set_obd_conditions_met(18);

    echo::info("Ignition cycles since clear: 25");
    echo::info("OBD monitoring conditions met: 18");

    // Track catalyst monitor (SPN 3050)
    echo::info("\nCatalyst Monitor (SPN 3050):");
    for (u32 i = 0; i < 20; ++i) {
        diag.increment_monitor_opportunity(3050);
        if (i % 2 == 0) { // Run 50% of the time
            diag.increment_monitor_execution(3050);
        }
    }
    auto catalyst_ratio = diag.get_performance_ratio(3050);
    if (catalyst_ratio.has_value()) {
        echo::info("  Numerator: ", catalyst_ratio.value().numerator);
        echo::info("  Denominator: ", catalyst_ratio.value().denominator);
        echo::info("  Percentage: ", static_cast<u32>(catalyst_ratio.value().percentage()), "%");
        echo::info("  Meets 75% threshold: ", catalyst_ratio.value().meets_threshold(75) ? "YES" : "NO");
    }

    // Track O2 sensor monitor (SPN 3053)
    echo::info("\nO2 Sensor Monitor (SPN 3053):");
    for (u32 i = 0; i < 20; ++i) {
        diag.increment_monitor_opportunity(3053);
        if (i < 18) { // Run 90% of the time
            diag.increment_monitor_execution(3053);
        }
    }
    auto o2_ratio = diag.get_performance_ratio(3053);
    if (o2_ratio.has_value()) {
        echo::info("  Numerator: ", o2_ratio.value().numerator);
        echo::info("  Denominator: ", o2_ratio.value().denominator);
        echo::info("  Percentage: ", static_cast<u32>(o2_ratio.value().percentage()), "%");
        echo::info("  Meets 75% threshold: ", o2_ratio.value().meets_threshold(75) ? "YES" : "NO");
    }

    // Track EVAP monitor (SPN 3056)
    echo::info("\nEVAP Monitor (SPN 3056):");
    for (u32 i = 0; i < 10; ++i) {
        diag.increment_monitor_opportunity(3056);
        if (i < 6) { // Run 60% of the time
            diag.increment_monitor_execution(3056);
        }
    }
    auto evap_ratio = diag.get_performance_ratio(3056);
    if (evap_ratio.has_value()) {
        echo::info("  Numerator: ", evap_ratio.value().numerator);
        echo::info("  Denominator: ", evap_ratio.value().denominator);
        echo::info("  Percentage: ", static_cast<u32>(evap_ratio.value().percentage()), "%");
        echo::info("  Meets 75% threshold: ", evap_ratio.value().meets_threshold(75) ? "YES" : "NO");
    }

    // Get complete DM20 data
    const auto &dm20 = diag.dm20_data();
    echo::info("\nDM20 Summary:");
    echo::info("  Total monitors tracked: ", dm20.ratios.size());
    echo::info("  Ignition cycles: ", static_cast<u32>(dm20.ignition_cycles));
    echo::info("  OBD conditions met: ", static_cast<u32>(dm20.obd_monitoring_conditions_met));

    u32 ready_count = 0;
    for (const auto &ratio : dm20.ratios) {
        if (ratio.meets_threshold(75))
            ready_count++;
    }
    echo::info("  Monitors ready (≥75%): ", ready_count, " / ", dm20.ratios.size());

    // ═════════════════════════════════════════════════════════════════════════
    // SCENARIO 5: Complete Fault Lifecycle with Freeze Frames
    // ═════════════════════════════════════════════════════════════════════════

    echo::info("\n--- SCENARIO 5: Complete Fault Lifecycle ---");

    DTC boost_dtc;
    boost_dtc.spn = 102; // Boost pressure
    boost_dtc.fmi = FMI::AboveNormal;

    echo::info("1. Activating DTC: SPN=102 (Boost Pressure), FMI=AboveNormal");
    dp::Vector<SPNSnapshot> boost_snapshots;
    boost_snapshots.push_back({.spn = 102, .value = 250}); // 2.5 bar
    boost_snapshots.push_back({.spn = 190, .value = 2200}); // 2200 RPM
    diag.capture_freeze_frame(boost_dtc, boost_snapshots, 50000);
    diag.set_active(boost_dtc);

    echo::info("2. DTC is active - freeze frame captured");
    auto boost_ff = diag.get_freeze_frame(102, FMI::AboveNormal, 0);
    if (boost_ff.has_value()) {
        echo::info("   ✓ Freeze frame available: ", boost_ff.value().snapshots.size(), " snapshots");
    }

    echo::info("3. Clearing DTC (moving to previously active)");
    diag.clear_active(102, FMI::AboveNormal);

    echo::info("4. Freeze frame still available after clear");
    auto boost_ff2 = diag.get_freeze_frame(102, FMI::AboveNormal, 0);
    if (boost_ff2.has_value()) {
        echo::info("   ✓ Freeze frame persists after DTC clear");
    }

    echo::info("5. Explicitly clearing freeze frame");
    diag.clear_freeze_frames(102, FMI::AboveNormal);
    auto boost_ff3 = diag.get_freeze_frame(102, FMI::AboveNormal, 0);
    if (!boost_ff3.has_value()) {
        echo::info("   ✓ Freeze frame removed");
    }

    // ═════════════════════════════════════════════════════════════════════════
    // SCENARIO 6: Diagnostic Lamps and Active DTCs
    // ═════════════════════════════════════════════════════════════════════════

    echo::info("\n--- SCENARIO 6: Diagnostic Lamps and Active DTCs ---");

    DiagnosticLamps lamps;
    lamps.malfunction = LampStatus::On;
    lamps.malfunction_flash = LampFlash::SlowFlash;
    lamps.amber_warning = LampStatus::On;
    lamps.amber_warning_flash = LampFlash::Off;

    diag.set_lamps(lamps);
    echo::info("Diagnostic lamps set:");
    echo::info("  MIL: ON (slow flash)");
    echo::info("  Amber Warning: ON");

    // Get all active DTCs
    const auto &active_dtcs = diag.active_dtcs();
    echo::info("\nActive DTCs: ", active_dtcs.size());
    for (const auto &dtc : active_dtcs) {
        echo::info("  SPN=", dtc.spn, " FMI=", static_cast<u32>(dtc.fmi), " OC=", static_cast<u32>(dtc.occurrence_count));
    }

    // ═════════════════════════════════════════════════════════════════════════
    // Summary
    // ═════════════════════════════════════════════════════════════════════════

    echo::info("\n=== Diagnostic Extended Features Demo Complete ===");
    echo::info("Demonstrated:");
    echo::info("  ✓ DM25 Freeze Frame automatic capture");
    echo::info("  ✓ DM25 Freeze Frame manual capture with SPN snapshots");
    echo::info("  ✓ DM25 Freeze Frame depth limiting (FIFO)");
    echo::info("  ✓ DM20 Monitor Performance Ratios");
    echo::info("  ✓ DM20 Ignition cycles and OBD conditions");
    echo::info("  ✓ DM20 Percentage calculations and thresholds");
    echo::info("  ✓ Complete fault lifecycle with freeze frames");
    echo::info("  ✓ Product and Software identification");
    echo::info("  ✓ Diagnostic lamps management");
    echo::info("  ✓ Active DTC tracking with occurrence counts");

    return 0;
}
