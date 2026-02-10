// GCC false positive: datapod Optional/Vector union internals
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

#include <doctest/doctest.h>
#include <agrobus/net/tp.hpp>
#include <agrobus/net/etp.hpp>
#include <agrobus/net/fast_packet.hpp>
#include <agrobus/net/constants.hpp>

using namespace agrobus::net;

TEST_CASE("Transport auto-selection by size") {
    SUBCASE("single frame (<=8 bytes) - no transport needed") {
        dp::Vector<u8> data(8, 0xAA);
        TransportProtocol tp;
        auto result = tp.send(0xFECA, data, 0x28, 0x30);
        CHECK(result.is_err()); // TP rejects <= 8 bytes
    }

    SUBCASE("TP range (9 to 1785 bytes)") {
        TransportProtocol tp;
        dp::Vector<u8> data(100, 0xBB);
        auto result = tp.send(0xFECA, data, 0x28, 0x30);
        CHECK(result.is_ok());
    }

    SUBCASE("ETP range (>1785 bytes)") {
        ExtendedTransportProtocol etp;
        dp::Vector<u8> data(2000, 0xCC);
        auto result = etp.send(0xFECA, data, 0x28, 0x30);
        CHECK(result.is_ok());
    }

    SUBCASE("TP rejects too large") {
        TransportProtocol tp;
        dp::Vector<u8> data(TP_MAX_DATA_LENGTH + 1, 0xDD);
        auto result = tp.send(0xFECA, data, 0x28, 0x30);
        CHECK(result.is_err());
    }

    SUBCASE("ETP rejects too small") {
        ExtendedTransportProtocol etp;
        dp::Vector<u8> data(100, 0xEE);
        auto result = etp.send(0xFECA, data, 0x28, 0x30);
        CHECK(result.is_err());
    }
}

TEST_CASE("BAM vs CM selection") {
    TransportProtocol tp;
    dp::Vector<u8> data(100, 0xAA);

    SUBCASE("broadcast uses BAM") {
        auto result = tp.send(0xFECA, data, 0x28, BROADCAST_ADDRESS);
        CHECK(result.is_ok());
        auto& frames = result.value();
        CHECK(frames[0].data[0] == tp_cm::BAM);
    }

    SUBCASE("destination-specific uses RTS/CTS") {
        auto result = tp.send(0xFECA, data, 0x28, 0x30);
        CHECK(result.is_ok());
        auto& frames = result.value();
        CHECK(frames[0].data[0] == tp_cm::RTS);
    }
}

TEST_CASE("TP data integrity via BAM") {
    TransportProtocol tp_tx;
    TransportProtocol tp_rx;

    // Generate test data pattern
    dp::Vector<u8> original(14);
    for (usize i = 0; i < original.size(); ++i) {
        original[i] = static_cast<u8>(i + 1);
    }

    // Send - returns BAM announcement + data frames
    auto send_result = tp_tx.send(0xFECA, original, 0x28, BROADCAST_ADDRESS);
    CHECK(send_result.is_ok());

    // Receive by feeding frames
    bool completed = false;
    dp::Vector<u8> received;
    tp_rx.on_complete.subscribe([&](TransportSession& session) {
        completed = true;
        received = session.data;
    });

    auto& frames = send_result.value();
    for (const auto& frame : frames) {
        tp_rx.process_frame(frame);
    }

    // For BAM, need to also process data frames generated via update
    for (i32 i = 0; i < 100 && !completed; ++i) {
        auto update_frames = tp_tx.update(50);
        for (const auto& f : update_frames) {
            tp_rx.process_frame(f);
        }
    }

    CHECK(completed);
    CHECK(received.size() == 14);
    for (usize i = 0; i < received.size(); ++i) {
        CHECK(received[i] == static_cast<u8>(i + 1));
    }
}

TEST_CASE("Fast Packet data integrity") {
    FastPacketProtocol fp_tx;
    FastPacketProtocol fp_rx;

    dp::Vector<u8> original(50);
    for (usize i = 0; i < original.size(); ++i) {
        original[i] = static_cast<u8>((i * 7) & 0xFF);
    }

    auto send_result = fp_tx.send(PGN_GNSS_POSITION, original, 0x28);
    CHECK(send_result.is_ok());

    dp::Optional<Message> msg{};
    for (const auto& frame : send_result.value()) {
        msg = fp_rx.process_frame(frame);
    }

    CHECK(msg.has_value());
    CHECK(msg->data.size() == 50);
    for (usize i = 0; i < 50; ++i) {
        CHECK(msg->data[i] == static_cast<u8>((i * 7) & 0xFF));
    }
}

TEST_CASE("Multiple concurrent TP sessions") {
    TransportProtocol tp;

    dp::Vector<u8> data1(100, 0x11);
    dp::Vector<u8> data2(200, 0x22);

    // First session: 0x28 -> 0x30
    auto r1 = tp.send(0xFECA, data1, 0x28, 0x30);
    CHECK(r1.is_ok());

    // Second session: different source -> different destination
    auto r2 = tp.send(0xFECB, data2, 0x29, 0x31);
    CHECK(r2.is_ok());

    // Different PGN, same source+dest: should succeed (keyed by pgn too)
    auto r3 = tp.send(0xFECC, data1, 0x28, 0x30);
    CHECK(r3.is_ok());

    // Same source+dest+pgn should fail (already in session)
    auto r4 = tp.send(0xFECA, data1, 0x28, 0x30);
    CHECK(r4.is_err());
}
