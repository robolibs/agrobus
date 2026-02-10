// pgn_request_handler.cpp
//
// Comprehensive example demonstrating the PGNRequestProtocol class.
// Shows how to register PGN responders, subscribe to request/ack events,
// send PGN requests (broadcast and destination-specific), and send
// positive/negative acknowledgments.

#include <agrobus/net/network_manager.hpp>
#include <agrobus/j1939/pgn_request.hpp>
#include <echo/echo.hpp>

using namespace agrobus::net;
using namespace agrobus::j1939;

// Custom PGN for application-specific data (proprietary B range)
static constexpr PGN PGN_CUSTOM_STATUS = 0xFF20;

// Simulated DTC active lamp data for DM1 response
static dp::Vector<u8> build_dm1_response() {
    dp::Vector<u8> data(8, 0xFF);
    // Byte 0-1: Malfunction indicator lamp status (off)
    data[0] = 0x00;
    data[1] = 0x00;
    // Byte 2-3: SPN (Suspect Parameter Number) = 100
    data[2] = 0x64;
    data[3] = 0x00;
    // Byte 4: FMI (Failure Mode Identifier) = 3 (voltage above normal)
    data[4] = 0x03;
    // Byte 5: Occurrence count = 1
    data[5] = 0x01;
    // Byte 6-7: Reserved
    data[6] = 0xFF;
    data[7] = 0xFF;
    return data;
}

// Simulated software identification response
static dp::Vector<u8> build_software_id_response() {
    dp::Vector<u8> data(8, 0xFF);
    // Byte 0: Number of software ID fields = 1
    data[0] = 0x01;
    // Bytes 1-7: ASCII software version "v2.1.0\0"
    data[1] = 'v';
    data[2] = '2';
    data[3] = '.';
    data[4] = '1';
    data[5] = '.';
    data[6] = '0';
    data[7] = 0x00;
    return data;
}

// Simulated custom status response
static dp::Vector<u8> build_custom_status_response() {
    dp::Vector<u8> data(8, 0xFF);
    // Byte 0: Operating mode (0x01 = normal operation)
    data[0] = 0x01;
    // Byte 1: System health (0-100%)
    data[1] = 0x62; // 98%
    // Byte 2-3: Operating hours (little-endian), 1234 hours
    data[2] = 0xD2;
    data[3] = 0x04;
    // Byte 4: Temperature (offset by 40, so 0x50 = 40 deg C)
    data[4] = 0x50;
    // Byte 5: Battery voltage (0.05V/bit), 0xF0 = 12.0V
    data[5] = 0xF0;
    // Byte 6-7: Reserved
    data[6] = 0xFF;
    data[7] = 0xFF;
    return data;
}

int main() {
    echo::info("=== PGN Request Handler Demo ===");

    // ─── Step 1: Set up the network manager ─────────────────────────────────────
    IsoNet nm(NetworkConfig{}.ports(1).bus_load(true));

    // Build our ECU NAME (ISO 11783 64-bit device identity)
    Name ecu_name = Name::build()
        .set_identity_number(1001)
        .set_manufacturer_code(200)
        .set_function_code(0x1E)       // General-purpose ECU
        .set_device_class(0x19)        // Non-specific system
        .set_industry_group(2)         // Agriculture
        .set_self_configurable(true);

    // Create our internal control function with preferred address 0x80
    auto cf_result = nm.create_internal(ecu_name, 0, 0x80);
    if (!cf_result.is_ok()) {
        echo::error("Failed to create internal CF: ", cf_result.error().message);
        return 1;
    }
    InternalCF *our_cf = cf_result.value();
    echo::info("Internal CF created at preferred address 0x80");

    // ─── Step 2: Create the PGN Request Protocol ────────────────────────────────
    PGNRequestProtocol pgn_req(nm, our_cf);

    auto init_result = pgn_req.initialize();
    if (!init_result.is_ok()) {
        echo::error("Failed to initialize PGNRequestProtocol: ", init_result.error().message);
        return 1;
    }
    echo::info("PGNRequestProtocol initialized");

    // ─── Step 3: Register PGN responders ────────────────────────────────────────
    // These functions are called automatically when another device requests
    // the corresponding PGN from us.

    // Responder for DM1 (Active Diagnostic Trouble Codes)
    pgn_req.register_responder(PGN_DM1, []() -> dp::Vector<u8> {
        echo::debug("Responding to DM1 request (active DTCs)");
        return build_dm1_response();
    });
    echo::info("Registered responder for PGN_DM1 (0xFECA)");

    // Responder for Software Identification
    pgn_req.register_responder(PGN_SOFTWARE_ID, []() -> dp::Vector<u8> {
        echo::debug("Responding to Software ID request");
        return build_software_id_response();
    });
    echo::info("Registered responder for PGN_SOFTWARE_ID (0xFEDA)");

    // Responder for custom application-specific PGN
    pgn_req.register_responder(PGN_CUSTOM_STATUS, []() -> dp::Vector<u8> {
        echo::debug("Responding to custom status request");
        return build_custom_status_response();
    });
    echo::info("Registered responder for PGN_CUSTOM_STATUS (0xFF20)");

    // ─── Step 4: Subscribe to events ────────────────────────────────────────────
    // Notified whenever any PGN request is received (before responder is called)
    pgn_req.on_request_received += [](PGN requested_pgn, Address requester) {
        echo::info("PGN request received: pgn=0x", requested_pgn,
                   " from address=0x", requester);

        // Application-level logic: log which PGNs are being requested
        if (requested_pgn == PGN_DM1) {
            echo::debug("  -> Diagnostic data requested (DM1)");
        } else if (requested_pgn == PGN_SOFTWARE_ID) {
            echo::debug("  -> Software identification requested");
        } else {
            echo::debug("  -> PGN 0x", requested_pgn, " requested");
        }
    };

    // Notified whenever an acknowledgment is received from another device
    pgn_req.on_ack_received += [](const Acknowledgment &ack) {
        const char *type_str = "Unknown";
        switch (ack.control) {
            case AckControl::PositiveAck:  type_str = "Positive ACK"; break;
            case AckControl::NegativeAck:  type_str = "Negative ACK (NACK)"; break;
            case AckControl::AccessDenied: type_str = "Access Denied"; break;
            case AckControl::CannotRespond: type_str = "Cannot Respond"; break;
        }
        echo::info("Acknowledgment received: ", type_str,
                   " for PGN=0x", ack.acknowledged_pgn,
                   " from address=0x", ack.address);
    };

    echo::info("Event handlers connected");

    // ─── Step 5: Demonstrate sending PGN requests ───────────────────────────────
    echo::info("");
    echo::info("--- Sending PGN Requests ---");

    // Broadcast request for DM1 (all devices with DM1 data should respond)
    echo::info("Sending broadcast request for PGN_DM1...");
    auto req_result = pgn_req.request(PGN_DM1, BROADCAST_ADDRESS);
    if (req_result.is_ok()) {
        echo::info("  Broadcast DM1 request sent successfully");
    } else {
        echo::warn("  Failed to send DM1 request: ", req_result.error().message);
    }

    // Destination-specific request for Software ID to address 0x00 (typically TC)
    Address tc_address = 0x00;
    echo::info("Sending destination-specific request for PGN_SOFTWARE_ID to 0x00...");
    req_result = pgn_req.request(PGN_SOFTWARE_ID, tc_address);
    if (req_result.is_ok()) {
        echo::info("  Software ID request sent to address 0x00");
    } else {
        echo::warn("  Failed to send Software ID request: ", req_result.error().message);
    }

    // Broadcast request for Vehicle Speed
    echo::info("Sending broadcast request for PGN_VEHICLE_SPEED...");
    req_result = pgn_req.request(PGN_VEHICLE_SPEED, BROADCAST_ADDRESS);
    if (req_result.is_ok()) {
        echo::info("  Vehicle speed request sent (broadcast)");
    } else {
        echo::warn("  Failed to send vehicle speed request: ", req_result.error().message);
    }

    // Destination-specific request for custom PGN
    Address implement_address = 0x28;
    echo::info("Sending destination-specific request for PGN_CUSTOM_STATUS to 0x28...");
    req_result = pgn_req.request(PGN_CUSTOM_STATUS, implement_address);
    if (req_result.is_ok()) {
        echo::info("  Custom status request sent to address 0x28");
    } else {
        echo::warn("  Failed to send custom status request: ", req_result.error().message);
    }

    // ─── Step 6: Demonstrate sending acknowledgments ────────────────────────────
    echo::info("");
    echo::info("--- Sending Acknowledgments ---");

    // Send a positive acknowledgment (e.g., confirming a command was executed)
    Address commander_address = 0x00;
    echo::info("Sending positive ACK for PGN_LANGUAGE_COMMAND to 0x00...");
    auto ack_result = pgn_req.send_ack(AckControl::PositiveAck, PGN_LANGUAGE_COMMAND, commander_address);
    if (ack_result.is_ok()) {
        echo::info("  Positive ACK sent");
    } else {
        echo::warn("  Failed to send positive ACK: ", ack_result.error().message);
    }

    // Send a negative acknowledgment (e.g., unsupported PGN was requested)
    echo::info("Sending negative ACK for unknown PGN 0xABCD to 0x28...");
    ack_result = pgn_req.send_ack(AckControl::NegativeAck, 0xABCD, implement_address);
    if (ack_result.is_ok()) {
        echo::info("  Negative ACK (NACK) sent");
    } else {
        echo::warn("  Failed to send NACK: ", ack_result.error().message);
    }

    // Send an Access Denied acknowledgment
    echo::info("Sending Access Denied for PGN_COMMANDED_ADDRESS to 0x28...");
    ack_result = pgn_req.send_ack(AckControl::AccessDenied, PGN_COMMANDED_ADDRESS, implement_address);
    if (ack_result.is_ok()) {
        echo::info("  Access Denied ACK sent");
    } else {
        echo::warn("  Failed to send Access Denied: ", ack_result.error().message);
    }

    // Send a Cannot Respond acknowledgment
    echo::info("Sending Cannot Respond for PGN_DM11 to 0x00...");
    ack_result = pgn_req.send_ack(AckControl::CannotRespond, PGN_DM11, commander_address);
    if (ack_result.is_ok()) {
        echo::info("  Cannot Respond ACK sent");
    } else {
        echo::warn("  Failed to send Cannot Respond: ", ack_result.error().message);
    }

    // ─── Step 7: Simulate the network update loop ───────────────────────────────
    echo::info("");
    echo::info("--- Simulating Network Update Loop ---");
    echo::info("In a real application, you would call nm.update(elapsed_ms) in a loop.");
    echo::info("This processes incoming CAN frames, triggers responders, and emits events.");

    // Simulate a few update cycles (no real CAN bus connected in this demo)
    for (u32 cycle = 0; cycle < 5; ++cycle) {
        nm.update(10); // 10ms per cycle
    }
    echo::info("Simulated 5 update cycles (50ms total)");

    echo::info("");
    echo::info("=== PGN Request Handler Demo Complete ===");
    echo::info("Summary:");
    echo::info("  - Registered 3 PGN responders (DM1, Software ID, Custom Status)");
    echo::info("  - Connected event handlers for requests and acknowledgments");
    echo::info("  - Sent 4 PGN requests (2 broadcast, 2 destination-specific)");
    echo::info("  - Sent 4 acknowledgments (positive, negative, access denied, cannot respond)");

    return 0;
}
