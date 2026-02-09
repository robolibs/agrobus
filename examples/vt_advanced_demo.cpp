#include <agrobus.hpp>
#include <echo/echo.hpp>

using namespace agrobus::net;
using namespace agrobus::isobus::vt;

int main() {
    echo::info("=== VT Advanced Features Demo (ISO 11783-6 Phase 3) ===");
    echo::info("Demonstrating Window Mask, Key Groups, Alarm Priority Stack, Macros, and Language Negotiation");

    // Create network manager
    IsoNet net;

    // ═════════════════════════════════════════════════════════════════════════
    // SCENARIO 1: Window Mask and Key Group Objects
    // ═════════════════════════════════════════════════════════════════════════

    echo::info("\n--- SCENARIO 1: Window Mask and Key Group Objects ---");

    // Create a Window Mask object
    WindowMaskBody window;
    window.window_type = 0; // Freeform window
    window.background_color = 0;
    window.options = 0x01; // Available for adjustment
    window.name = 1000;
    window.window_title = 1001;
    window.window_icon = 1002;

    VTObject window_obj = create_window_mask(100, window);
    echo::info("Created Window Mask object ID=100");
    echo::info("  Type: Freeform, BG Color: 0, Options: 0x01");
    echo::info("  Name: 1000, Title: 1001, Icon: 1002");

    // Encode and decode round-trip
    auto encoded = window.encode();
    auto decoded = WindowMaskBody::decode(encoded);
    if (decoded.is_ok()) {
        auto &w = decoded.value();
        echo::info("Round-trip successful:");
        echo::info("  Decoded type: ", static_cast<u32>(w.window_type));
        echo::info("  Decoded color: ", static_cast<u32>(w.background_color));
    } else {
        echo::error("Round-trip failed");
    }

    // Create a Key Group object
    KeyGroupBody key_group;
    key_group.options = 0x01; // Available
    key_group.name = 2000;
    key_group.key_group_icon = 2001;

    VTObject kg_obj = create_key_group(200, key_group);
    echo::info("\nCreated Key Group object ID=200");
    echo::info("  Options: 0x01, Name: 2000, Icon: 2001");

    // Create Key objects for the group
    KeyBody key1;
    key1.background_color = 1;
    key1.key_code = 0x01;
    key1.options = 0x00;

    KeyBody key2;
    key2.background_color = 2;
    key2.key_code = 0x02;
    key2.options = 0x01; // Latchable

    VTObject key1_obj = create_key(201, key1);
    VTObject key2_obj = create_key(202, key2);

    kg_obj.add_child(201).add_child(202);
    echo::info("Added 2 Key objects to Key Group");

    // ═════════════════════════════════════════════════════════════════════════
    // SCENARIO 2: Alarm Priority Stack
    // ═════════════════════════════════════════════════════════════════════════

    echo::info("\n--- SCENARIO 2: Alarm Priority Stack ---");

    // Create Alarm Mask objects with different priorities
    AlarmMaskBody critical_alarm;
    critical_alarm.background_color = 1;
    critical_alarm.soft_key_mask = 0xFFFF;
    critical_alarm.priority = 0; // Critical
    critical_alarm.acoustic_signal = 2;
    critical_alarm.options = 0;

    AlarmMaskBody warning_alarm;
    warning_alarm.background_color = 3;
    warning_alarm.soft_key_mask = 0xFFFF;
    warning_alarm.priority = 1; // Warning
    warning_alarm.acoustic_signal = 1;
    warning_alarm.options = 0;

    AlarmMaskBody info_alarm;
    info_alarm.background_color = 5;
    info_alarm.soft_key_mask = 0xFFFF;
    info_alarm.priority = 2; // Information
    info_alarm.acoustic_signal = 0;
    info_alarm.options = 0;

    VTObject alarm1 = create_alarm_mask(300, critical_alarm);
    VTObject alarm2 = create_alarm_mask(301, warning_alarm);
    VTObject alarm3 = create_alarm_mask(302, info_alarm);

    echo::info("Created 3 Alarm Mask objects:");
    echo::info("  Alarm 300: Priority 0 (Critical)");
    echo::info("  Alarm 301: Priority 1 (Warning)");
    echo::info("  Alarm 302: Priority 2 (Information)");

    // Simulate alarm priority stack
    VTClientStateTracker tracker(net);
    tracker.initialize();

    // Register alarm priorities
    tracker.register_alarm_priority(300, AlarmPriority::Critical);
    tracker.register_alarm_priority(301, AlarmPriority::Warning);
    tracker.register_alarm_priority(302, AlarmPriority::Information);

    // Subscribe to alarm events
    tracker.on_alarm_activated.subscribe([](ObjectID id, AlarmPriority priority) {
        echo::info("Alarm activated: ID=", id, " Priority=", static_cast<u32>(priority));
    });

    tracker.on_alarm_deactivated.subscribe([](ObjectID id) {
        echo::info("Alarm deactivated: ID=", id);
    });

    // Activate alarms out of priority order
    echo::info("\nActivating alarms (out of order):");
    tracker.activate_alarm(302, 1000); // Info
    tracker.activate_alarm(301, 2000); // Warning
    tracker.activate_alarm(300, 3000); // Critical

    // Display active alarm stack
    echo::info("\nActive alarm stack (highest priority first):");
    const auto &alarms = tracker.active_alarms();
    for (usize i = 0; i < alarms.size(); ++i) {
        echo::info("  [", i, "] ID=", alarms[i].alarm_mask_id, " Priority=", static_cast<u32>(alarms[i].priority));
    }

    // Get highest priority alarm
    auto highest = tracker.highest_priority_alarm();
    if (highest.has_value()) {
        echo::info("\nHighest priority alarm: ID=", highest.value().alarm_mask_id);
    }

    // Acknowledge alarms one by one
    echo::info("\nAcknowledging alarms:");
    tracker.acknowledge_alarm();
    echo::info("Active alarms after acknowledge: ", tracker.active_alarms().size());

    tracker.acknowledge_alarm();
    echo::info("Active alarms after acknowledge: ", tracker.active_alarms().size());

    tracker.acknowledge_alarm();
    echo::info("Active alarms after acknowledge: ", tracker.active_alarms().size());

    // ═════════════════════════════════════════════════════════════════════════
    // SCENARIO 3: Macro Objects
    // ═════════════════════════════════════════════════════════════════════════

    echo::info("\n--- SCENARIO 3: Macro Objects ---");

    // Create macro commands
    MacroBody macro;

    MacroCommand cmd1;
    cmd1.command_type = 0xA0; // Hide/Show
    cmd1.parameters = {0x00, 0x01, 0x01, 0x00, 0x00}; // Hide object 0x0100

    MacroCommand cmd2;
    cmd2.command_type = 0xA8; // Change Numeric Value
    cmd2.parameters = {0x00, 0x02, 0xFF, 0x64, 0x00, 0x00, 0x00}; // Set object 0x0200 to 100

    MacroCommand cmd3;
    cmd3.command_type = 0xA5; // Change Background Colour
    cmd3.parameters = {0x00, 0x03, 0x05}; // Set object 0x0300 to color 5

    macro.commands.push_back(cmd1);
    macro.commands.push_back(cmd2);
    macro.commands.push_back(cmd3);

    VTObject macro_obj = create_macro(400, macro);
    echo::info("Created Macro object ID=400 with 3 commands:");
    echo::info("  [0] Hide/Show (0xA0)");
    echo::info("  [1] Change Numeric Value (0xA8)");
    echo::info("  [2] Change Background Colour (0xA5)");

    // Encode and decode macro
    auto macro_encoded = macro.encode();
    echo::info("Macro encoded size: ", macro_encoded.size(), " bytes");

    auto macro_decoded = MacroBody::decode(macro_encoded);
    if (macro_decoded.is_ok()) {
        auto &m = macro_decoded.value();
        echo::info("Macro decoded successfully: ", m.commands.size(), " commands");
        for (usize i = 0; i < m.commands.size(); ++i) {
            echo::info("  [", i, "] Command type: 0x", m.commands[i].command_type);
        }
    } else {
        echo::error("Macro decode failed");
    }

    // ═════════════════════════════════════════════════════════════════════════
    // SCENARIO 4: Language Negotiation
    // ═════════════════════════════════════════════════════════════════════════

    echo::info("\n--- SCENARIO 4: Language Negotiation ---");

    // Create language codes
    LanguageCode en = LanguageCode::from_string("en");
    LanguageCode de = LanguageCode::from_string("de");
    LanguageCode fr = LanguageCode::from_string("fr");

    echo::info("Created language codes:");
    echo::info("  English: ", en.to_string());
    echo::info("  German: ", de.to_string());
    echo::info("  French: ", fr.to_string());

    // Test equality
    echo::info("\nLanguage comparison:");
    echo::info("  en == en: ", (en == en ? "true" : "false"));
    echo::info("  en == de: ", (en == de ? "true" : "false"));
    echo::info("  de != fr: ", (de != fr ? "true" : "false"));

    // Simulate language change scenario
    echo::info("\nSimulating language change scenario:");
    echo::info("1. Client starts with language: en");
    echo::info("2. VT reports language change to: de");
    echo::info("3. Client detects mismatch");
    echo::info("4. Client transitions to ReloadPool state");
    echo::info("5. Pool reloaded with German strings");
    echo::info("6. Client reconnects with correct language");

    // ═════════════════════════════════════════════════════════════════════════
    // SCENARIO 5: Complete Object Pool with Advanced Features
    // ═════════════════════════════════════════════════════════════════════════

    echo::info("\n--- SCENARIO 5: Complete Object Pool ---");

    ObjectPool pool;

    // Add all advanced objects to pool
    pool.add(window_obj);
    pool.add(kg_obj);
    pool.add(key1_obj);
    pool.add(key2_obj);
    pool.add(alarm1);
    pool.add(alarm2);
    pool.add(alarm3);
    pool.add(macro_obj);

    echo::info("Created object pool with ", pool.size(), " objects:");
    echo::info("  1x Window Mask (ID 100)");
    echo::info("  1x Key Group (ID 200) with 2 Keys (201, 202)");
    echo::info("  3x Alarm Masks (300-302) with priorities");
    echo::info("  1x Macro (ID 400) with 3 commands");

    // Serialize pool
    auto pool_data = pool.serialize();
    if (pool_data.is_ok()) {
        echo::info("\nPool serialization successful:");
        echo::info("  Total size: ", pool_data.value().size(), " bytes");
    } else {
        echo::error("Pool serialization failed");
    }

    echo::info("\n=== VT Advanced Features Demo Complete ===");
    echo::info("Demonstrated:");
    echo::info("  ✓ Window Mask object type");
    echo::info("  ✓ Key Group and Key objects");
    echo::info("  ✓ Alarm priority stack (Critical/Warning/Info)");
    echo::info("  ✓ Macro command encoding/decoding");
    echo::info("  ✓ Language negotiation (en/de/fr)");
    echo::info("  ✓ Object pool with advanced features");

    return 0;
}
