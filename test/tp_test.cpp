#include <doctest/doctest.h>
#include <agrobus/net/tp.hpp>
#include <agrobus/net/constants.hpp>

using namespace agrobus::net;

TEST_CASE("Transport Protocol BAM send") {
    TransportProtocol tp;

    dp::Vector<u8> data(100, 0xAA); // 100 bytes, requires TP
    auto result = tp.send(0xFECA, data, 0x28, BROADCAST_ADDRESS);
    CHECK(result.is_ok());

    auto& frames = result.value();
    CHECK(!frames.empty());
    // First frame should be BAM announcement
    CHECK(frames[0].data[0] == tp_cm::BAM);
    // Message size
    u16 msg_size = static_cast<u16>(frames[0].data[1]) | (static_cast<u16>(frames[0].data[2]) << 8);
    CHECK(msg_size == 100);
}

TEST_CASE("Transport Protocol CM send") {
    TransportProtocol tp;

    dp::Vector<u8> data(50, 0xBB);
    auto result = tp.send(0xFECA, data, 0x28, 0x30);
    CHECK(result.is_ok());

    auto& frames = result.value();
    CHECK(!frames.empty());
    // First frame should be RTS
    CHECK(frames[0].data[0] == tp_cm::RTS);
}

TEST_CASE("Transport Protocol rejects too-small data") {
    TransportProtocol tp;
    dp::Vector<u8> data(5); // <= 8 bytes, single frame
    auto result = tp.send(0xFECA, data, 0x28, 0x30);
    CHECK(result.is_err());
}

TEST_CASE("Transport Protocol rejects too-large data") {
    TransportProtocol tp;
    dp::Vector<u8> data(TP_MAX_DATA_LENGTH + 1, 0);
    auto result = tp.send(0xFECA, data, 0x28, BROADCAST_ADDRESS);
    CHECK(result.is_err());
}

TEST_CASE("Transport Protocol timeout") {
    TransportProtocol tp;

    bool aborted = false;
    tp.on_abort.subscribe([&](TransportSession&, TransportAbortReason reason) {
        aborted = true;
        CHECK(reason == TransportAbortReason::Timeout);
    });

    dp::Vector<u8> data(50, 0xCC);
    tp.send(0xFECA, data, 0x28, 0x30); // RTS, waiting for CTS
    tp.update(TP_TIMEOUT_T3_MS + 1);

    CHECK(aborted);
}

TEST_CASE("Transport Protocol BAM receive") {
    TransportProtocol tp;

    bool completed = false;
    dp::Vector<u8> received_data;
    tp.on_complete.subscribe([&](TransportSession& session) {
        completed = true;
        received_data = session.data;
    });

    // Simulate incoming BAM
    Frame bam;
    bam.id = Identifier::encode(Priority::Lowest, PGN_TP_CM, 0x30, BROADCAST_ADDRESS);
    bam.data[0] = tp_cm::BAM;
    bam.data[1] = 14; // 14 bytes total
    bam.data[2] = 0;
    bam.data[3] = 2;  // 2 packets
    bam.data[4] = 0xFF;
    bam.data[5] = 0xCA; // PGN low
    bam.data[6] = 0xFE; // PGN mid
    bam.data[7] = 0x00; // PGN high
    bam.length = 8;
    tp.process_frame(bam);

    // First data frame
    Frame dt1;
    dt1.id = Identifier::encode(Priority::Lowest, PGN_TP_DT, 0x30, BROADCAST_ADDRESS);
    dt1.data[0] = 1; // sequence 1
    for (i32 i = 1; i < 8; i++) dt1.data[i] = static_cast<u8>(i);
    dt1.length = 8;
    tp.process_frame(dt1);

    // Second data frame
    Frame dt2;
    dt2.id = Identifier::encode(Priority::Lowest, PGN_TP_DT, 0x30, BROADCAST_ADDRESS);
    dt2.data[0] = 2; // sequence 2
    for (i32 i = 1; i < 8; i++) dt2.data[i] = static_cast<u8>(i + 7);
    dt2.length = 8;
    tp.process_frame(dt2);

    CHECK(completed);
    CHECK(received_data.size() == 14);
}

TEST_CASE("Transport Protocol session exists error") {
    TransportProtocol tp;
    dp::Vector<u8> data(50, 0xDD);
    auto r1 = tp.send(0xFECA, data, 0x28, 0x30);
    CHECK(r1.is_ok());
    auto r2 = tp.send(0xFECA, data, 0x28, 0x30);
    CHECK(r2.is_err()); // Already in session
}
