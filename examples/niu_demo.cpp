#include <agrobus.hpp>
#include <echo/echo.hpp>

using namespace agrobus::net;

int main() {
    echo::info("=== Network Interconnect Unit (NIU) Demo (ISO 11783-4 Phase 5) ===");
    echo::info("Demonstrating all 4 NIU types: Repeater, Bridge, Router, Gateway");

    // ═════════════════════════════════════════════════════════════════════════
    // SCENARIO 1: Repeater NIU - Simple Extension
    // ═════════════════════════════════════════════════════════════════════════

    echo::info("\n--- SCENARIO 1: Repeater NIU - Cable Extension ---");

    IsoNet tractor_net1;
    IsoNet implement_net1;

    RepeaterNIU repeater(NIUConfig{}.set_name("CableRepeater"));
    repeater.attach_tractor(&tractor_net1);
    repeater.attach_implement(&implement_net1);
    repeater.initialize();

    echo::info("Repeater initialized: ", repeater.state() == NIUState::Active ? "ACTIVE" : "INACTIVE");
    echo::info("Filter mode: ", repeater.filter_mode() == NIUFilterMode::PassAll ? "PassAll" : "BlockAll");
    echo::info("Purpose: Extends physical network, forwards all frames bidirectionally");
    echo::info("Use case: Cable length extension, segment isolation");

    // Add optional filtering
    repeater.block_pgn(0xFECA); // Block proprietary PGN
    echo::info("Added filter: Block PGN 0xFECA (proprietary messages)");

    // Simulate address uniqueness check
    if (repeater.check_address_unique(0x20)) {
        echo::info("✓ Address 0x20 is unique across both segments");
    }

    echo::info("Repeater stats: ", repeater.forwarded(), " forwarded, ", repeater.blocked(), " blocked");

    // ═════════════════════════════════════════════════════════════════════════
    // SCENARIO 2: Bridge NIU - Smart Filtering
    // ═════════════════════════════════════════════════════════════════════════

    echo::info("\n--- SCENARIO 2: Bridge NIU - Learning Bridge ---");

    IsoNet tractor_net2;
    IsoNet implement_net2;

    BridgeNIU bridge(NIUConfig{}.set_name("SmartBridge").mode(NIUFilterMode::PassAll));
    bridge.attach_tractor(&tractor_net2);
    bridge.attach_implement(&implement_net2);
    bridge.initialize();

    echo::info("Bridge initialized with PassAll mode");
    echo::info("Purpose: Intelligent filtering with learning bridge behavior");

    // Learning bridge: Track which devices are on which side
    echo::info("\nLearning device locations:");
    bridge.learn_address(0x20, Side::Tractor);
    bridge.learn_address(0x21, Side::Tractor);
    bridge.learn_address(0x30, Side::Implement);
    bridge.learn_address(0x31, Side::Implement);

    echo::info("Learned: ECU at 0x20 is on tractor side");
    echo::info("Learned: ECU at 0x21 is on tractor side");
    echo::info("Learned: Implement at 0x30 is on implement side");
    echo::info("Learned: Implement at 0x31 is on implement side");

    // Lookup learned addresses
    auto side1 = bridge.lookup_address(0x20);
    if (side1.has_value()) {
        echo::info("✓ Address 0x20 found on: ",
                   side1.value() == Side::Tractor ? "TRACTOR" : "IMPLEMENT");
    }

    auto side2 = bridge.lookup_address(0x30);
    if (side2.has_value()) {
        echo::info("✓ Address 0x30 found on: ",
                   side2.value() == Side::Tractor ? "TRACTOR" : "IMPLEMENT");
    }

    // Add PGN-based filtering
    bridge.block_pgn(0xFECA, true); // Block proprietary bidirectionally
    bridge.monitor_pgn(0xFEF5, true); // Monitor ambient conditions
    echo::info("\nFilters configured:");
    echo::info("  - Block PGN 0xFECA (proprietary)");
    echo::info("  - Monitor PGN 0xFEF5 (ambient conditions)");

    echo::info("Bridge can now optimize forwarding based on learned addresses");

    // ═════════════════════════════════════════════════════════════════════════
    // SCENARIO 3: Router NIU - Address Translation
    // ═════════════════════════════════════════════════════════════════════════

    echo::info("\n--- SCENARIO 3: Router NIU - Address Translation ---");

    IsoNet tractor_net3;
    IsoNet implement_net3;

    RouterNIU router(NIUConfig{}.set_name("AddressRouter"));
    router.attach_tractor(&tractor_net3);
    router.attach_implement(&implement_net3);
    router.initialize();

    echo::info("Router initialized with address translation");
    echo::info("Purpose: Maintain separate address spaces per segment");

    // Set up address translations
    echo::info("\nConfiguring address translations:");

    Name tractor_ecu = Name::build()
                           .set_identity_number(100)
                           .set_manufacturer_code(1234)
                           .set_function_code(0); // Engine
    router.add_translation(tractor_ecu, 0x20, 0x40);
    echo::info("Engine (NAME ", tractor_ecu.raw, "): 0x20 on tractor → 0x40 on implement");

    Name implement_controller = Name::build()
                                    .set_identity_number(200)
                                    .set_manufacturer_code(5678)
                                    .set_function_code(30); // Task Controller
    router.add_translation(implement_controller, 0x50, 0x21);
    echo::info("Task Controller (NAME ", implement_controller.raw, "): 0x50 on tractor → 0x21 on implement");

    Name vt_terminal = Name::build()
                           .set_identity_number(300)
                           .set_manufacturer_code(9012)
                           .set_function_code(26); // VT
    router.add_translation(vt_terminal, 0x30, 0x30);
    echo::info("VT Terminal (NAME ", vt_terminal.raw, "): 0x30 on tractor → 0x30 on implement");

    // Verify translations
    const auto &db = router.translation_db();
    echo::info("\nTranslation database has ", db.entries().size(), " entries");

    // Test address translation
    Address translated = db.translate(0x20, Side::Tractor);
    if (translated != NULL_ADDRESS) {
        echo::info("✓ Tractor address 0x20 translates to implement address 0x", translated);
    }

    Address reverse = db.translate(0x40, Side::Implement);
    if (reverse != NULL_ADDRESS) {
        echo::info("✓ Implement address 0x40 translates back to tractor address 0x", reverse);
    }

    // Check address availability
    if (!db.is_address_available(0x20, Side::Tractor)) {
        echo::info("✓ Address 0x20 is allocated on tractor segment");
    }

    if (db.is_address_available(0x99, Side::Tractor)) {
        echo::info("✓ Address 0x99 is available on tractor segment");
    }

    echo::info("\nRouter benefits:");
    echo::info("  - Resolves address conflicts between segments");
    echo::info("  - Enables security boundaries");
    echo::info("  - Controlled routing between networks");

    // ═════════════════════════════════════════════════════════════════════════
    // SCENARIO 4: Gateway NIU - Message Repackaging
    // ═════════════════════════════════════════════════════════════════════════

    echo::info("\n--- SCENARIO 4: Gateway NIU - Protocol Translation ---");

    IsoNet tractor_net4;
    IsoNet implement_net4;

    GatewayNIU gateway(NIUConfig{}.set_name("ProtocolGateway"));
    gateway.attach_tractor(&tractor_net4);
    gateway.attach_implement(&implement_net4);
    gateway.initialize();

    echo::info("Gateway initialized with message repackaging");
    echo::info("Purpose: Router + message transformation");

    // Set up address translations (Gateway extends Router)
    Name sensor = Name::build()
                      .set_identity_number(500)
                      .set_manufacturer_code(1111)
                      .set_function_code(50);
    gateway.add_translation(sensor, 0x25, 0x45);
    echo::info("\nSensor (NAME ", sensor.raw, "): 0x25 on tractor → 0x45 on implement");

    // Register message transformations
    echo::info("\nRegistering message transforms:");

    // Transform 1: Convert imperial to metric (tractor → implement)
    gateway.register_tractor_transform(0xFEF5, [](const Message &msg) -> dp::Optional<Message> {
        echo::info("  Transform: Converting imperial units to metric (tractor → implement)");
        Message transformed = msg;
        // Example: Convert Fahrenheit to Celsius in ambient conditions
        if (transformed.data.size() >= 4) {
            u16 temp_f = static_cast<u16>(transformed.data[3]) | (static_cast<u16>(transformed.data[4]) << 8);
            u16 temp_c = static_cast<u16>((temp_f - 32) * 5.0 / 9.0);
            transformed.data[3] = static_cast<u8>(temp_c & 0xFF);
            transformed.data[4] = static_cast<u8>((temp_c >> 8) & 0xFF);
        }
        return transformed;
    });
    echo::info("  Registered: PGN 0xFEF5 (Ambient Conditions) - Imperial → Metric");

    // Transform 2: Convert metric to imperial (implement → tractor)
    gateway.register_implement_transform(0xFEF5, [](const Message &msg) -> dp::Optional<Message> {
        echo::info("  Transform: Converting metric units to imperial (implement → tractor)");
        Message transformed = msg;
        // Example: Convert Celsius to Fahrenheit
        if (transformed.data.size() >= 4) {
            u16 temp_c = static_cast<u16>(transformed.data[3]) | (static_cast<u16>(transformed.data[4]) << 8);
            u16 temp_f = static_cast<u16>(temp_c * 9.0 / 5.0 + 32);
            transformed.data[3] = static_cast<u8>(temp_f & 0xFF);
            transformed.data[4] = static_cast<u8>((temp_f >> 8) & 0xFF);
        }
        return transformed;
    });
    echo::info("  Registered: PGN 0xFEF5 (Ambient Conditions) - Metric → Imperial");

    // Transform 3: Block proprietary messages
    gateway.register_tractor_transform(0xFECA, [](const Message &msg) -> dp::Optional<Message> {
        echo::info("  Transform: Blocking proprietary message");
        return dp::nullopt; // Block message
    });
    echo::info("  Registered: PGN 0xFECA (Proprietary) - BLOCK");

    echo::info("\nGateway capabilities:");
    echo::info("  - All Router features (address translation)");
    echo::info("  - Message structure transformation");
    echo::info("  - Protocol bridging (CAN 2.0B ↔ CAN FD)");
    echo::info("  - Data unit conversion (imperial ↔ metric)");
    echo::info("  - Custom message processing");

    // ═════════════════════════════════════════════════════════════════════════
    // SCENARIO 5: Filter Database Features
    // ═════════════════════════════════════════════════════════════════════════

    echo::info("\n--- SCENARIO 5: Filter Database Features ---");

    NIU advanced_niu(NIUConfig{}.set_name("AdvancedNIU").mode(NIUFilterMode::BlockAll));

    echo::info("Created NIU with BlockAll mode (whitelist approach)");
    echo::info("Purpose: Demonstrate advanced filtering capabilities");

    // NAME-based filtering
    echo::info("\nNAME-based filtering:");
    Name trusted_device = Name::build().set_identity_number(999).set_manufacturer_code(7777);
    advanced_niu.allow_name(trusted_device);
    echo::info("  Allowed NAME ", trusted_device.raw, " (any PGN)");

    Name suspicious_device = Name::build().set_identity_number(666).set_manufacturer_code(6666);
    advanced_niu.block_name(suspicious_device, 0xFECA);
    echo::info("  Blocked NAME ", suspicious_device.raw, " for PGN 0xFECA");

    // Rate-limited filtering
    echo::info("\nRate-limited filtering:");
    advanced_niu.allow_pgn_rate_limited(0xFEF5, 100, true); // Max 10 Hz
    echo::info("  Allowed PGN 0xFEF5 with rate limit: 100ms (max 10 Hz)");

    advanced_niu.allow_pgn_rate_limited(0xFEF6, 1000, true); // Max 1 Hz
    echo::info("  Allowed PGN 0xFEF6 with rate limit: 1000ms (max 1 Hz)");

    // Persistence
    echo::info("\nFilter persistence:");
    const auto &filters = advanced_niu.filters();
    echo::info("  Total filters: ", filters.size());

    u32 persistent_count = 0;
    for (const auto &filter : filters) {
        if (filter.persistent) {
            persistent_count++;
        }
    }
    echo::info("  Persistent filters: ", persistent_count);
    echo::info("  Note: Persistent filters survive NIU reset");

    // ═════════════════════════════════════════════════════════════════════════
    // SCENARIO 6: NIU Network Messages (PGN 0xED00)
    // ═════════════════════════════════════════════════════════════════════════

    echo::info("\n--- SCENARIO 6: NIU Network Messages (PGN 0xED00) ---");

    echo::info("NIU Network Message functions:");

    // Add filter entry
    NIUNetworkMsg add_msg;
    add_msg.function = NIUFunction::AddFilterEntry;
    add_msg.port_number = 1;
    add_msg.filter_pgn = 0xFEF5;
    auto add_encoded = add_msg.encode();
    echo::info("  AddFilterEntry: ", add_encoded.size(), " bytes");

    // Set filter mode
    NIUNetworkMsg mode_msg;
    mode_msg.function = NIUFunction::SetFilterMode;
    mode_msg.port_number = 0;
    mode_msg.filter_mode = NIUFilterMode::BlockAll;
    auto mode_encoded = mode_msg.encode();
    echo::info("  SetFilterMode: ", mode_encoded.size(), " bytes");

    // Port statistics
    NIUNetworkMsg stats_msg;
    stats_msg.function = NIUFunction::PortStatsResponse;
    stats_msg.port_number = 1;
    stats_msg.msgs_forwarded = 12345;
    stats_msg.msgs_blocked = 678;
    auto stats_encoded = stats_msg.encode();
    echo::info("  PortStatsResponse: ", stats_encoded.size(), " bytes");
    echo::info("    Forwarded: ", stats_msg.msgs_forwarded);
    echo::info("    Blocked: ", stats_msg.msgs_blocked);

    // ═════════════════════════════════════════════════════════════════════════
    // Summary
    // ═════════════════════════════════════════════════════════════════════════

    echo::info("\n=== NIU Demo Complete ===");
    echo::info("Demonstrated:");
    echo::info("  ✓ Repeater NIU - Simple extension with filtering");
    echo::info("  ✓ Bridge NIU - Learning bridge with MAC table");
    echo::info("  ✓ Router NIU - Address translation between segments");
    echo::info("  ✓ Gateway NIU - Message repackaging and protocol translation");
    echo::info("  ✓ Filter Database - NAME, rate limiting, persistence");
    echo::info("  ✓ NIU Network Messages - Remote configuration (PGN 0xED00)");

    echo::info("\nNIU Selection Guide:");
    echo::info("  - Repeater: Simple extension, no address conflicts");
    echo::info("  - Bridge: Smart filtering, reduce cross-segment traffic");
    echo::info("  - Router: Address conflicts between segments");
    echo::info("  - Gateway: Router + protocol/format translation");

    return 0;
}
