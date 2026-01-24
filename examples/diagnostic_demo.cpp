#include <agrobus/net/network_manager.hpp>
#include <agrobus/j1939/diagnostic.hpp>
#include <echo/echo.hpp>

using namespace agrobus::net;
using namespace agrobus::j1939;

int main() {
    echo::info("=== Diagnostic Protocol Demo ===");

    IsoNet nm;
    Name name = Name::build()
        .set_identity_number(1)
        .set_manufacturer_code(42)
        .set_function_code(25);

    auto cf_result = nm.create_internal(name, 0, 0x28);
    auto* cf = cf_result.value();

    DiagnosticProtocol diag(nm, cf);
    diag.initialize();

    // Subscribe to remote diagnostics
    diag.on_dm1_received.subscribe([](const dp::Vector<DTC>& dtcs, Address src) {
        echo::info("DM1 from 0x", src, ": ", dtcs.size(), " active DTCs");
        for (const auto& dtc : dtcs) {
            echo::info("  SPN=", dtc.spn, " FMI=", static_cast<int>(dtc.fmi),
                       " OC=", dtc.occurrence_count);
        }
    });

    // Set our own DTCs
    DTC low_voltage{520, FMI::VoltageLow, 0};
    diag.set_active(low_voltage);
    echo::info("Set DTC: SPN=520 (Battery Voltage) FMI=VoltageLow");

    DTC sensor_fail{190, FMI::MechanicalFail, 0};
    diag.set_active(sensor_fail);
    echo::info("Set DTC: SPN=190 (Engine Speed) FMI=MechanicalFail");

    // Set warning lamps
    DiagnosticLamps lamps;
    lamps.amber_warning = LampStatus::On;
    lamps.amber_warning_flash = LampFlash::SlowFlash;
    diag.set_lamps(lamps);
    echo::info("Amber warning lamp: ON (slow flash)");

    // Report status
    echo::info("\nActive DTCs: ", diag.active_dtcs().size());
    for (const auto& dtc : diag.active_dtcs()) {
        echo::info("  SPN=", dtc.spn, " FMI=", static_cast<int>(dtc.fmi));
    }

    // Clear one DTC
    diag.clear_active(520, FMI::VoltageLow);
    echo::info("\nAfter clearing SPN=520:");
    echo::info("  Active: ", diag.active_dtcs().size());
    echo::info("  Previous: ", diag.previous_dtcs().size());

    return 0;
}
