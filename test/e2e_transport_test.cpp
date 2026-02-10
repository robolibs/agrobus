#include <doctest/doctest.h>
#include <agrobus.hpp>
#include <wirebit/can/socketcan_link.hpp>
#include <wirebit/can/can_endpoint.hpp>

using namespace agrobus::net;
using namespace agrobus::j1939;
using namespace agrobus::isobus;
using namespace agrobus::nmea;
using namespace agrobus::isobus::vt;
using namespace agrobus::isobus::tc;
using namespace agrobus::isobus::sc;
using namespace agrobus::isobus::implement;
using namespace agrobus::isobus::fs;

// ─── Multi-node test harness ─────────────────────────────────────────────────
// Two IsoNets connected via shared vcan interface (kernel loopback)
struct DualNodeSetup {
    std::shared_ptr<wirebit::SocketCanLink> link_a;
    std::shared_ptr<wirebit::SocketCanLink> link_b;
    wirebit::CanEndpoint ep_a;
    wirebit::CanEndpoint ep_b;
    IsoNet nm_a;
    IsoNet nm_b;
    InternalCF *cf_a = nullptr;
    InternalCF *cf_b = nullptr;

    DualNodeSetup()
        : link_a(std::make_shared<wirebit::SocketCanLink>(
              wirebit::SocketCanLink::create({.interface_name = "vcan_e2e_tp", .create_if_missing = true, .destroy_on_close = true}).value())),
          link_b(std::make_shared<wirebit::SocketCanLink>(
              wirebit::SocketCanLink::attach("vcan_e2e_tp").value())),
          ep_a(link_a, wirebit::CanConfig{}, 1),
          ep_b(link_b, wirebit::CanConfig{}, 2) {
        nm_a.set_endpoint(0, &ep_a);
        nm_b.set_endpoint(0, &ep_b);
        cf_a = nm_a.create_internal(Name::build().set_identity_number(1).set_manufacturer_code(100), 0, 0x28).value();
        cf_b = nm_b.create_internal(Name::build().set_identity_number(2).set_manufacturer_code(200), 0, 0x30).value();
    }

    // Run a full round: update both nodes (vcan kernel handles frame routing)
    void tick(u32 elapsed_ms = 10) {
        nm_a.update(elapsed_ms);
        nm_b.update(elapsed_ms);
        nm_a.update(0);
        nm_b.update(0);
    }
};

TEST_CASE("E2E: TP BAM broadcast between two nodes") {
    DualNodeSetup setup;

    // Node B listens for DM1
    bool received = false;
    dp::Vector<u8> received_data;
    setup.nm_b.register_pgn_callback(PGN_DM1, [&](const Message &msg) {
        received = true;
        received_data = msg.data;
    });

    // Node A sends DM1 with multi-DTC payload (>8 bytes, triggers TP BAM)
    dp::Vector<u8> payload(22); // 2 lamp bytes + 5 DTCs * 4 bytes
    for (usize i = 0; i < payload.size(); ++i) {
        payload[i] = static_cast<u8>(i + 1);
    }

    auto result = setup.nm_a.send(PGN_DM1, payload, setup.cf_a);
    CHECK(result.is_ok());

    // Run several ticks to let BAM complete
    for (i32 i = 0; i < 20 && !received; ++i) {
        setup.tick(50);
    }

    CHECK(received);
    CHECK(received_data.size() == 22);
    for (usize i = 0; i < 22; ++i) {
        CHECK(received_data[i] == static_cast<u8>(i + 1));
    }
}

TEST_CASE("E2E: TP connection mode (RTS/CTS) between two nodes") {
    DualNodeSetup setup;

    // Node B listens for ECU_TO_VT (PDU1, destination-specific)
    bool received = false;
    dp::Vector<u8> received_data;
    setup.nm_b.register_pgn_callback(PGN_ECU_TO_VT, [&](const Message &msg) {
        received = true;
        received_data = msg.data;
    });

    // Node A sends to Node B specifically
    ControlFunction dest;
    dest.address = 0x30;

    dp::Vector<u8> payload(100);
    for (usize i = 0; i < payload.size(); ++i) {
        payload[i] = static_cast<u8>((i * 3) & 0xFF);
    }

    auto result = setup.nm_a.send(PGN_ECU_TO_VT, payload, setup.cf_a, &dest);
    CHECK(result.is_ok());

    // Run ticks to complete RTS/CTS/DT/EOMA handshake
    for (i32 i = 0; i < 50 && !received; ++i) {
        setup.tick(50);
    }

    CHECK(received);
    CHECK(received_data.size() == 100);
    for (usize i = 0; i < 100; ++i) {
        CHECK(received_data[i] == static_cast<u8>((i * 3) & 0xFF));
    }
}

TEST_CASE("E2E: Large TP transfer (near max 1785 bytes)") {
    DualNodeSetup setup;

    bool received = false;
    usize received_size = 0;
    setup.nm_b.register_pgn_callback(PGN_DM1, [&](const Message &msg) {
        received = true;
        received_size = msg.data.size();
    });

    // Send near-max TP payload via broadcast
    dp::Vector<u8> payload(1000);
    for (usize i = 0; i < payload.size(); ++i) {
        payload[i] = static_cast<u8>(i & 0xFF);
    }

    auto result = setup.nm_a.send(PGN_DM1, payload, setup.cf_a);
    CHECK(result.is_ok());

    for (i32 i = 0; i < 200 && !received; ++i) {
        setup.tick(50);
    }

    CHECK(received);
    CHECK(received_size == 1000);
}

TEST_CASE("E2E: PGN request with ACK/NACK") {
    DualNodeSetup setup;

    // Node B registers a responder for SOFTWARE_ID
    PGNRequestProtocol pgn_req_b(setup.nm_b, setup.cf_b);
    pgn_req_b.initialize();
    pgn_req_b.register_responder(PGN_SOFTWARE_ID, []() -> dp::Vector<u8> {
        dp::Vector<u8> data(8, 0);
        data[0] = 'T';
        data[1] = 'E';
        data[2] = 'S';
        data[3] = 'T';
        return data;
    });

    // Node A sends a request for SOFTWARE_ID
    PGNRequestProtocol pgn_req_a(setup.nm_a, setup.cf_a);
    pgn_req_a.initialize();

    bool got_response = false;
    setup.nm_a.register_pgn_callback(PGN_SOFTWARE_ID, [&](const Message &msg) {
        got_response = true;
        CHECK(msg.data[0] == 'T');
    });

    auto result = pgn_req_a.request(PGN_SOFTWARE_ID);
    CHECK(result.is_ok());

    for (i32 i = 0; i < 10 && !got_response; ++i) {
        setup.tick(10);
    }

    CHECK(got_response);
}

TEST_CASE("E2E: PGN request NACK for unknown PGN") {
    DualNodeSetup setup;

    PGNRequestProtocol pgn_req_b(setup.nm_b, setup.cf_b);
    pgn_req_b.initialize();

    PGNRequestProtocol pgn_req_a(setup.nm_a, setup.cf_a);
    pgn_req_a.initialize();

    bool got_nack = false;
    pgn_req_a.on_ack_received.subscribe([&](const Acknowledgment &ack) {
        if (ack.control == AckControl::NegativeAck) {
            got_nack = true;
        }
    });

    // Request a PGN that B doesn't know about (destination-specific)
    ControlFunction dest;
    dest.address = 0x30;
    auto result = pgn_req_a.request(0xFE99, 0x30);
    CHECK(result.is_ok());

    for (i32 i = 0; i < 10 && !got_nack; ++i) {
        setup.tick(10);
    }

    CHECK(got_nack);
}

TEST_CASE("E2E: Diagnostics multi-DTC via transport") {
    DualNodeSetup setup;

    // Node A sets up diagnostics
    DiagnosticProtocol diag(setup.nm_a, setup.cf_a);
    diag.initialize();

    // Add 3 DTCs (2 lamp bytes + 3*4 = 14 bytes total, needs TP)
    diag.set_active({.spn = 100, .fmi = FMI::AboveNormal});
    diag.set_active({.spn = 200, .fmi = FMI::VoltageHigh});
    diag.set_active({.spn = 300, .fmi = FMI::CurrentLow});

    // Node B listens for DM1
    bool received = false;
    dp::Vector<DTC> received_dtcs;
    setup.nm_b.register_pgn_callback(PGN_DM1, [&](const Message &msg) {
        received = true;
        // Decode DTCs from multi-packet message
        if (msg.data.size() >= 6) {
            for (usize i = 2; i + 3 < msg.data.size(); i += 4) {
                DTC dtc = DTC::decode(&msg.data[i]);
                if (dtc.spn != 0 || static_cast<u8>(dtc.fmi) != 0) {
                    received_dtcs.push_back(dtc);
                }
            }
        }
    });

    auto result = diag.send_dm1();
    CHECK(result.is_ok());

    for (i32 i = 0; i < 20 && !received; ++i) {
        setup.tick(50);
    }

    CHECK(received);
    CHECK(received_dtcs.size() == 3);
    CHECK(received_dtcs[0].spn == 100);
    CHECK(received_dtcs[1].spn == 200);
    CHECK(received_dtcs[2].spn == 300);
}

TEST_CASE("E2E: Timer and Scheduler utilities") {
    // Timer basic functionality
    Timer timer(100);
    timer.start();

    CHECK_FALSE(timer.expired());
    CHECK_FALSE(timer.update(50));
    CHECK_FALSE(timer.update(40));
    CHECK(timer.update(20)); // 50+40+20 = 110 >= 100
    CHECK(timer.elapsed() == 10); // auto-reset: 110-100=10

    // One-shot timeout
    Timeout timeout(200);
    timeout.start();
    CHECK(timeout.active());
    CHECK_FALSE(timeout.update(100));
    CHECK(timeout.update(150)); // 250 >= 200
    CHECK_FALSE(timeout.active());

    // Scheduler
    Scheduler sched;
    i32 call_count = 0;
    sched.add("test_task", 50, [&]() -> bool {
        call_count++;
        return true;
    });

    sched.update(30);
    CHECK(call_count == 0);
    sched.update(30); // 60 >= 50
    CHECK(call_count == 1);
    sched.update(55); // 55 >= 50
    CHECK(call_count == 2);

    // ProcessingFlags
    ProcessingFlags flags;
    i32 flag_a_count = 0;
    i32 flag_b_count = 0;
    flags.register_flag(0, [&]() { flag_a_count++; });
    flags.register_flag(1, [&]() { flag_b_count++; });

    flags.set(0);
    flags.set(1);
    CHECK(flags.any_pending());
    flags.process();
    CHECK(flag_a_count == 1);
    CHECK(flag_b_count == 1);
    CHECK_FALSE(flags.any_pending());
}

TEST_CASE("E2E: Event unsubscribe during dispatch") {
    Event<i32> event;

    i32 call_count_a = 0;
    i32 call_count_b = 0;
    ListenerToken token_b = INVALID_TOKEN;

    event.subscribe([&](i32) { call_count_a++; });
    token_b = event.subscribe([&](i32) {
        call_count_b++;
        // Unsubscribe self during dispatch
        event.unsubscribe(token_b);
    });
    event.subscribe([&](i32) { call_count_a++; }); // Third listener

    event.emit(42);
    CHECK(call_count_a == 2);
    CHECK(call_count_b == 1);
    CHECK(event.count() == 2); // B removed after dispatch

    event.emit(43);
    CHECK(call_count_a == 4);
    CHECK(call_count_b == 1); // B no longer called
}
