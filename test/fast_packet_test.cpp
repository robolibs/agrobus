#include <doctest/doctest.h>
#include <agrobus/net/fast_packet.hpp>
#include <agrobus/net/constants.hpp>

using namespace agrobus::net;

TEST_CASE("Fast Packet send") {
    FastPacketProtocol fp;

    SUBCASE("multi-frame message") {
        dp::Vector<u8> data(20, 0xAA);
        auto result = fp.send(PGN_GNSS_POSITION, data, 0x28);
        CHECK(result.is_ok());
        auto& frames = result.value();
        // First frame carries 6 data bytes, subsequent carry 7
        // 20 bytes: 6 + 7 + 7 = 3 frames
        CHECK(frames.size() == 3);

        // Check first frame format
        CHECK((frames[0].data[0] & 0x1F) == 0); // frame counter = 0
        CHECK(frames[0].data[1] == 20); // total bytes
        CHECK(frames[0].data[2] == 0xAA); // first data byte
    }

    SUBCASE("rejects too small") {
        dp::Vector<u8> data(5);
        auto result = fp.send(PGN_GNSS_POSITION, data, 0x28);
        CHECK(result.is_err());
    }

    SUBCASE("rejects too large") {
        dp::Vector<u8> data(FAST_PACKET_MAX_DATA + 1);
        auto result = fp.send(PGN_GNSS_POSITION, data, 0x28);
        CHECK(result.is_err());
    }
}

TEST_CASE("Fast Packet receive") {
    FastPacketProtocol fp;

    dp::Vector<u8> original(20);
    for (usize i = 0; i < 20; ++i) original[i] = static_cast<u8>(i);

    // Send to get frames
    auto result = fp.send(PGN_GNSS_POSITION, original, 0x30);
    CHECK(result.is_ok());
    auto& frames = result.value();

    // Create a new FP to receive
    FastPacketProtocol fp_rx;
    dp::Optional<Message> msg;

    for (const auto& frame : frames) {
        msg = fp_rx.process_frame(frame);
    }

    CHECK(msg.has_value());
    CHECK(msg->pgn == PGN_GNSS_POSITION);
    CHECK(msg->data.size() == 20);
    for (usize i = 0; i < 20; ++i) {
        CHECK(msg->data[i] == static_cast<u8>(i));
    }
}

TEST_CASE("Fast Packet bad sequence") {
    FastPacketProtocol fp;

    // First frame
    Frame f0;
    f0.id = Identifier::encode(Priority::Default, PGN_GNSS_POSITION, 0x30, BROADCAST_ADDRESS);
    f0.data[0] = 0x00; // seq=0, frame=0
    f0.data[1] = 20;   // total bytes
    for (i32 i = 2; i < 8; i++) f0.data[i] = static_cast<u8>(i);
    f0.length = 8;

    auto msg = fp.process_frame(f0);
    CHECK(!msg.has_value()); // Not complete yet

    // Skip frame 1, send frame 2 (bad sequence)
    Frame f2;
    f2.id = f0.id;
    f2.data[0] = 0x02; // seq=0, frame=2 (skipped 1!)
    for (i32 i = 1; i < 8; i++) f2.data[i] = 0xBB;
    f2.length = 8;

    msg = fp.process_frame(f2);
    CHECK(!msg.has_value()); // Should be discarded
}
