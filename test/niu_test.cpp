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
