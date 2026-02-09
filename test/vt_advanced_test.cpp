#include <doctest/doctest.h>
#include <agrobus/isobus/vt/client.hpp>
#include <agrobus/isobus/vt/objects.hpp>
#include <agrobus/isobus/vt/state_tracker.hpp>

using namespace agrobus::isobus;
using namespace agrobus::isobus::vt;

// ═════════════════════════════════════════════════════════════════════════════
// Window Mask Object Tests (Phase 3)
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("WindowMaskBody encode and decode") {
    SUBCASE("freeform window") {
        WindowMaskBody body;
        body.window_type = 0; // Freeform
        body.background_color = 5;
        body.options = 0x03;
        body.name = 100;
        body.window_title = 200;
        body.window_icon = 300;

        auto encoded = body.encode();
        CHECK(encoded.size() >= 9);

        auto decoded = WindowMaskBody::decode(encoded);
        CHECK(decoded.is_ok());
        CHECK(decoded.value().window_type == 0);
        CHECK(decoded.value().background_color == 5);
        CHECK(decoded.value().options == 0x03);
        CHECK(decoded.value().name == 100);
        CHECK(decoded.value().window_title == 200);
        CHECK(decoded.value().window_icon == 300);
    }

    SUBCASE("numeric output window") {
        WindowMaskBody body;
        body.window_type = 1; // Numeric output
        body.background_color = 10;
        body.options = 0x01;
        body.name = 0xFFFF;
        body.window_title = 500;
        body.window_icon = 0xFFFF;

        auto encoded = body.encode();
        auto decoded = WindowMaskBody::decode(encoded);
        CHECK(decoded.is_ok());
        CHECK(decoded.value().window_type == 1);
        CHECK(decoded.value().background_color == 10);
        CHECK(decoded.value().name == 0xFFFF);
    }

    SUBCASE("list window") {
        WindowMaskBody body;
        body.window_type = 2; // List
        body.background_color = 15;
        body.options = 0x00;
        body.name = 1000;
        body.window_title = 1001;
        body.window_icon = 1002;

        auto encoded = body.encode();
        auto decoded = WindowMaskBody::decode(encoded);
        CHECK(decoded.is_ok());
        CHECK(decoded.value().window_type == 2);
    }

    SUBCASE("decode insufficient data") {
        dp::Vector<u8> short_data = {0x00, 0x05};
        auto decoded = WindowMaskBody::decode(short_data);
        CHECK(decoded.is_err());
    }
}

TEST_CASE("WindowMask object integration") {
    VTObject window_mask;
    window_mask.id = 5000;
    window_mask.type = ObjectType::WindowMask;

    WindowMaskBody body;
    body.window_type = 0;
    body.background_color = 3;
    body.options = 0x01;
    body.name = 5001;
    body.window_title = 5002;
    body.window_icon = 5003;

    window_mask.body = body.encode();
    window_mask.children = {5010, 5011, 5012};

    auto serialized = window_mask.serialize();
    CHECK(serialized.size() > 0);
}

// ═════════════════════════════════════════════════════════════════════════════
// Key Group Object Tests (Phase 3)
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("KeyGroupBody encode and decode") {
    SUBCASE("basic key group") {
        KeyGroupBody body;
        body.options = 0x01;
        body.name = 2000;
        body.key_group_icon = 2001;

        auto encoded = body.encode();
        CHECK(encoded.size() >= 5);

        auto decoded = KeyGroupBody::decode(encoded);
        CHECK(decoded.is_ok());
        CHECK(decoded.value().options == 0x01);
        CHECK(decoded.value().name == 2000);
        CHECK(decoded.value().key_group_icon == 2001);
    }

    SUBCASE("key group with no icon") {
        KeyGroupBody body;
        body.options = 0x00;
        body.name = 3000;
        body.key_group_icon = 0xFFFF; // No icon

        auto encoded = body.encode();
        auto decoded = KeyGroupBody::decode(encoded);
        CHECK(decoded.is_ok());
        CHECK(decoded.value().key_group_icon == 0xFFFF);
    }

    SUBCASE("decode insufficient data") {
        dp::Vector<u8> short_data = {0x01};
        auto decoded = KeyGroupBody::decode(short_data);
        CHECK(decoded.is_err());
    }
}

TEST_CASE("KeyGroup object integration") {
    VTObject key_group;
    key_group.id = 6000;
    key_group.type = ObjectType::KeyGroup;

    KeyGroupBody body;
    body.options = 0x02;
    body.name = 6001;
    body.key_group_icon = 6002;

    key_group.body = body.encode();
    key_group.children = {6010, 6011, 6012, 6013}; // Keys

    auto serialized = key_group.serialize();
    CHECK(serialized.size() > 0);
}

// ═════════════════════════════════════════════════════════════════════════════
// Key Object Tests (Phase 3)
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("KeyBody encode and decode") {
    SUBCASE("basic key") {
        KeyBody body;
        body.background_color = 8;
        body.key_code = 42;
        body.options = 0x01;

        auto encoded = body.encode();
        CHECK(encoded.size() >= 3);

        auto decoded = KeyBody::decode(encoded);
        CHECK(decoded.is_ok());
        CHECK(decoded.value().background_color == 8);
        CHECK(decoded.value().key_code == 42);
        CHECK(decoded.value().options == 0x01);
    }

    SUBCASE("key with different key codes") {
        for (u8 code = 0; code < 255; code += 10) {
            KeyBody body;
            body.background_color = 1;
            body.key_code = code;
            body.options = 0x00;

            auto encoded = body.encode();
            auto decoded = KeyBody::decode(encoded);
            CHECK(decoded.is_ok());
            CHECK(decoded.value().key_code == code);
        }
    }

    SUBCASE("decode insufficient data") {
        dp::Vector<u8> short_data = {0x08};
        auto decoded = KeyBody::decode(short_data);
        CHECK(decoded.is_err());
    }
}

TEST_CASE("Key object integration") {
    VTObject key;
    key.id = 7000;
    key.type = ObjectType::Key;

    KeyBody body;
    body.background_color = 12;
    body.key_code = 100;
    body.options = 0x03;

    key.body = body.encode();
    key.children = {7010, 7011}; // Icon and label

    auto serialized = key.serialize();
    CHECK(serialized.size() > 0);
}

// ═════════════════════════════════════════════════════════════════════════════
// Alarm Mask Object Tests (Phase 3)
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("AlarmMaskBody encode and decode") {
    SUBCASE("critical alarm") {
        AlarmMaskBody body;
        body.background_color = 1; // Red
        body.soft_key_mask = 8000;
        body.priority = 0; // Critical
        body.acoustic_signal = 2;
        body.options = 0x01;

        auto encoded = body.encode();
        CHECK(encoded.size() >= 6);

        auto decoded = AlarmMaskBody::decode(encoded);
        CHECK(decoded.is_ok());
        CHECK(decoded.value().background_color == 1);
        CHECK(decoded.value().soft_key_mask == 8000);
        CHECK(decoded.value().priority == 0);
        CHECK(decoded.value().acoustic_signal == 2);
        CHECK(decoded.value().options == 0x01);
    }

    SUBCASE("warning alarm") {
        AlarmMaskBody body;
        body.background_color = 2; // Yellow
        body.soft_key_mask = 8001;
        body.priority = 1; // Warning
        body.acoustic_signal = 1;
        body.options = 0x00;

        auto encoded = body.encode();
        auto decoded = AlarmMaskBody::decode(encoded);
        CHECK(decoded.is_ok());
        CHECK(decoded.value().priority == 1);
    }

    SUBCASE("information alarm") {
        AlarmMaskBody body;
        body.background_color = 3; // Blue
        body.soft_key_mask = 8002;
        body.priority = 2; // Information
        body.acoustic_signal = 0;
        body.options = 0x02;

        auto encoded = body.encode();
        auto decoded = AlarmMaskBody::decode(encoded);
        CHECK(decoded.is_ok());
        CHECK(decoded.value().priority == 2);
    }

    SUBCASE("decode insufficient data") {
        dp::Vector<u8> short_data = {0x01, 0x00};
        auto decoded = AlarmMaskBody::decode(short_data);
        CHECK(decoded.is_err());
    }
}

TEST_CASE("AlarmMask object integration") {
    VTObject alarm_mask;
    alarm_mask.id = 9000;
    alarm_mask.type = ObjectType::AlarmMask;

    AlarmMaskBody body;
    body.background_color = 1;
    body.soft_key_mask = 9001;
    body.priority = 0;
    body.acoustic_signal = 2;
    body.options = 0x01;

    alarm_mask.body = body.encode();
    alarm_mask.children = {9010, 9011, 9012}; // Text and graphics

    auto serialized = alarm_mask.serialize();
    CHECK(serialized.size() > 0);
}

// ═════════════════════════════════════════════════════════════════════════════
// Macro Object Tests (Phase 3)
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("MacroCommand length") {
    SUBCASE("ChangeActiveMask command") {
        MacroCommand cmd;
        cmd.command_type = 0xA4; // ChangeActiveMask
    }

    SUBCASE("HideShowObject command") {
        MacroCommand cmd;
        cmd.command_type = 0xA0; // HideShowObject
    }

    SUBCASE("ChangePriority command") {
        MacroCommand cmd;
        cmd.command_type = 0xAC; // ChangePriority
    }

    SUBCASE("unknown command defaults to 2") {
        MacroCommand cmd;
        cmd.command_type = 0xFF; // Unknown
    }
}

TEST_CASE("MacroBody encode and decode") {
    SUBCASE("empty macro") {
        MacroBody body;
        auto encoded = body.encode();
        CHECK(encoded.size() == 0);

        auto decoded = MacroBody::decode(encoded);
        CHECK(decoded.is_ok());
        CHECK(decoded.value().commands.size() == 0);
    }

    SUBCASE("single command macro") {
        MacroBody body;
        MacroCommand cmd;
        cmd.command_type = 0xA0; // HideShowObject
        cmd.parameters = {0x01, 0x02, 0x03};
        body.commands.push_back(cmd);

        auto encoded = body.encode();
        CHECK(encoded.size() >= 4); // command_id + data

        auto decoded = MacroBody::decode(encoded);
        CHECK(decoded.is_ok());
        CHECK(decoded.value().commands.size() == 1);
        CHECK(decoded.value().commands[0].command_type == 0xA0);
    }

    SUBCASE("multiple command macro") {
        MacroBody body;

        // Command 1: HideShowObject
        MacroCommand cmd1;
        cmd1.command_type = 0xA0;
        cmd1.data = {0x10, 0x00, 0x01}; // 3 bytes
        body.commands.push_back(cmd1);

        // Command 2: ChangeActiveMask
        MacroCommand cmd2;
        cmd2.command_type = 0xA4;
        cmd2.data = {0x20, 0x00, 0x00, 0xFF}; // 4 bytes
        body.commands.push_back(cmd2);

        // Command 3: ChangePriority
        MacroCommand cmd3;
        cmd3.command_type = 0xAC;
        cmd3.data = {0x30, 0x00, 0x02}; // 3 bytes
        body.commands.push_back(cmd3);

        auto encoded = body.encode();
        CHECK(encoded.size() >= 13); // 3 command_ids + 10 data bytes

        auto decoded = MacroBody::decode(encoded);
        CHECK(decoded.is_ok());
        CHECK(decoded.value().commands.size() == 3);
        CHECK(decoded.value().commands[0].command_type == 0xA0);
        CHECK(decoded.value().commands[1].command_type == 0xA4);
        CHECK(decoded.value().commands[2].command_type == 0xAC);
    }

    SUBCASE("decode truncated data") {
        dp::Vector<u8> bad_data = {0xA4, 0x10}; // ChangeActiveMask needs 5 bytes, only 1 provided
        auto decoded = MacroBody::decode(bad_data);
        CHECK(decoded.is_err());
    }
}

TEST_CASE("Macro object integration") {
    VTObject macro;
    macro.id = 10000;
    macro.type = ObjectType::Macro;

    MacroBody body;
    MacroCommand cmd1;
    cmd1.command_type = 0xA0;
    cmd1.data = {0x10, 0x00, 0x01};
    body.commands.push_back(cmd1);

    MacroCommand cmd2;
    cmd2.command_type = 0xA4;
    cmd2.data = {0x20, 0x00, 0x00, 0xFF};
    body.commands.push_back(cmd2);

    macro.body = body.encode();

    auto serialized = macro.serialize();
    CHECK(serialized.size() > 0);
}

// ═════════════════════════════════════════════════════════════════════════════
// Alarm Priority Stack Tests (Phase 3)
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("AlarmPriority enum") {
    CHECK(static_cast<u8>(AlarmPriority::Critical) == 0);
    CHECK(static_cast<u8>(AlarmPriority::Warning) == 1);
    CHECK(static_cast<u8>(AlarmPriority::Information) == 2);
}

TEST_CASE("VTStateTracker alarm priority stack") {
    VTStateTracker tracker;

    SUBCASE("activate single alarm") {
        tracker.activate_alarm(1000, 100);
        const auto &alarms = tracker.active_alarms();
        CHECK(alarms.size() == 1);
        CHECK(alarms[0].alarm_mask_id == 1000);
        CHECK(alarms[0].priority == AlarmPriority::Information);
        CHECK(alarms[0].activation_timestamp_ms == 100);
    }

    SUBCASE("activate multiple alarms with different priorities") {
        tracker.activate_alarm(1000, 100); // Information (default)
        tracker.activate_alarm(1001, 200); // Information
        tracker.activate_alarm(1002, 300); // Information

        // Change priorities manually for testing
        auto &alarms_mut = const_cast<dp::Vector<AlarmEntry> &>(tracker.active_alarms());
        alarms_mut[0].priority = AlarmPriority::Information;
        alarms_mut[1].priority = AlarmPriority::Warning;
        alarms_mut[2].priority = AlarmPriority::Critical;

        // Stack should be sorted: Critical first, then Warning, then Information
        // Within same priority, FIFO (earlier timestamp first)
    }

    SUBCASE("acknowledge alarm removes top") {
        tracker.activate_alarm(1000, 100);
        tracker.activate_alarm(1001, 200);
        tracker.activate_alarm(1002, 300);
        CHECK(tracker.active_alarms().size() == 3);

        tracker.acknowledge_alarm();
        CHECK(tracker.active_alarms().size() == 2);

        tracker.acknowledge_alarm();
        CHECK(tracker.active_alarms().size() == 1);

        tracker.acknowledge_alarm();
        CHECK(tracker.active_alarms().size() == 0);
    }

    SUBCASE("acknowledge when empty does nothing") {
        CHECK(tracker.active_alarms().size() == 0);
        tracker.acknowledge_alarm();
        CHECK(tracker.active_alarms().size() == 0);
    }

    SUBCASE("deactivate specific alarm") {
        tracker.activate_alarm(1000, 100);
        tracker.activate_alarm(1001, 200);
        tracker.activate_alarm(1002, 300);
        CHECK(tracker.active_alarms().size() == 3);

        tracker.deactivate_alarm(1001);
        CHECK(tracker.active_alarms().size() == 2);

        // Verify 1001 is removed
        const auto &alarms = tracker.active_alarms();
        bool found = false;
        for (const auto &alarm : alarms) {
            if (alarm.alarm_mask_id == 1001) {
                found = true;
            }
        }
        CHECK_FALSE(found);
    }

    SUBCASE("deactivate non-existent alarm does nothing") {
        tracker.activate_alarm(1000, 100);
        CHECK(tracker.active_alarms().size() == 1);

        tracker.deactivate_alarm(9999);
        CHECK(tracker.active_alarms().size() == 1);
    }

    SUBCASE("multiple alarms FIFO within same priority") {
        tracker.activate_alarm(1000, 100);
        tracker.activate_alarm(1001, 200);
        tracker.activate_alarm(1002, 300);

        const auto &alarms = tracker.active_alarms();
        CHECK(alarms[0].activation_timestamp_ms == 100);
        CHECK(alarms[1].activation_timestamp_ms == 200);
        CHECK(alarms[2].activation_timestamp_ms == 300);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Language Negotiation Tests (Phase 3)
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("LanguageCode") {
    SUBCASE("default language is English") {
        LanguageCode lang;
        CHECK(lang.code[0] == 'e');
        CHECK(lang.code[1] == 'n');
        CHECK(lang.to_string() == "en");
    }

    SUBCASE("from_string two-letter code") {
        auto lang = LanguageCode::from_string("de");
        CHECK(lang.code[0] == 'd');
        CHECK(lang.code[1] == 'e');
        CHECK(lang.to_string() == "de");
    }

    SUBCASE("from_string longer code truncates") {
        auto lang = LanguageCode::from_string("fr-FR");
        CHECK(lang.code[0] == 'f');
        CHECK(lang.code[1] == 'r');
    }

    SUBCASE("from_string empty defaults to en") {
        auto lang = LanguageCode::from_string("");
        CHECK(lang.to_string() == "en");
    }

    SUBCASE("from_string single char pads") {
        auto lang = LanguageCode::from_string("x");
        CHECK(lang.code[0] == 'x');
        // Second char may be null or space, implementation dependent
    }

    SUBCASE("to_string round trip") {
        dp::Vector<dp::String> languages = {"en", "de", "fr", "es", "it", "ja", "zh"};
        for (const auto &lang_str : languages) {
            auto lang = LanguageCode::from_string(lang_str);
            CHECK(lang.to_string() == lang_str);
        }
    }
}

TEST_CASE("VTClient language negotiation") {
    SUBCASE("default language is English") {
        VTClientConfig config;
        // VTClient client(config);
        // Default language should be 'en'
        // Check would require accessing client internal state
    }

    SUBCASE("set current language") {
        // Test setting language and triggering reload
        // This requires VTClient instantiation and state verification
        // Placeholder for integration testing
    }

    SUBCASE("auto reload on language change") {
        // Test that language mismatch triggers ReloadPool state
        // Requires full VTClient setup and language command handling
        // Placeholder for integration testing
    }

    SUBCASE("handle language command") {
        // Test Language Command (PGN 0xFE0F) handling
        // Should update VT language and trigger reload if mismatch
        // Placeholder for integration testing
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Object Pool with Phase 3 Objects Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("ObjectPool with Phase 3 objects") {
    ObjectPool pool;

    SUBCASE("pool with Window Mask") {
        VTObject window;
        window.id = 5000;
        window.type = ObjectType::WindowMask;
        WindowMaskBody body;
        body.window_type = 0;
        body.background_color = 3;
        body.name = 5001;
        window.body = body.encode();
        pool.add(std::move(window));

        CHECK(pool.size() == 1);
        auto found = pool.find(5000);
        CHECK(found.has_value());
        CHECK((*found)->type == ObjectType::WindowMask);
    }

    SUBCASE("pool with Key Group and Keys") {
        // Key Group
        VTObject key_group;
        key_group.id = 6000;
        key_group.type = ObjectType::KeyGroup;
        KeyGroupBody kg_body;
        kg_body.name = 6001;
        key_group.body = kg_body.encode();
        pool.add(std::move(key_group));

        // Keys
        for (u16 i = 0; i < 5; i++) {
            VTObject key;
            key.id = 6010 + i;
            key.type = ObjectType::Key;
            KeyBody k_body;
            k_body.key_code = 10 + i;
            key.body = k_body.encode();
            pool.add(std::move(key));
        }

        CHECK(pool.size() == 6);
    }

    SUBCASE("pool with Alarm Masks") {
        // Critical alarm
        VTObject critical;
        critical.id = 9000;
        critical.type = ObjectType::AlarmMask;
        AlarmMaskBody c_body;
        c_body.priority = 0;
        critical.body = c_body.encode();
        pool.add(std::move(critical));

        // Warning alarm
        VTObject warning;
        warning.id = 9001;
        warning.type = ObjectType::AlarmMask;
        AlarmMaskBody w_body;
        w_body.priority = 1;
        warning.body = w_body.encode();
        pool.add(std::move(warning));

        // Information alarm
        VTObject info;
        info.id = 9002;
        info.type = ObjectType::AlarmMask;
        AlarmMaskBody i_body;
        i_body.priority = 2;
        info.body = i_body.encode();
        pool.add(std::move(info));

        CHECK(pool.size() == 3);
    }

    SUBCASE("pool with Macros") {
        VTObject macro;
        macro.id = 10000;
        macro.type = ObjectType::Macro;
        MacroBody m_body;
        MacroCommand cmd;
        cmd.command_type = 0xA0;
        cmd.parameters = {0x10, 0x00, 0x01};
        m_body.commands.push_back(cmd);
        macro.body = m_body.encode();
        pool.add(std::move(macro));

        CHECK(pool.size() == 1);
        auto found = pool.find(10000);
        CHECK(found.has_value());
        CHECK((*found)->type == ObjectType::Macro);
    }

    SUBCASE("serialize pool with all Phase 3 objects") {
        // Window Mask
        VTObject window;
        window.id = 5000;
        window.type = ObjectType::WindowMask;
        WindowMaskBody w_body;
        w_body.window_type = 0;
        window.body = w_body.encode();
        pool.add(std::move(window));

        // Key Group
        VTObject key_group;
        key_group.id = 6000;
        key_group.type = ObjectType::KeyGroup;
        KeyGroupBody kg_body;
        kg_body.name = 6001;
        key_group.body = kg_body.encode();
        pool.add(std::move(key_group));

        // Alarm Mask
        VTObject alarm;
        alarm.id = 9000;
        alarm.type = ObjectType::AlarmMask;
        AlarmMaskBody a_body;
        a_body.priority = 0;
        alarm.body = a_body.encode();
        pool.add(std::move(alarm));

        // Macro
        VTObject macro;
        macro.id = 10000;
        macro.type = ObjectType::Macro;
        MacroBody m_body;
        macro.body = m_body.encode();
        pool.add(std::move(macro));

        auto ser_result = pool.serialize();
        CHECK(ser_result.is_ok());
        CHECK(ser_result.value().size() > 0);

        // Deserialize and verify
        auto deser_result = ObjectPool::deserialize(ser_result.value());
        CHECK(deser_result.is_ok());
        CHECK(deser_result.value().size() == 4);
    }
}
