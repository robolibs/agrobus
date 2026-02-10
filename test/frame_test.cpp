#include <doctest/doctest.h>
#include <agrobus/net/frame.hpp>
#include <agrobus/net/constants.hpp>

using namespace agrobus::net;

TEST_CASE("Frame construction") {
    SUBCASE("default frame") {
        Frame f;
        CHECK(f.length == 8);
        CHECK(f.id.raw == 0);
    }

    SUBCASE("from_message") {
        u8 payload[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
        Frame f = Frame::from_message(Priority::Default, PGN_VEHICLE_SPEED, 0x28, BROADCAST_ADDRESS, payload, 8);
        CHECK(f.pgn() == PGN_VEHICLE_SPEED);
        CHECK(f.source() == 0x28);
        CHECK(f.is_broadcast());
        CHECK(f.data[0] == 0x01);
        CHECK(f.data[7] == 0x08);
    }

    SUBCASE("short payload pads with 0xFF") {
        u8 payload[] = {0xAA, 0xBB, 0xCC};
        Frame f = Frame::from_message(Priority::Normal, PGN_REQUEST, 0x10, 0x20, payload, 3);
        CHECK(f.data[0] == 0xAA);
        CHECK(f.data[2] == 0xCC);
        CHECK(f.data[3] == 0xFF);
        CHECK(f.data[7] == 0xFF);
    }

    SUBCASE("priority accessor") {
        Frame f;
        f.id = Identifier::encode(Priority::High, PGN_DM1, 0x00, BROADCAST_ADDRESS);
        CHECK(f.priority() == Priority::High);
    }
}
