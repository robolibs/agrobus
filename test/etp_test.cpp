#include <doctest/doctest.h>
#include <agrobus/net/etp.hpp>
#include <agrobus/net/constants.hpp>

using namespace agrobus::net;

TEST_CASE("Extended Transport Protocol send") {
    ExtendedTransportProtocol etp;

    SUBCASE("accepts large data") {
        dp::Vector<u8> data(2000, 0xAA); // > 1785, needs ETP
        auto result = etp.send(0xFECA, data, 0x28, 0x30);
        CHECK(result.is_ok());
        auto& frames = result.value();
        CHECK(!frames.empty());
        CHECK(frames[0].data[0] == etp_cm::RTS);
    }

    SUBCASE("rejects broadcast") {
        dp::Vector<u8> data(2000, 0xBB);
        auto result = etp.send(0xFECA, data, 0x28, BROADCAST_ADDRESS);
        CHECK(result.is_err());
    }

    SUBCASE("rejects small data") {
        dp::Vector<u8> data(100, 0xCC); // < 1785, use regular TP
        auto result = etp.send(0xFECA, data, 0x28, 0x30);
        CHECK(result.is_err());
    }

    SUBCASE("rejects oversized data") {
        dp::Vector<u8> data(ETP_MAX_DATA_LENGTH + 1, 0);
        auto result = etp.send(0xFECA, data, 0x28, 0x30);
        CHECK(result.is_err());
    }
}

TEST_CASE("ETP timeout") {
    ExtendedTransportProtocol etp;

    bool aborted = false;
    etp.on_abort.subscribe([&](TransportSession&, TransportAbortReason reason) {
        aborted = true;
        CHECK(reason == TransportAbortReason::Timeout);
    });

    dp::Vector<u8> data(2000, 0xDD);
    etp.send(0xFECA, data, 0x28, 0x30);
    etp.update(ETP_TIMEOUT_T1_MS + 1);

    CHECK(aborted);
}

TEST_CASE("ETP RTS message format") {
    ExtendedTransportProtocol etp;
    dp::Vector<u8> data(5000, 0xEE);
    auto result = etp.send(0xFECA, data, 0x28, 0x30);
    CHECK(result.is_ok());

    auto& frames = result.value();
    Frame& rts = frames[0];
    CHECK(rts.data[0] == etp_cm::RTS);
    // Total bytes (4 bytes LE)
    u32 total = static_cast<u32>(rts.data[1]) |
                (static_cast<u32>(rts.data[2]) << 8) |
                (static_cast<u32>(rts.data[3]) << 16) |
                (static_cast<u32>(rts.data[4]) << 24);
    CHECK(total == 5000);
    // PGN
    u32 pgn = static_cast<u32>(rts.data[5]) |
              (static_cast<u32>(rts.data[6]) << 8) |
              (static_cast<u32>(rts.data[7]) << 16);
    CHECK(pgn == 0xFECA);
}
