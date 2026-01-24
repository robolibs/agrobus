// Comprehensive Task Controller Server example
// Demonstrates: TCServerConfig, TaskControllerServer lifecycle, value request/command
// handling, peer control assignments, event subscriptions, polling connected clients,
// and sending set_value commands to control sections.

#include <agrobus/net/network_manager.hpp>
#include <agrobus/isobus/tc/server.hpp>
#include <echo/echo.hpp>

using namespace agrobus::net;
using namespace agrobus::isobus;
using namespace agrobus::isobus::tc;

// DDI constants used for section control
namespace ddi {
    inline constexpr DDI ACTUAL_WORK_STATE      = 0x0041;
    inline constexpr DDI SETPOINT_WORK_STATE    = 0x0042;
    inline constexpr DDI SECTION_CONTROL_STATE  = 0x0043;
    inline constexpr DDI ACTUAL_VOLUME_PER_AREA = 0x0001;
    inline constexpr DDI SETPOINT_VOLUME_PER_AREA = 0x0002;
} // namespace ddi

// Storage for process data values received from clients
struct ProcessDataStore {
    dp::Map<u32, i32> values; // key = (element << 16) | ddi

    void store(ElementNumber elem, DDI ddi, i32 value) {
        u32 key = (static_cast<u32>(elem) << 16) | static_cast<u32>(ddi);
        values[key] = value;
    }

    i32 get(ElementNumber elem, DDI ddi, i32 default_val = 0) const {
        u32 key = (static_cast<u32>(elem) << 16) | static_cast<u32>(ddi);
        auto it = values.find(key);
        if (it != values.end()) {
            return it->second;
        }
        return default_val;
    }
};

int main() {
    echo::info("=== Comprehensive TC Server Example ===");

    // ─── Network Setup ──────────────────────────────────────────────────────────
    IsoNet nm;

    // Build the ISO NAME for our TC server
    // Function code 29 = Task Controller per ISO 11783
    Name tc_name = Name::build()
        .set_identity_number(500)
        .set_manufacturer_code(42)
        .set_function_code(29)
        .set_device_class(0)
        .set_industry_group(2)
        .set_self_configurable(true);

    // Claim address 0x27 (common for TCs) on CAN port 0
    auto cf_result = nm.create_internal(tc_name, 0, 0x27);
    if (!cf_result.is_ok()) {
        echo::error("Failed to create internal CF");
        return 1;
    }
    auto* cf = cf_result.value();
    echo::info("TC server CF created at address 0x", cf->preferred_address());

    // ─── TC Server Configuration ────────────────────────────────────────────────
    // Configure as a TC v4 server with 1 boom, 16 sections, no extra channels
    TCServerConfig config = TCServerConfig{}
        .number(0)
        .version(4)
        .booms(1)
        .sections(16)
        .channels(0)
        .options(0);

    echo::info("TCServerConfig: version=", config.tc_version,
               " booms=", config.num_booms,
               " sections=", config.num_sections);

    // ─── Create the Task Controller Server ──────────────────────────────────────
    TaskControllerServer tc_server(nm, cf, config);

    // ─── Process Data Store ─────────────────────────────────────────────────────
    ProcessDataStore store;

    // ─── Register Value Request Handler ─────────────────────────────────────────
    // Called when a client requests a value from us (the server).
    // We respond with whatever we have stored or a sensible default.
    tc_server.on_value_request([&store](ElementNumber elem, DDI ddi, TCClientInfo* client) -> Result<i32> {
        echo::info("  [value_request] elem=", elem, " ddi=0x", ddi,
                   " from client addr=", client ? client->address : 0xFF);

        // Return stored value or defaults
        if (ddi == ddi::SETPOINT_WORK_STATE) {
            return Result<i32>::ok(store.get(elem, ddi, 1)); // default: active
        }
        if (ddi == ddi::SECTION_CONTROL_STATE) {
            return Result<i32>::ok(store.get(elem, ddi, 0xFFFF)); // all sections on
        }
        if (ddi == ddi::SETPOINT_VOLUME_PER_AREA) {
            return Result<i32>::ok(store.get(elem, ddi, 200)); // 200 L/ha default
        }

        // Generic: return whatever is stored
        return Result<i32>::ok(store.get(elem, ddi, 0));
    });

    // ─── Register Value Received Handler ────────────────────────────────────────
    // Called when a client sends a process data value to us.
    // We store it and acknowledge with no errors.
    tc_server.on_value_received([&store](ElementNumber elem, DDI ddi, i32 value, TCClientInfo* client)
                                    -> Result<ProcessDataAcknowledgeErrorCodes> {
        echo::info("  [value_received] elem=", elem, " ddi=0x", ddi,
                   " value=", value,
                   " from client addr=", client ? client->address : 0xFF);

        store.store(elem, ddi, value);

        // Log specific known DDIs
        if (ddi == ddi::ACTUAL_WORK_STATE) {
            echo::info("    -> Work state = ", value == 1 ? "ACTIVE" : "INACTIVE");
        } else if (ddi == ddi::ACTUAL_VOLUME_PER_AREA) {
            echo::info("    -> Actual rate = ", value, " (units per area)");
        }

        return Result<ProcessDataAcknowledgeErrorCodes>::ok(ProcessDataAcknowledgeErrorCodes::NoError);
    });

    // ─── Register Peer Control Assignment Handler ───────────────────────────────
    // Called when a client requests a peer-to-peer process data linkage.
    tc_server.on_peer_control_assignment([](ElementNumber src_elem, DDI src_ddi,
                                            ElementNumber dst_elem, DDI dst_ddi) -> Result<void> {
        echo::info("  [peer_control] src_elem=", src_elem, " src_ddi=0x", src_ddi,
                   " -> dst_elem=", dst_elem, " dst_ddi=0x", dst_ddi);

        // Accept any assignment for this demo
        echo::info("    -> Assignment accepted");
        return {};
    });

    // ─── Subscribe to State Change Events ───────────────────────────────────────
    tc_server.on_state_change.subscribe([](TCServerState state) {
        const char* state_str = "Unknown";
        switch (state) {
            case TCServerState::Disconnected:   state_str = "Disconnected"; break;
            case TCServerState::WaitForClients: state_str = "WaitForClients"; break;
            case TCServerState::Active:         state_str = "Active"; break;
        }
        echo::info("  [state_change] TC Server -> ", state_str);
    });

    // ─── Subscribe to Client Connected Events ───────────────────────────────────
    tc_server.on_client_connected.subscribe([](Address addr) {
        echo::info("  [client_connected] New TC client at address 0x", addr);
    });

    tc_server.on_client_disconnected.subscribe([](Address addr) {
        echo::info("  [client_disconnected] TC client at address 0x", addr, " disconnected");
    });

    // ─── Start the TC Server ────────────────────────────────────────────────────
    echo::info("\nStarting TC Server...");
    auto start_result = tc_server.start();
    if (!start_result.is_ok()) {
        echo::error("Failed to start TC Server");
        return 1;
    }
    echo::info("TC Server started, state=", static_cast<u8>(tc_server.state()));

    // ─── Main Update Loop ───────────────────────────────────────────────────────
    // Run 20 iterations of 200ms each (4 seconds simulated time).
    // The TC status broadcast fires every 2000ms, so we expect 2 broadcasts.
    echo::info("\nEntering main loop (20 iterations x 200ms = 4s simulated)...\n");

    constexpr u32 LOOP_ITERATIONS = 20;
    constexpr u32 DT_MS = 200;

    for (u32 tick = 0; tick < LOOP_ITERATIONS; ++tick) {
        echo::info("[tick ", tick, "] elapsed=", (tick + 1) * DT_MS, " ms");

        // Update the network manager (handles address claiming, message dispatch)
        nm.update(DT_MS);

        // Update the TC server (handles status broadcasts, client timeouts)
        tc_server.update(DT_MS);

        // ─── Poll connected clients for section states ──────────────────────────
        // Every 5th tick (1 second), request section states from all clients
        if (tick > 0 && tick % 5 == 0 && !tc_server.clients().empty()) {
            echo::info("  Polling ", tc_server.clients().size(), " client(s) for section states...");

            for (const auto& client : tc_server.clients()) {
                // Create a temporary CF to address this client
                ControlFunction dest_cf;
                dest_cf.address = client.address;

                // Request actual work state from root element (element 1)
                tc_server.send_request_value(1, ddi::ACTUAL_WORK_STATE, &dest_cf);
                echo::info("    -> Requested ACTUAL_WORK_STATE from elem=1, addr=0x", client.address);

                // Request actual application rate from root element
                tc_server.send_request_value(1, ddi::ACTUAL_VOLUME_PER_AREA, &dest_cf);
                echo::info("    -> Requested ACTUAL_VOLUME_PER_AREA from elem=1, addr=0x", client.address);
            }
        }

        // ─── Send set_value commands to control sections ────────────────────────
        // At tick 10 and 15, send section control commands to connected clients
        if (tick == 10 && !tc_server.clients().empty()) {
            echo::info("  Sending section ON command to all clients...");
            for (const auto& client : tc_server.clients()) {
                ControlFunction dest_cf;
                dest_cf.address = client.address;

                // Turn on all 16 sections (bitmask 0xFFFF)
                tc_server.send_set_value(1, ddi::SECTION_CONTROL_STATE, 0xFFFF, &dest_cf);
                echo::info("    -> Set SECTION_CONTROL_STATE=0xFFFF (all on) for addr=0x", client.address);

                // Set target application rate to 250 L/ha
                tc_server.send_set_value(1, ddi::SETPOINT_VOLUME_PER_AREA, 250, &dest_cf);
                echo::info("    -> Set SETPOINT_VOLUME_PER_AREA=250 for addr=0x", client.address);
            }
        }

        if (tick == 15 && !tc_server.clients().empty()) {
            echo::info("  Sending partial section OFF command...");
            for (const auto& client : tc_server.clients()) {
                ControlFunction dest_cf;
                dest_cf.address = client.address;

                // Turn off sections 9-16 (only sections 1-8 remain on: 0x00FF)
                tc_server.send_set_value(1, ddi::SECTION_CONTROL_STATE, 0x00FF, &dest_cf);
                echo::info("    -> Set SECTION_CONTROL_STATE=0x00FF (sections 1-8 on) for addr=0x",
                           client.address);
            }
        }
    }

    // ─── Summary ────────────────────────────────────────────────────────────────
    echo::info("\n=== Server Summary ===");
    echo::info("Final state: ", static_cast<u8>(tc_server.state()));
    echo::info("Connected clients: ", tc_server.clients().size());

    for (const auto& client : tc_server.clients()) {
        echo::info("  Client addr=0x", client.address,
                   " pool_activated=", client.pool_activated,
                   " last_status_ms=", client.last_status_ms);
    }

    echo::info("Stored process data entries: ", store.values.size());

    // ─── Stop the Server ────────────────────────────────────────────────────────
    echo::info("\nStopping TC Server...");
    tc_server.stop();
    echo::info("TC Server stopped, state=", static_cast<u8>(tc_server.state()));

    echo::info("\n=== Comprehensive TC Server Example Complete ===");
    return 0;
}
