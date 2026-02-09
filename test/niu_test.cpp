#include <doctest/doctest.h>
#include <agrobus/net/constants.hpp>
#include <agrobus/net/frame.hpp>
#include <agrobus/net/niu.hpp>

using namespace agrobus::net;

// Helper to create a test frame with a given PGN
static Frame make_frame(PGN pgn, Address src = 0x28, Address dst = BROADCAST_ADDRESS) {
    u8 payload[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    return Frame::from_message(Priority::Default, pgn, src, dst, payload, 8);
}

TEST_CASE("NIU construction and configuration") {
    SUBCASE("default configuration") {
        NIU niu;
        CHECK(niu.state() == NIUState::Inactive);
        CHECK(niu.forwarded() == 0);
        CHECK(niu.blocked() == 0);
    }

    SUBCASE("custom configuration") {
        NIUConfig cfg;
        cfg.set_name("TestNIU").global_default(false).specific_default(false);
        NIU niu(cfg);
        CHECK(niu.state() == NIUState::Inactive);
    }
}

TEST_CASE("NIU attach networks") {
    NIU niu;
    IsoNet tractor_net;
    IsoNet implement_net;

    SUBCASE("attach null tractor network fails") {
        auto result = niu.attach_tractor(nullptr);
        CHECK_FALSE(result.is_ok());
    }

    SUBCASE("attach null implement network fails") {
        auto result = niu.attach_implement(nullptr);
        CHECK_FALSE(result.is_ok());
    }

    SUBCASE("attach valid networks succeeds") {
        auto r1 = niu.attach_tractor(&tractor_net);
        auto r2 = niu.attach_implement(&implement_net);
        CHECK(r1.is_ok());
        CHECK(r2.is_ok());
    }
}

TEST_CASE("NIU start/stop") {
    NIU niu;
    IsoNet tractor_net;
    IsoNet implement_net;

    SUBCASE("start without networks fails") {
        auto result = niu.start();
        CHECK_FALSE(result.is_ok());
        CHECK(niu.state() == NIUState::Error);
    }

    SUBCASE("start with only tractor fails") {
        niu.attach_tractor(&tractor_net);
        auto result = niu.start();
        CHECK_FALSE(result.is_ok());
        CHECK(niu.state() == NIUState::Error);
    }

    SUBCASE("start with both networks succeeds") {
        niu.attach_tractor(&tractor_net);
        niu.attach_implement(&implement_net);
        auto result = niu.start();
        CHECK(result.is_ok());
        CHECK(niu.state() == NIUState::Active);
    }

    SUBCASE("stop transitions to inactive") {
        niu.attach_tractor(&tractor_net);
        niu.attach_implement(&implement_net);
        niu.start();
        niu.stop();
        CHECK(niu.state() == NIUState::Inactive);
    }
}

TEST_CASE("NIU filter management") {
    NIU niu;

    SUBCASE("add_filter via fluent API") {
        niu.allow_pgn(PGN_VEHICLE_SPEED)
           .block_pgn(PGN_DM1)
           .monitor_pgn(PGN_GROUND_SPEED);
        // No crash, filters added
    }

    SUBCASE("clear_filters") {
        niu.allow_pgn(PGN_VEHICLE_SPEED);
        niu.clear_filters();
        // After clearing, defaults should apply
    }
}

TEST_CASE("NIU forwarding with default allow policy") {
    NIU niu;
    IsoNet tractor_net;
    IsoNet implement_net;
    niu.attach_tractor(&tractor_net);
    niu.attach_implement(&implement_net);
    niu.start();

    SUBCASE("broadcast frame from tractor is forwarded by default") {
        Frame f = make_frame(PGN_VEHICLE_SPEED);
        niu.process_tractor_frame(f);
        CHECK(niu.forwarded() == 1);
        CHECK(niu.blocked() == 0);
    }

    SUBCASE("broadcast frame from implement is forwarded by default") {
        Frame f = make_frame(PGN_GROUND_SPEED);
        niu.process_implement_frame(f);
        CHECK(niu.forwarded() == 1);
        CHECK(niu.blocked() == 0);
    }

    SUBCASE("destination-specific frame is forwarded by default") {
        Frame f = make_frame(PGN_REQUEST, 0x28, 0x10);
        niu.process_tractor_frame(f);
        CHECK(niu.forwarded() == 1);
    }

    SUBCASE("multiple frames increment counter") {
        Frame f = make_frame(PGN_VEHICLE_SPEED);
        niu.process_tractor_frame(f);
        niu.process_tractor_frame(f);
        niu.process_implement_frame(f);
        CHECK(niu.forwarded() == 3);
    }
}

TEST_CASE("NIU forwarding with block-all defaults") {
    NIUConfig cfg;
    cfg.global_default(false).specific_default(false);
    NIU niu(cfg);
    IsoNet tractor_net;
    IsoNet implement_net;
    niu.attach_tractor(&tractor_net);
    niu.attach_implement(&implement_net);
    niu.start();

    SUBCASE("broadcast frame blocked when default is block") {
        Frame f = make_frame(PGN_VEHICLE_SPEED);
        niu.process_tractor_frame(f);
        CHECK(niu.forwarded() == 0);
        CHECK(niu.blocked() == 1);
    }

    SUBCASE("explicit allow overrides block-all default") {
        niu.allow_pgn(PGN_VEHICLE_SPEED);
        Frame f = make_frame(PGN_VEHICLE_SPEED);
        niu.process_tractor_frame(f);
        CHECK(niu.forwarded() == 1);
        CHECK(niu.blocked() == 0);
    }
}

TEST_CASE("NIU block filter") {
    NIU niu;
    IsoNet tractor_net;
    IsoNet implement_net;
    niu.attach_tractor(&tractor_net);
    niu.attach_implement(&implement_net);
    niu.start();

    niu.block_pgn(PGN_DM1);

    SUBCASE("blocked PGN is not forwarded") {
        Frame f = make_frame(PGN_DM1);
        niu.process_tractor_frame(f);
        CHECK(niu.forwarded() == 0);
        CHECK(niu.blocked() == 1);
    }

    SUBCASE("non-blocked PGN is still forwarded") {
        Frame f = make_frame(PGN_VEHICLE_SPEED);
        niu.process_tractor_frame(f);
        CHECK(niu.forwarded() == 1);
        CHECK(niu.blocked() == 0);
    }

    SUBCASE("bidirectional block applies to both sides") {
        Frame f = make_frame(PGN_DM1);
        niu.process_tractor_frame(f);
        niu.process_implement_frame(f);
        CHECK(niu.blocked() == 2);
    }
}

TEST_CASE("NIU monitor filter") {
    NIU niu;
    IsoNet tractor_net;
    IsoNet implement_net;
    niu.attach_tractor(&tractor_net);
    niu.attach_implement(&implement_net);
    niu.start();

    niu.monitor_pgn(PGN_GROUND_SPEED);

    SUBCASE("monitored PGN is forwarded and triggers monitor event") {
        u32 monitor_count = 0;
        niu.on_monitored.subscribe([&](Frame, Side) { ++monitor_count; });

        Frame f = make_frame(PGN_GROUND_SPEED);
        niu.process_tractor_frame(f);
        CHECK(niu.forwarded() == 1);
        CHECK(niu.blocked() == 0);
        CHECK(monitor_count == 1);
    }
}

TEST_CASE("NIU events") {
    NIU niu;
    IsoNet tractor_net;
    IsoNet implement_net;
    niu.attach_tractor(&tractor_net);
    niu.attach_implement(&implement_net);
    niu.start();

    niu.block_pgn(PGN_DM1);

    SUBCASE("on_forwarded event fires for allowed frames") {
        u32 event_count = 0;
        Side last_side = Side::Implement;
        niu.on_forwarded.subscribe([&](Frame, Side s) {
            ++event_count;
            last_side = s;
        });

        Frame f = make_frame(PGN_VEHICLE_SPEED);
        niu.process_tractor_frame(f);
        CHECK(event_count == 1);
        CHECK(last_side == Side::Tractor);

        niu.process_implement_frame(f);
        CHECK(event_count == 2);
        CHECK(last_side == Side::Implement);
    }

    SUBCASE("on_blocked event fires for blocked frames") {
        u32 event_count = 0;
        niu.on_blocked.subscribe([&](Frame, Side) { ++event_count; });

        Frame f = make_frame(PGN_DM1);
        niu.process_tractor_frame(f);
        CHECK(event_count == 1);
    }
}

TEST_CASE("NIU does not process when inactive") {
    NIU niu;
    IsoNet tractor_net;
    IsoNet implement_net;
    niu.attach_tractor(&tractor_net);
    niu.attach_implement(&implement_net);
    // Do NOT start

    Frame f = make_frame(PGN_VEHICLE_SPEED);
    niu.process_tractor_frame(f);
    CHECK(niu.forwarded() == 0);
    CHECK(niu.blocked() == 0);
}

TEST_CASE("NIU unidirectional filter") {
    NIU niu;
    IsoNet tractor_net;
    IsoNet implement_net;
    niu.attach_tractor(&tractor_net);
    niu.attach_implement(&implement_net);
    niu.start();

    // Block PGN_DM1 only from tractor side (bidirectional = false)
    niu.block_pgn(PGN_DM1, false);

    SUBCASE("unidirectional block applies from tractor") {
        Frame f = make_frame(PGN_DM1);
        niu.process_tractor_frame(f);
        CHECK(niu.blocked() == 1);
    }

    SUBCASE("unidirectional block does not apply from implement (uses default allow)") {
        Frame f = make_frame(PGN_DM1);
        niu.process_implement_frame(f);
        // Default is allow, and rule is not bidirectional so it does not match implement side
        CHECK(niu.forwarded() == 1);
        CHECK(niu.blocked() == 0);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Filter Database Extended Tests (Phase 5)
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("FilterRule NAME-based filtering") {
    Name test_name = Name::build().set_identity_number(100).set_manufacturer_code(1234);

    SUBCASE("create filter with source NAME") {
        FilterRule rule(PGN_VEHICLE_SPEED, ForwardPolicy::Allow, true);
        rule.source_name = test_name;
        CHECK(rule.source_name.has_value());
        CHECK(rule.source_name.value() == test_name);
    }

    SUBCASE("create filter with destination NAME") {
        FilterRule rule(PGN_VEHICLE_SPEED, ForwardPolicy::Block, true);
        rule.destination_name = test_name;
        CHECK(rule.destination_name.has_value());
        CHECK(rule.destination_name.value() == test_name);
    }
}

TEST_CASE("FilterRule rate limiting") {
    SUBCASE("rate limit configuration") {
        FilterRule rule(PGN_VEHICLE_SPEED, ForwardPolicy::Allow, true);
        rule.max_frequency_ms = 100;
        CHECK(rule.max_frequency_ms == 100);
        CHECK(rule.last_forward_time == 0);
    }
}

TEST_CASE("FilterRule persistence") {
    SUBCASE("encode and decode basic filter") {
        FilterRule rule(PGN_VEHICLE_SPEED, ForwardPolicy::Monitor, true);
        rule.persistent = true;
        rule.max_frequency_ms = 500;

        auto encoded = rule.encode();
        CHECK(encoded.size() >= 22);

        auto decoded = FilterRule::decode(encoded);
        CHECK(decoded.is_ok());
        CHECK(decoded.value().pgn == PGN_VEHICLE_SPEED);
        CHECK(decoded.value().policy == ForwardPolicy::Monitor);
        CHECK(decoded.value().bidirectional == true);
        CHECK(decoded.value().persistent == true);
        CHECK(decoded.value().max_frequency_ms == 500);
    }

    SUBCASE("encode and decode with NAME") {
        Name test_name = Name::build().set_identity_number(999).set_manufacturer_code(5678);
        FilterRule rule(PGN_GROUND_SPEED, ForwardPolicy::Block, false);
        rule.source_name = test_name;
        rule.persistent = true;

        auto encoded = rule.encode();
        auto decoded = FilterRule::decode(encoded);
        CHECK(decoded.is_ok());
        CHECK(decoded.value().source_name.has_value());
        CHECK(decoded.value().source_name.value() == test_name);
        CHECK(decoded.value().bidirectional == false);
    }
}

TEST_CASE("NIU filter mode") {
    SUBCASE("default filter mode is PassAll") {
        NIU niu;
        CHECK(niu.filter_mode() == NIUFilterMode::PassAll);
    }

    SUBCASE("set filter mode to BlockAll") {
        NIU niu;
        niu.set_filter_mode(NIUFilterMode::BlockAll);
        CHECK(niu.filter_mode() == NIUFilterMode::BlockAll);
    }
}

TEST_CASE("NIU NAME-based filtering API") {
    Name name1 = Name::build().set_identity_number(100).set_manufacturer_code(1234);
    Name name2 = Name::build().set_identity_number(200).set_manufacturer_code(5678);

    NIU niu;

    SUBCASE("allow NAME") {
        niu.allow_name(name1);
        const auto &filters = niu.filters();
        CHECK(filters.size() == 1);
        CHECK(filters[0].source_name.has_value());
        CHECK(filters[0].source_name.value() == name1);
    }

    SUBCASE("block NAME") {
        niu.block_name(name2, PGN_VEHICLE_SPEED);
        const auto &filters = niu.filters();
        CHECK(filters.size() == 1);
        CHECK(filters[0].source_name.has_value());
        CHECK(filters[0].pgn == PGN_VEHICLE_SPEED);
    }
}

TEST_CASE("NIU rate-limited filtering API") {
    NIU niu;
    niu.allow_pgn_rate_limited(PGN_VEHICLE_SPEED, 100, true);

    const auto &filters = niu.filters();
    CHECK(filters.size() == 1);
    CHECK(filters[0].max_frequency_ms == 100);
}

// ═════════════════════════════════════════════════════════════════════════════
// Repeater NIU Tests (Phase 5)
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("RepeaterNIU initialization") {
    IsoNet tractor_net;
    IsoNet implement_net;

    RepeaterNIU repeater(NIUConfig{}.set_name("TestRepeater"));
    repeater.attach_tractor(&tractor_net);
    repeater.attach_implement(&implement_net);

    SUBCASE("initialize starts NIU") {
        auto res = repeater.initialize();
        CHECK(res.is_ok());
        CHECK(repeater.state() == NIUState::Active);
    }

    SUBCASE("repeater forwards all by default") {
        CHECK(repeater.filter_mode() == NIUFilterMode::PassAll);
    }
}

TEST_CASE("RepeaterNIU address uniqueness") {
    IsoNet tractor_net;
    IsoNet implement_net;

    RepeaterNIU repeater;
    repeater.attach_tractor(&tractor_net);
    repeater.attach_implement(&implement_net);
    repeater.initialize();

    SUBCASE("check address uniqueness") {
        CHECK(repeater.check_address_unique(0x20));
        CHECK(repeater.check_address_unique(0x21));
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Bridge NIU Tests (Phase 5)
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("BridgeNIU initialization") {
    IsoNet tractor_net;
    IsoNet implement_net;

    BridgeNIU bridge(NIUConfig{}.set_name("TestBridge").mode(NIUFilterMode::BlockAll));
    bridge.attach_tractor(&tractor_net);
    bridge.attach_implement(&implement_net);

    SUBCASE("initialize with BlockAll mode") {
        auto res = bridge.initialize();
        CHECK(res.is_ok());
        CHECK(bridge.state() == NIUState::Active);
        CHECK(bridge.filter_mode() == NIUFilterMode::BlockAll);
    }
}

TEST_CASE("BridgeNIU learning bridge") {
    IsoNet tractor_net;
    IsoNet implement_net;

    BridgeNIU bridge;
    bridge.attach_tractor(&tractor_net);
    bridge.attach_implement(&implement_net);
    bridge.initialize();

    SUBCASE("learn and lookup addresses") {
        bridge.learn_address(0x20, Side::Tractor);
        bridge.learn_address(0x21, Side::Implement);
        bridge.learn_address(0x22, Side::Tractor);

        auto side1 = bridge.lookup_address(0x20);
        CHECK(side1.has_value());
        CHECK(side1.value() == Side::Tractor);

        auto side2 = bridge.lookup_address(0x21);
        CHECK(side2.has_value());
        CHECK(side2.value() == Side::Implement);

        auto side3 = bridge.lookup_address(0x22);
        CHECK(side3.has_value());
        CHECK(side3.value() == Side::Tractor);
    }

    SUBCASE("lookup unknown address") {
        auto side = bridge.lookup_address(0x99);
        CHECK_FALSE(side.has_value());
    }

    SUBCASE("update learned address") {
        bridge.learn_address(0x20, Side::Tractor);
        bridge.learn_address(0x20, Side::Implement); // Update

        auto side = bridge.lookup_address(0x20);
        CHECK(side.has_value());
        CHECK(side.value() == Side::Implement);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Address Translation Database Tests (Phase 5)
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("AddressTranslationDB add and lookup") {
    AddressTranslationDB db;
    Name name1 = Name::build().set_identity_number(100).set_manufacturer_code(1234);
    Name name2 = Name::build().set_identity_number(200).set_manufacturer_code(5678);

    SUBCASE("add single translation") {
        db.add(name1, 0x20, 0x30);
        auto trans = db.lookup_by_name(name1);
        CHECK(trans.has_value());
        CHECK(trans.value().tractor_address == 0x20);
        CHECK(trans.value().implement_address == 0x30);
    }

    SUBCASE("add multiple translations") {
        db.add(name1, 0x20, 0x30);
        db.add(name2, 0x21, 0x31);
        CHECK(db.entries().size() == 2);

        auto trans1 = db.lookup_by_name(name1);
        CHECK(trans1.has_value());
        CHECK(trans1.value().tractor_address == 0x20);

        auto trans2 = db.lookup_by_name(name2);
        CHECK(trans2.has_value());
        CHECK(trans2.value().tractor_address == 0x21);
    }

    SUBCASE("update existing translation") {
        db.add(name1, 0x20, 0x30);
        db.add(name1, 0x21, 0x31); // Update

        auto trans = db.lookup_by_name(name1);
        CHECK(trans.has_value());
        CHECK(trans.value().tractor_address == 0x21);
        CHECK(trans.value().implement_address == 0x31);
    }
}

TEST_CASE("AddressTranslationDB translate") {
    AddressTranslationDB db;
    Name name1 = Name::build().set_identity_number(100).set_manufacturer_code(1234);
    db.add(name1, 0x20, 0x30);

    SUBCASE("translate tractor to implement") {
        Address impl_addr = db.translate(0x20, Side::Tractor);
        CHECK(impl_addr == 0x30);
    }

    SUBCASE("translate implement to tractor") {
        Address trac_addr = db.translate(0x30, Side::Implement);
        CHECK(trac_addr == 0x20);
    }

    SUBCASE("translate unknown address") {
        Address unknown = db.translate(0x99, Side::Tractor);
        CHECK(unknown == INVALID_ADDRESS);
    }
}

TEST_CASE("AddressTranslationDB lookup by address") {
    AddressTranslationDB db;
    Name name1 = Name::build().set_identity_number(100).set_manufacturer_code(1234);
    db.add(name1, 0x20, 0x30);

    SUBCASE("lookup tractor address") {
        auto trans = db.lookup_by_address(0x20, Side::Tractor);
        CHECK(trans.has_value());
        CHECK(trans.value().name == name1);
    }

    SUBCASE("lookup implement address") {
        auto trans = db.lookup_by_address(0x30, Side::Implement);
        CHECK(trans.has_value());
        CHECK(trans.value().name == name1);
    }

    SUBCASE("lookup unknown address") {
        auto trans = db.lookup_by_address(0x99, Side::Tractor);
        CHECK_FALSE(trans.has_value());
    }
}

TEST_CASE("AddressTranslationDB address availability") {
    AddressTranslationDB db;
    Name name1 = Name::build().set_identity_number(100).set_manufacturer_code(1234);
    db.add(name1, 0x20, 0x30);

    SUBCASE("allocated addresses are not available") {
        CHECK_FALSE(db.is_address_available(0x20, Side::Tractor));
        CHECK_FALSE(db.is_address_available(0x30, Side::Implement));
    }

    SUBCASE("unallocated addresses are available") {
        CHECK(db.is_address_available(0x99, Side::Tractor));
        CHECK(db.is_address_available(0x99, Side::Implement));
    }
}

TEST_CASE("AddressTranslationDB remove") {
    AddressTranslationDB db;
    Name name1 = Name::build().set_identity_number(100).set_manufacturer_code(1234);
    db.add(name1, 0x20, 0x30);

    SUBCASE("remove existing translation") {
        CHECK(db.lookup_by_name(name1).has_value());
        db.remove(name1);
        CHECK_FALSE(db.lookup_by_name(name1).has_value());
    }

    SUBCASE("remove non-existent translation") {
        Name name2 = Name::build().set_identity_number(999).set_manufacturer_code(9999);
        db.remove(name2); // Should not crash
    }
}

TEST_CASE("AddressTranslationDB clear") {
    AddressTranslationDB db;
    Name name1 = Name::build().set_identity_number(100).set_manufacturer_code(1234);
    Name name2 = Name::build().set_identity_number(200).set_manufacturer_code(5678);

    db.add(name1, 0x20, 0x30);
    db.add(name2, 0x21, 0x31);
    CHECK(db.entries().size() == 2);

    db.clear();
    CHECK(db.entries().size() == 0);
}

// ═════════════════════════════════════════════════════════════════════════════
// Router NIU Tests (Phase 5)
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("RouterNIU initialization") {
    IsoNet tractor_net;
    IsoNet implement_net;

    RouterNIU router(NIUConfig{}.set_name("TestRouter"));
    router.attach_tractor(&tractor_net);
    router.attach_implement(&implement_net);

    SUBCASE("initialize router") {
        auto res = router.initialize();
        CHECK(res.is_ok());
        CHECK(router.state() == NIUState::Active);
    }
}

TEST_CASE("RouterNIU translation management") {
    RouterNIU router;
    Name name1 = Name::build().set_identity_number(100).set_manufacturer_code(1234);
    Name name2 = Name::build().set_identity_number(200).set_manufacturer_code(5678);

    SUBCASE("add translation") {
        router.add_translation(name1, 0x20, 0x30);
        const auto &db = router.translation_db();
        auto trans = db.lookup_by_name(name1);
        CHECK(trans.has_value());
        CHECK(trans.value().tractor_address == 0x20);
        CHECK(trans.value().implement_address == 0x30);
    }

    SUBCASE("add multiple translations") {
        router.add_translation(name1, 0x20, 0x30);
        router.add_translation(name2, 0x21, 0x31);
        CHECK(router.translation_db().entries().size() == 2);
    }

    SUBCASE("remove translation") {
        router.add_translation(name1, 0x20, 0x30);
        CHECK(router.translation_db().lookup_by_name(name1).has_value());

        router.remove_translation(name1);
        CHECK_FALSE(router.translation_db().lookup_by_name(name1).has_value());
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Gateway NIU Tests (Phase 5)
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("GatewayNIU initialization") {
    IsoNet tractor_net;
    IsoNet implement_net;

    GatewayNIU gateway(NIUConfig{}.set_name("TestGateway"));
    gateway.attach_tractor(&tractor_net);
    gateway.attach_implement(&implement_net);

    SUBCASE("initialize gateway") {
        auto res = gateway.initialize();
        CHECK(res.is_ok());
        CHECK(gateway.state() == NIUState::Active);
    }
}

TEST_CASE("GatewayNIU message transforms") {
    GatewayNIU gateway;

    SUBCASE("register tractor transform") {
        gateway.register_tractor_transform(PGN_VEHICLE_SPEED, [](const Message &msg) -> dp::Optional<Message> {
            Message transformed = msg;
            if (!transformed.data.empty()) {
                transformed.data[0] = transformed.data[0] * 2;
            }
            return transformed;
        });
        // Transform registered successfully
    }

    SUBCASE("register implement transform") {
        gateway.register_implement_transform(PGN_VEHICLE_SPEED, [](const Message &msg) -> dp::Optional<Message> {
            Message transformed = msg;
            if (!transformed.data.empty()) {
                transformed.data[0] = transformed.data[0] / 2;
            }
            return transformed;
        });
        // Transform registered successfully
    }

    SUBCASE("register blocking transform") {
        gateway.register_tractor_transform(PGN_VEHICLE_SPEED, [](const Message &msg) -> dp::Optional<Message> {
            return dp::nullopt; // Block message
        });
        // Transform registered successfully
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// NIU Network Message Tests (PGN 0xED00)
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("NIUNetworkMsg encode and decode") {
    SUBCASE("AddFilterEntry") {
        NIUNetworkMsg msg;
        msg.function = NIUFunction::AddFilterEntry;
        msg.port_number = 1;
        msg.filter_pgn = PGN_VEHICLE_SPEED;

        auto encoded = msg.encode();
        CHECK(encoded.size() == 8);
        CHECK(encoded[0] == static_cast<u8>(NIUFunction::AddFilterEntry));
        CHECK(encoded[1] == 1);

        auto decoded = NIUNetworkMsg::decode(encoded);
        CHECK(decoded.function == NIUFunction::AddFilterEntry);
        CHECK(decoded.port_number == 1);
        CHECK(decoded.filter_pgn == PGN_VEHICLE_SPEED);
    }

    SUBCASE("SetFilterMode") {
        NIUNetworkMsg msg;
        msg.function = NIUFunction::SetFilterMode;
        msg.port_number = 0;
        msg.filter_mode = NIUFilterMode::BlockAll;

        auto encoded = msg.encode();
        CHECK(encoded[0] == static_cast<u8>(NIUFunction::SetFilterMode));
        CHECK(encoded[2] == static_cast<u8>(NIUFilterMode::BlockAll));

        auto decoded = NIUNetworkMsg::decode(encoded);
        CHECK(decoded.function == NIUFunction::SetFilterMode);
        CHECK(decoded.filter_mode == NIUFilterMode::BlockAll);
    }

    SUBCASE("PortStatsResponse") {
        NIUNetworkMsg msg;
        msg.function = NIUFunction::PortStatsResponse;
        msg.port_number = 1;
        msg.msgs_forwarded = 1234;
        msg.msgs_blocked = 567;

        auto encoded = msg.encode();
        CHECK(encoded[0] == static_cast<u8>(NIUFunction::PortStatsResponse));
        CHECK(encoded[1] == 1);

        auto decoded = NIUNetworkMsg::decode(encoded);
        CHECK(decoded.function == NIUFunction::PortStatsResponse);
        CHECK(decoded.port_number == 1);
        CHECK(decoded.msgs_forwarded == 1234);
        CHECK(decoded.msgs_blocked == 567);
    }
}
