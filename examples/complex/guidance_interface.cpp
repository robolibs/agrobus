#include <agrobus/net/network_manager.hpp>
#include <agrobus/isobus/guidance.hpp>
#include <echo/echo.hpp>

using namespace agrobus::net;
using namespace agrobus::isobus;

// Guidance status byte definitions (per ISO 11783-7)
namespace guidance_status {
    inline constexpr u8 NOT_REQUESTING   = 0x00;
    inline constexpr u8 REQUESTING_LEFT  = 0x01;
    inline constexpr u8 REQUESTING_RIGHT = 0x02;
    inline constexpr u8 ACTIVE           = 0x04;
}

int main() {
    echo::info("=== Guidance Interface Demo ===");
    echo::info("Demonstrates a guidance controller sending curvature commands");
    echo::info("and receiving machine info feedback from the implement.\n");

    // ─── Network Setup ─────────────────────────────────────────────────────
    IsoNet nm;

    Name name = Name::build()
        .set_identity_number(500)
        .set_manufacturer_code(42)
        .set_function_code(28)    // Guidance controller
        .set_industry_group(2)    // Agriculture
        .set_self_configurable(true);

    auto cf_result = nm.create_internal(name, 0, 0x28);
    auto* cf = cf_result.value();
    echo::info("Created guidance controller CF at address 0x28");

    // ─── GuidanceInterface Setup ────────────────────────────────────────────
    // Enable listening for both machine info (from implement) and
    // system commands (from other guidance sources on the bus)
    GuidanceConfig config = GuidanceConfig{}
        .machine_info(true)
        .system_command(true);

    GuidanceInterface guidance(nm, cf, config);

    auto init_result = guidance.initialize();
    if (!init_result.is_ok()) {
        echo::error("Failed to initialize guidance interface");
        return 1;
    }
    echo::info("GuidanceInterface initialized (listening for machine + system)");

    // ─── Event Subscriptions ────────────────────────────────────────────────
    // Subscribe to guidance machine info (feedback from the implement ECU)
    guidance.on_guidance_machine.subscribe([](const GuidanceData& data) {
        echo::info("  [MACHINE] Received machine info:");
        if (data.curvature) {
            f64 curv = *data.curvature;
            echo::info("    Curvature: ", curv, " 1/m");
            if (curv != 0.0) {
                echo::info("    Radius:    ", 1.0 / curv, " m");
            }
        }
        if (data.status) {
            echo::info("    Status:    0x", *data.status);
        }
        echo::info("    Timestamp: ", data.timestamp_us, " us");
    });

    // Subscribe to guidance system commands (from other controllers on the bus)
    guidance.on_guidance_system.subscribe([](const GuidanceData& data) {
        echo::info("  [SYSTEM] Received system command from bus:");
        if (data.curvature) {
            echo::info("    Curvature: ", *data.curvature, " 1/m");
        }
        if (data.status) {
            echo::info("    Status:    0x", *data.status);
        }
    });

    echo::info("Event handlers registered\n");

    // ─── Guidance Controller Simulation ─────────────────────────────────────
    // Simulate a guidance controller that:
    //   1. Sends curvature commands to steer the implement
    //   2. Receives machine info feedback
    //   3. Gradually adjusts curvature to follow a curved path
    //
    // Curvature values:
    //   0.0    = straight ahead
    //   0.01   = 100m radius turn (gentle)
    //   0.02   = 50m radius turn (moderate)
    //   0.05   = 20m radius turn (tight)

    echo::info("─── Starting Guidance Simulation (8 iterations) ───\n");

    // Target curvature ramps up then straightens out (simulating entering
    // and exiting a curved headland turn)
    f64 curvature_profile[] = {
        0.000,   // Straight approach
        0.005,   // Begin gentle turn (200m radius)
        0.010,   // Tighten to 100m radius
        0.020,   // Moderate turn (50m radius)
        0.020,   // Hold moderate turn
        0.010,   // Begin straightening
        0.005,   // Almost straight (200m radius)
        0.000    // Straight exit
    };

    for (u32 tick = 0; tick < 8; ++tick) {
        u32 dt_ms = 100;
        f64 target_curvature = curvature_profile[tick];

        echo::info("[Tick ", tick, "] ─────────────────────────────────────");

        // Determine steering direction from curvature
        u8 command_status = guidance_status::NOT_REQUESTING;
        if (target_curvature > 0.001) {
            command_status = guidance_status::REQUESTING_RIGHT;
        } else if (target_curvature < -0.001) {
            command_status = guidance_status::REQUESTING_LEFT;
        }

        // Send guidance system command (controller -> implement)
        GuidanceData system_cmd;
        system_cmd.curvature = target_curvature;
        system_cmd.status = command_status;

        if (target_curvature != 0.0) {
            echo::info("  Commanding curvature: ", target_curvature,
                       " 1/m (radius: ", 1.0 / target_curvature, " m)");
        } else {
            echo::info("  Commanding curvature: 0.0 (straight)");
        }

        auto send_result = guidance.send_system_command(system_cmd);
        if (!send_result.is_ok()) {
            echo::error("  Failed to send system command");
        }

        // Simulate the implement responding with machine info
        // In a real system this would come from the implement ECU over CAN.
        // Here we simulate a slight lag: machine follows at 80% of commanded curvature.
        GuidanceData machine_feedback;
        machine_feedback.curvature = target_curvature * 0.8;
        machine_feedback.status = (target_curvature > 0.001)
                                      ? guidance_status::ACTIVE
                                      : guidance_status::NOT_REQUESTING;

        auto info_result = guidance.send_machine_info(machine_feedback);
        if (!info_result.is_ok()) {
            echo::error("  Failed to send machine info");
        }

        // Update the network (processes callbacks, dispatches messages)
        nm.update(dt_ms);

        // Query the latest cached data
        auto latest_machine = guidance.latest_machine();
        auto latest_system = guidance.latest_system();

        if (latest_machine.has_value()) {
            echo::info("  Latest machine curvature: ",
                       latest_machine->curvature.has_value()
                           ? *latest_machine->curvature
                           : 0.0,
                       " 1/m");
        }

        if (latest_system.has_value()) {
            echo::info("  Latest system curvature:  ",
                       latest_system->curvature.has_value()
                           ? *latest_system->curvature
                           : 0.0,
                       " 1/m");
        }

        echo::info("");
    }

    // ─── Summary ────────────────────────────────────────────────────────────
    echo::info("─── Simulation Complete ───\n");

    auto final_machine = guidance.latest_machine();
    auto final_system = guidance.latest_system();

    echo::info("Final state:");
    if (final_machine.has_value() && final_machine->curvature.has_value()) {
        echo::info("  Machine curvature: ", *final_machine->curvature, " 1/m");
    } else {
        echo::info("  Machine curvature: (none)");
    }
    if (final_system.has_value() && final_system->curvature.has_value()) {
        echo::info("  System curvature:  ", *final_system->curvature, " 1/m");
    } else {
        echo::info("  System curvature:  (none)");
    }

    echo::info("\n=== Guidance Interface Demo Complete ===");
    return 0;
}
