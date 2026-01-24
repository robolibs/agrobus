#include <doctest/doctest.h>
#include <agrobus.hpp>
#include <wirebit/can/can_endpoint.hpp>
#include <wirebit/can/socketcan_link.hpp>
#include <cstring>

using namespace agrobus::net;
using namespace agrobus::j1939;
using namespace agrobus::isobus;
using namespace agrobus::nmea;
using namespace agrobus::isobus::vt;
using namespace agrobus::isobus::tc;
using namespace agrobus::isobus::sc;
using namespace agrobus::isobus::implement;
using namespace agrobus::isobus::fs;

// Helper: create a IsoNet with vcan and a claimed InternalCF
struct TestSetup {
    std::shared_ptr<wirebit::SocketCanLink> dut_link;
    wirebit::CanEndpoint dut_ep;
    std::shared_ptr<wirebit::SocketCanLink> harness_link;
    wirebit::CanEndpoint harness_ep;
    IsoNet nm;
    InternalCF *cf = nullptr;

    TestSetup()
        : dut_link(std::make_shared<wirebit::SocketCanLink>(
              wirebit::SocketCanLink::create({.interface_name = "vcan_nm_tp", .create_if_missing = true, .destroy_on_close = true}).value())),
          dut_ep(std::static_pointer_cast<wirebit::Link>(dut_link), wirebit::CanConfig{.bitrate = 250000}, 1),
          harness_link(std::make_shared<wirebit::SocketCanLink>(
              wirebit::SocketCanLink::attach("vcan_nm_tp").value())),
          harness_ep(std::static_pointer_cast<wirebit::Link>(harness_link), wirebit::CanConfig{.bitrate = 250000}, 2) {
        nm.set_endpoint(0, &dut_ep);
        cf = nm.create_internal(Name::build().set_identity_number(1).set_manufacturer_code(100), 0, 0x28).value();
    }

    void inject(const Frame &frame) {
        can_frame cf = wirebit::CanEndpoint::make_ext_frame(frame.id.raw, frame.data.data(), frame.length);
        harness_ep.send_can(cf);
    }

    dp::Vector<Frame> captured() {
        dp::Vector<Frame> frames;
        can_frame cf;
        while (harness_ep.recv_can(cf).is_ok()) {
            Frame f;
            f.id = Identifier(cf.can_id & CAN_EFF_MASK);
            f.length = cf.can_dlc > 8 ? 8 : cf.can_dlc;
            for (u8 i = 0; i < f.length; ++i)
                f.data[i] = cf.data[i];
            frames.push_back(f);
        }
        return frames;
    }

    void drain() {
        can_frame cf;
        while (harness_ep.recv_can(cf).is_ok()) {}
    }
};

TEST_CASE("IsoNet - send auto-selects transport based on size") {
    TestSetup setup;

    SUBCASE("single frame for <= 8 bytes") {
        dp::Vector<u8> data(8, 0xAA);
        auto result = setup.nm.send(PGN_HEARTBEAT, data, setup.cf);
        CHECK(result.is_ok());

        auto frames = setup.captured();
        CHECK(frames.size() == 1); // Single CAN frame
    }

    SUBCASE("TP for 9-1785 bytes (broadcast BAM)") {
        dp::Vector<u8> data(100, 0xBB);
        auto result = setup.nm.send(PGN_DM1, data, setup.cf); // Broadcast PGN
        CHECK(result.is_ok());

        auto frames = setup.captured();
        CHECK(frames.size() == 1); // BAM announcement only (data sent via update)
        // Verify it's a TP.CM BAM
        CHECK(frames[0].pgn() == PGN_TP_CM);
        CHECK(frames[0].data[0] == tp_cm::BAM);
    }

    SUBCASE("TP for 9-1785 bytes (connection mode RTS)") {
        // Create a partner to send to
        ControlFunction dest_cf;
        dest_cf.address = 0x30;

        dp::Vector<u8> data(100, 0xCC);
        auto result = setup.nm.send(PGN_VT_TO_ECU, data, setup.cf, &dest_cf);
        CHECK(result.is_ok());

        auto frames = setup.captured();
        CHECK(frames.size() == 1); // RTS
        CHECK(frames[0].pgn() == PGN_TP_CM);
        CHECK(frames[0].data[0] == tp_cm::RTS);
    }

    SUBCASE("ETP rejects broadcast for > 1785 bytes") {
        dp::Vector<u8> data(2000, 0xDD);
        auto result = setup.nm.send(PGN_DM1, data, setup.cf); // Broadcast
        CHECK_FALSE(result.is_ok()); // ETP doesn't support broadcast
    }

    SUBCASE("ETP for > 1785 bytes (connection mode)") {
        ControlFunction dest_cf;
        dest_cf.address = 0x30;

        dp::Vector<u8> data(2000, 0xEE);
        auto result = setup.nm.send(PGN_VT_TO_ECU, data, setup.cf, &dest_cf);
        CHECK(result.is_ok());

        auto frames = setup.captured();
        CHECK(frames.size() == 1); // ETP RTS
        CHECK(frames[0].pgn() == PGN_ETP_CM);
        CHECK(frames[0].data[0] == etp_cm::RTS);
    }
}

TEST_CASE("IsoNet - TP BAM receive via process_frame") {
    TestSetup setup;

    // Simulate receiving a BAM + data frames from external node
    PGN test_pgn = PGN_DM1;
    u8 remote_addr = 0x30;
    dp::Vector<u8> original(14);
    for (usize i = 0; i < original.size(); ++i) {
        original[i] = static_cast<u8>(i + 1);
    }

    // Track received messages
    bool received = false;
    dp::Vector<u8> received_data;
    PGN received_pgn = 0;
    setup.nm.on_message.subscribe([&](const Message &msg) {
        if (msg.pgn == test_pgn) {
            received = true;
            received_data = msg.data;
            received_pgn = msg.pgn;
        }
    });

    // Also test PGN callback
    bool pgn_cb_called = false;
    setup.nm.register_pgn_callback(test_pgn, [&](const Message &msg) { pgn_cb_called = true; });

    // Inject BAM announcement
    Frame bam;
    bam.id = Identifier::encode(Priority::Lowest, PGN_TP_CM, remote_addr, BROADCAST_ADDRESS);
    bam.data[0] = tp_cm::BAM;
    bam.data[1] = 14; // total bytes low
    bam.data[2] = 0;  // total bytes high
    bam.data[3] = 2;  // total packets
    bam.data[4] = 0xFF;
    bam.data[5] = static_cast<u8>(test_pgn & 0xFF);
    bam.data[6] = static_cast<u8>((test_pgn >> 8) & 0xFF);
    bam.data[7] = static_cast<u8>((test_pgn >> 16) & 0xFF);
    bam.length = 8;
    setup.inject(bam);

    // Inject data frame 1 (seq=1, 7 bytes)
    Frame dt1;
    dt1.id = Identifier::encode(Priority::Lowest, PGN_TP_DT, remote_addr, BROADCAST_ADDRESS);
    dt1.data[0] = 1; // sequence 1
    for (u8 i = 0; i < 7; ++i) dt1.data[i + 1] = original[i];
    dt1.length = 8;
    setup.inject(dt1);

    // Inject data frame 2 (seq=2, remaining 7 bytes)
    Frame dt2;
    dt2.id = Identifier::encode(Priority::Lowest, PGN_TP_DT, remote_addr, BROADCAST_ADDRESS);
    dt2.data[0] = 2; // sequence 2
    for (u8 i = 0; i < 7; ++i) dt2.data[i + 1] = (7 + i < 14) ? original[7 + i] : 0xFF;
    dt2.length = 8;
    setup.inject(dt2);

    // Process all injected frames
    setup.nm.update(0);

    CHECK(received);
    CHECK(pgn_cb_called);
    CHECK(received_pgn == test_pgn);
    CHECK(received_data.size() == 14);
    for (usize i = 0; i < 14; ++i) {
        CHECK(received_data[i] == static_cast<u8>(i + 1));
    }
}

TEST_CASE("IsoNet - TP connection mode RTS/CTS receive") {
    TestSetup setup;

    PGN test_pgn = PGN_ECU_TO_VT;
    u8 remote_addr = 0x30;
    dp::Vector<u8> original(14);
    for (usize i = 0; i < original.size(); ++i) {
        original[i] = static_cast<u8>(i + 0x10);
    }

    bool received = false;
    dp::Vector<u8> received_data;
    setup.nm.on_message.subscribe([&](const Message &msg) {
        if (msg.pgn == test_pgn) {
            received = true;
            received_data = msg.data;
        }
    });

    // Inject RTS from remote
    Frame rts;
    rts.id = Identifier::encode(Priority::Lowest, PGN_TP_CM, remote_addr, 0x28);
    rts.data[0] = tp_cm::RTS;
    rts.data[1] = 14; // total bytes low
    rts.data[2] = 0;  // total bytes high
    rts.data[3] = 2;  // total packets
    rts.data[4] = 16; // max per CTS
    rts.data[5] = static_cast<u8>(test_pgn & 0xFF);
    rts.data[6] = static_cast<u8>((test_pgn >> 8) & 0xFF);
    rts.data[7] = static_cast<u8>((test_pgn >> 16) & 0xFF);
    rts.length = 8;
    setup.inject(rts);

    // Process RTS - should generate CTS response
    setup.nm.update(0);

    // Verify CTS was sent
    auto tx_frames = setup.captured();
    bool cts_sent = false;
    for (const auto &f : tx_frames) {
        if (f.pgn() == PGN_TP_CM && f.data[0] == tp_cm::CTS) {
            cts_sent = true;
        }
    }
    CHECK(cts_sent);

    // Inject data frames
    Frame dt1;
    dt1.id = Identifier::encode(Priority::Lowest, PGN_TP_DT, remote_addr, 0x28);
    dt1.data[0] = 1;
    for (u8 i = 0; i < 7; ++i) dt1.data[i + 1] = original[i];
    dt1.length = 8;
    setup.inject(dt1);

    Frame dt2;
    dt2.id = Identifier::encode(Priority::Lowest, PGN_TP_DT, remote_addr, 0x28);
    dt2.data[0] = 2;
    for (u8 i = 0; i < 7; ++i) dt2.data[i + 1] = (7 + i < 14) ? original[7 + i] : 0xFF;
    dt2.length = 8;
    setup.inject(dt2);

    // Process data - should complete and send EOMA
    setup.nm.update(0);

    CHECK(received);
    CHECK(received_data.size() == 14);
    for (usize i = 0; i < 14; ++i) {
        CHECK(received_data[i] == static_cast<u8>(i + 0x10));
    }

    // Verify EOMA was sent
    tx_frames = setup.captured();
    bool eoma_sent = false;
    for (const auto &f : tx_frames) {
        if (f.pgn() == PGN_TP_CM && f.data[0] == tp_cm::EOMA) {
            eoma_sent = true;
        }
    }
    CHECK(eoma_sent);
}

TEST_CASE("IsoNet - TP BAM send with update generates data") {
    TestSetup setup;

    dp::Vector<u8> data(14, 0x42);
    auto result = setup.nm.send(PGN_DM1, data, setup.cf); // Broadcast
    CHECK(result.is_ok());

    setup.drain();

    // Update should generate BAM data frames
    setup.nm.update(50);

    auto frames = setup.captured();
    // Should have sent data frames
    usize dt_count = 0;
    for (const auto &f : frames) {
        if (f.pgn() == PGN_TP_DT) {
            dt_count++;
        }
    }
    CHECK(dt_count > 0);
}

TEST_CASE("IsoNet - transport frames not dispatched as regular messages") {
    TestSetup setup;

    bool tp_cm_dispatched = false;
    bool tp_dt_dispatched = false;
    setup.nm.register_pgn_callback(PGN_TP_CM, [&](const Message &) { tp_cm_dispatched = true; });
    setup.nm.register_pgn_callback(PGN_TP_DT, [&](const Message &) { tp_dt_dispatched = true; });

    // Inject a TP.CM BAM frame
    Frame bam;
    bam.id = Identifier::encode(Priority::Lowest, PGN_TP_CM, 0x30, BROADCAST_ADDRESS);
    bam.data[0] = tp_cm::BAM;
    bam.data[1] = 14;
    bam.data[2] = 0;
    bam.data[3] = 2;
    bam.data[4] = 0xFF;
    bam.data[5] = 0xCA;
    bam.data[6] = 0xFE;
    bam.data[7] = 0x00;
    bam.length = 8;
    setup.inject(bam);

    setup.nm.update(0);

    // TP.CM should NOT be dispatched as a regular message
    CHECK_FALSE(tp_cm_dispatched);
    CHECK_FALSE(tp_dt_dispatched);
}

TEST_CASE("IsoNet - fast packet send and receive") {
    NetworkConfig cfg;
    cfg.enable_fast_packet = true;

    auto link = std::make_shared<wirebit::SocketCanLink>(
        wirebit::SocketCanLink::create({.interface_name = "vcan_nm_fp", .create_if_missing = true, .destroy_on_close = true}).value());
    wirebit::CanEndpoint ep(std::static_pointer_cast<wirebit::Link>(link), wirebit::CanConfig{.bitrate = 250000}, 1);

    auto h_link = std::make_shared<wirebit::SocketCanLink>(wirebit::SocketCanLink::attach("vcan_nm_fp").value());
    wirebit::CanEndpoint harness(std::static_pointer_cast<wirebit::Link>(h_link), wirebit::CanConfig{.bitrate = 250000}, 2);

    IsoNet nm(cfg);
    nm.set_endpoint(0, &ep);
    auto *cf = nm.create_internal(Name::build().set_identity_number(2).set_manufacturer_code(200), 0, 0x29).value();

    nm.register_fast_packet_pgn(PGN_GNSS_POSITION);

    SUBCASE("send uses fast packet for registered PGN") {
        dp::Vector<u8> data(50, 0x55);
        auto result = nm.send(PGN_GNSS_POSITION, data, cf);
        CHECK(result.is_ok());

        dp::Vector<Frame> frames;
        can_frame rx;
        while (harness.recv_can(rx).is_ok()) {
            Frame f;
            f.id = Identifier(rx.can_id & CAN_EFF_MASK);
            f.length = rx.can_dlc > 8 ? 8 : rx.can_dlc;
            for (u8 i = 0; i < f.length; ++i) f.data[i] = rx.data[i];
            frames.push_back(f);
        }
        // Fast packet: first frame + subsequent frames
        // 50 bytes: first=6, then 44/7 = 7 more frames = 8 total
        CHECK(frames.size() == 8);
    }

    SUBCASE("receive reassembles fast packet") {
        bool received = false;
        dp::Vector<u8> received_data;
        nm.register_pgn_callback(PGN_GNSS_POSITION, [&](const Message &msg) {
            received = true;
            received_data = msg.data;
        });

        dp::Vector<u8> original(50);
        for (usize i = 0; i < 50; ++i) original[i] = static_cast<u8>(i);

        // Generate fast packet frames manually
        FastPacketProtocol fp;
        auto frames_result = fp.send(PGN_GNSS_POSITION, original, 0x30);
        CHECK(frames_result.is_ok());

        // Inject all frames via harness
        for (const auto &f : frames_result.value()) {
            can_frame tx = wirebit::CanEndpoint::make_ext_frame(f.id.raw, f.data.data(), f.length);
            harness.send_can(tx);
        }

        nm.update(0);

        CHECK(received);
        CHECK(received_data.size() == 50);
        for (usize i = 0; i < 50; ++i) {
            CHECK(received_data[i] == static_cast<u8>(i));
        }
    }
}

TEST_CASE("IsoNet - non-fast-packet PGN uses TP for multi-frame") {
    NetworkConfig cfg;
    cfg.enable_fast_packet = true;

    auto link = std::make_shared<wirebit::SocketCanLink>(
        wirebit::SocketCanLink::create({.interface_name = "vcan_nm_nfp", .create_if_missing = true, .destroy_on_close = true}).value());
    wirebit::CanEndpoint ep(std::static_pointer_cast<wirebit::Link>(link), wirebit::CanConfig{.bitrate = 250000}, 1);

    auto h_link = std::make_shared<wirebit::SocketCanLink>(wirebit::SocketCanLink::attach("vcan_nm_nfp").value());
    wirebit::CanEndpoint harness(std::static_pointer_cast<wirebit::Link>(h_link), wirebit::CanConfig{.bitrate = 250000}, 2);

    IsoNet nm(cfg);
    nm.set_endpoint(0, &ep);
    auto *cf = nm.create_internal(Name::build().set_identity_number(3).set_manufacturer_code(300), 0, 0x2A).value();

    nm.register_fast_packet_pgn(PGN_GNSS_POSITION);

    // A non-registered PGN should use TP, not fast packet
    dp::Vector<u8> data(50, 0x66);
    auto result = nm.send(PGN_DM1, data, cf); // Not registered as FP
    CHECK(result.is_ok());

    dp::Vector<Frame> frames;
    can_frame rx;
    while (harness.recv_can(rx).is_ok()) {
        Frame f;
        f.id = Identifier(rx.can_id & CAN_EFF_MASK);
        f.length = rx.can_dlc > 8 ? 8 : rx.can_dlc;
        for (u8 i = 0; i < f.length; ++i) f.data[i] = rx.data[i];
        frames.push_back(f);
    }
    CHECK(frames.size() == 1); // BAM announcement
    CHECK(frames[0].pgn() == PGN_TP_CM);
    CHECK(frames[0].data[0] == tp_cm::BAM);
}

TEST_CASE("IsoNet - transport protocol accessor") {
    IsoNet nm;
    // Verify transport instances are accessible
    TransportProtocol &tp = nm.transport_protocol();
    ExtendedTransportProtocol &etp = nm.extended_transport_protocol();
    FastPacketProtocol &fp = nm.fast_packet_protocol();
    (void)tp;
    (void)etp;
    (void)fp;
}
