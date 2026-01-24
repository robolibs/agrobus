// Comprehensive DiagnosticProtocol example demonstrating:
// - DM1 (active DTCs), DM2 (previously active DTCs)
// - DM11 (clear/reset all active DTCs)
// - DM13 (suspend/resume broadcast)
// - DM22 (individual DTC clear/reset)
// - Product and software identification
// - Event-driven callback system
// - Auto-send with periodic update loop

#include <agrobus/net/network_manager.hpp>
#include <agrobus/j1939/diagnostic.hpp>
#include <echo/echo.hpp>

using namespace agrobus::net;
using namespace agrobus::j1939;

int main() {
    echo::info("=== Comprehensive Diagnostic Protocol Demo ===");
    echo::info("");

    // -------------------------------------------------------------------------
    // Step 1: Set up IsoNet, Name, and InternalCF
    // -------------------------------------------------------------------------
    echo::info("[Setup] Creating network manager and control function...");

    IsoNet nm;

    Name name = Name::build()
        .set_identity_number(100)
        .set_manufacturer_code(512)
        .set_function_code(25)
        .set_device_class(7)
        .set_industry_group(2)
        .set_self_configurable(true);

    auto cf_result = nm.create_internal(name, 0, 0x28);
    auto* cf = cf_result.value();
    echo::info("[Setup] Internal CF created at address 0x28");

    // -------------------------------------------------------------------------
    // Step 2: Create DiagnosticProtocol with auto-send enabled via config
    // -------------------------------------------------------------------------
    echo::info("[Setup] Creating diagnostic protocol with auto-send...");

    DiagnosticConfig config;
    config.interval(1000).enable_auto_send(true);

    DiagnosticProtocol diag(nm, cf, config);
    auto init_result = diag.initialize();
    if (!init_result.is_ok()) {
        echo::error("Failed to initialize diagnostic protocol");
        return 1;
    }
    echo::info("[Setup] Diagnostic protocol initialized");

    // -------------------------------------------------------------------------
    // Step 3: Set product and software identification
    // -------------------------------------------------------------------------
    echo::info("");
    echo::info("[ID] Setting product and software identification...");

    diag.set_product_id(ProductIdentification{
        "RoboAgri",          // make
        "SprayerECU-3000",   // model
        "SN-2024-00742"      // serial number
    });
    echo::info("[ID] Product: RoboAgri SprayerECU-3000 (SN-2024-00742)");

    diag.set_software_id(SoftwareIdentification{{
        "FW-2.4.1",       // firmware version
        "APP-1.7.0",      // application version
        "CAL-3.2.0"       // calibration version
    }});
    echo::info("[ID] Software: FW-2.4.1 / APP-1.7.0 / CAL-3.2.0");

    // -------------------------------------------------------------------------
    // Step 4: Set active DTCs with various SPNs and FMIs
    // -------------------------------------------------------------------------
    echo::info("");
    echo::info("[DTC] Setting active diagnostic trouble codes...");

    // SPN 520 = Battery voltage, FMI 4 = VoltageLow
    diag.set_active(DTC{520, FMI::VoltageLow, 0});
    echo::info("[DTC] Active: SPN=520 (Battery Voltage) FMI=VoltageLow");

    // SPN 190 = Engine speed sensor, FMI 2 = Erratic
    diag.set_active(DTC{190, FMI::Erratic, 0});
    echo::info("[DTC] Active: SPN=190 (Engine Speed) FMI=Erratic");

    // SPN 100 = Oil pressure, FMI 1 = BelowNormal
    diag.set_active(DTC{100, FMI::BelowNormal, 0});
    echo::info("[DTC] Active: SPN=100 (Oil Pressure) FMI=BelowNormal");

    // SPN 110 = Coolant temperature, FMI 0 = AboveNormal
    diag.set_active(DTC{110, FMI::AboveNormal, 0});
    echo::info("[DTC] Active: SPN=110 (Coolant Temp) FMI=AboveNormal");

    echo::info("[DTC] Total active DTCs: ", diag.active_dtcs().size());

    // -------------------------------------------------------------------------
    // Step 5: Set lamp status (amber warning on with slow flash)
    // -------------------------------------------------------------------------
    echo::info("");
    echo::info("[Lamps] Configuring diagnostic lamp status...");

    DiagnosticLamps lamps;
    lamps.amber_warning = LampStatus::On;
    lamps.amber_warning_flash = LampFlash::SlowFlash;
    lamps.malfunction = LampStatus::Off;
    lamps.red_stop = LampStatus::Off;
    lamps.engine_protect = LampStatus::Off;
    diag.set_lamps(lamps);
    echo::info("[Lamps] Amber warning: ON (slow flash)");
    echo::info("[Lamps] Malfunction indicator: OFF");
    echo::info("[Lamps] Red stop: OFF");
    echo::info("[Lamps] Engine protect: OFF");

    // -------------------------------------------------------------------------
    // Step 6: Subscribe to DM1, DM2, DM11, DM13, DM22 events
    // -------------------------------------------------------------------------
    echo::info("");
    echo::info("[Events] Subscribing to diagnostic message events...");

    diag.on_dm1_received.subscribe([](const dp::Vector<DTC>& dtcs, Address src) {
        echo::info("  >> DM1 received from 0x", src, ": ", dtcs.size(), " active DTC(s)");
        for (const auto& dtc : dtcs) {
            echo::info("     SPN=", dtc.spn,
                       " FMI=", static_cast<int>(dtc.fmi),
                       " OC=", dtc.occurrence_count);
        }
    });

    diag.on_dm2_received.subscribe([](const dp::Vector<DTC>& dtcs, Address src) {
        echo::info("  >> DM2 received from 0x", src, ": ", dtcs.size(), " previously active DTC(s)");
        for (const auto& dtc : dtcs) {
            echo::info("     SPN=", dtc.spn,
                       " FMI=", static_cast<int>(dtc.fmi),
                       " OC=", dtc.occurrence_count);
        }
    });

    diag.on_dm11_received.subscribe([](Address src) {
        echo::warn("  >> DM11 clear/reset request from 0x", src);
    });

    diag.on_dm13_received.subscribe([](const DM13Signals& signals, Address src) {
        echo::info("  >> DM13 broadcast control from 0x", src);
        echo::info("     DM1 signal: ", static_cast<int>(signals.dm1_signal));
        echo::info("     DM2 signal: ", static_cast<int>(signals.dm2_signal));
        echo::info("     Hold signal: ", static_cast<int>(signals.hold_signal));
        if (signals.suspend_duration_s != 0xFFFF) {
            echo::info("     Suspend duration: ", signals.suspend_duration_s, "s");
        } else {
            echo::info("     Suspend duration: indefinite");
        }
    });

    diag.on_dm22_received.subscribe([](DM22Control control, u32 spn, FMI fmi, Address src) {
        echo::info("  >> DM22 individual DTC operation from 0x", src);
        echo::info("     Control: 0x", static_cast<int>(control));
        echo::info("     SPN=", spn, " FMI=", static_cast<int>(fmi));
    });

    echo::info("[Events] All diagnostic event handlers registered");

    // -------------------------------------------------------------------------
    // Step 7: Enable auto-send and run update loop (10 iterations)
    // -------------------------------------------------------------------------
    echo::info("");
    echo::info("[Loop] Starting diagnostic update loop (10 iterations, 500ms each)...");
    echo::info("");

    diag.enable_auto_send(1000);  // send DM1 every 1000ms

    u32 elapsed_per_tick = 500;  // 500ms per iteration

    for (i32 iteration = 1; iteration <= 10; ++iteration) {
        echo::info("--- Iteration ", iteration, "/10 ---");

        // Step 8: After iteration 4, clear a specific DTC and send DM2
        if (iteration == 4) {
            echo::info("[Action] Clearing DTC: SPN=520 FMI=VoltageLow");
            auto clear_result = diag.clear_active(520, FMI::VoltageLow);
            if (clear_result.is_ok()) {
                echo::info("[Action] DTC cleared successfully, moved to previously active");
                echo::info("[Action] Active DTCs remaining: ", diag.active_dtcs().size());
                echo::info("[Action] Previously active DTCs: ", diag.previous_dtcs().size());
            }

            echo::info("[Action] Sending DM2 (previously active DTCs)...");
            diag.send_dm2();
        }

        // Step 9: At iteration 6, demonstrate DM13 suspend broadcast
        if (iteration == 6) {
            echo::info("[Action] Sending DM13: Suspend DM1 broadcasts for 30 seconds");
            DM13Signals suspend_signals;
            suspend_signals.dm1_signal = DM13Command::SuspendBroadcast;
            suspend_signals.dm2_signal = DM13Command::DoNotCare;
            suspend_signals.hold_signal = DM13Command::DoNotCare;
            suspend_signals.suspend_duration_s = 30;
            diag.send_dm13(suspend_signals);
        }

        // At iteration 8, resume DM1 broadcasts
        if (iteration == 8) {
            echo::info("[Action] Sending DM13: Resume DM1 broadcasts");
            DM13Signals resume_signals;
            resume_signals.dm1_signal = DM13Command::ResumeBroadcast;
            resume_signals.dm2_signal = DM13Command::DoNotCare;
            resume_signals.hold_signal = DM13Command::DoNotCare;
            resume_signals.suspend_duration_s = 0xFFFF;
            diag.send_dm13(resume_signals);
        }

        // Step 10: At iteration 9, demonstrate DM22 individual DTC clear
        if (iteration == 9) {
            echo::info("[Action] Sending DM22: Request clear of SPN=190 FMI=Erratic to address 0x30");
            diag.send_dm22_clear(
                DM22Control::ClearActive,
                190,
                FMI::Erratic,
                Address{0x30}
            );
        }

        // Run the periodic update (triggers auto-send when timer elapses)
        diag.update(elapsed_per_tick);

        // Print current status
        echo::info("[Status] Active DTCs: ", diag.active_dtcs().size(),
                   " | Previous DTCs: ", diag.previous_dtcs().size());
        for (const auto& dtc : diag.active_dtcs()) {
            echo::debug("  Active: SPN=", dtc.spn,
                        " FMI=", static_cast<int>(dtc.fmi),
                        " OC=", dtc.occurrence_count);
        }
        echo::info("");
    }

    // -------------------------------------------------------------------------
    // Final Summary
    // -------------------------------------------------------------------------
    echo::info("=== Final Diagnostic State ===");
    echo::info("Active DTCs: ", diag.active_dtcs().size());
    for (const auto& dtc : diag.active_dtcs()) {
        echo::info("  SPN=", dtc.spn,
                   " FMI=", static_cast<int>(dtc.fmi),
                   " OC=", dtc.occurrence_count);
    }

    echo::info("Previously active DTCs: ", diag.previous_dtcs().size());
    for (const auto& dtc : diag.previous_dtcs()) {
        echo::info("  SPN=", dtc.spn,
                   " FMI=", static_cast<int>(dtc.fmi),
                   " OC=", dtc.occurrence_count);
    }

    auto current_lamps = diag.lamps();
    echo::info("Lamp status:");
    echo::info("  Amber warning: ", static_cast<int>(current_lamps.amber_warning),
               " (flash: ", static_cast<int>(current_lamps.amber_warning_flash), ")");
    echo::info("  Malfunction: ", static_cast<int>(current_lamps.malfunction));
    echo::info("  Red stop: ", static_cast<int>(current_lamps.red_stop));
    echo::info("  Engine protect: ", static_cast<int>(current_lamps.engine_protect));

    echo::info("");
    echo::info("=== Demo Complete ===");
    return 0;
}
