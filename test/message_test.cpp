#include <doctest/doctest.h>
#include <agrobus/net/message.hpp>

using namespace agrobus::net;

TEST_CASE("Message data extraction") {
    Message msg;
    msg.data = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};

    SUBCASE("get_u8") {
        CHECK(msg.get_u8(0) == 0x12);
        CHECK(msg.get_u8(7) == 0xF0);
        CHECK(msg.get_u8(8) == 0xFF); // out of bounds
    }

    SUBCASE("get_u16_le") {
        CHECK(msg.get_u16_le(0) == 0x3412);
        CHECK(msg.get_u16_le(2) == 0x7856);
    }

    SUBCASE("get_u32_le") {
        CHECK(msg.get_u32_le(0) == 0x78563412);
        CHECK(msg.get_u32_le(4) == 0xF0DEBC9A);
    }

    SUBCASE("get_bit") {
        // 0x12 = 0001 0010
        CHECK(!msg.get_bit(0, 0)); // bit 0 = 0
        CHECK(msg.get_bit(0, 1));  // bit 1 = 1
        CHECK(!msg.get_bit(0, 2)); // bit 2 = 0
        CHECK(!msg.get_bit(0, 3)); // bit 3 = 0
        CHECK(msg.get_bit(0, 4));  // bit 4 = 1
    }

    SUBCASE("get_bits") {
        // First byte: 0x12 = 0001 0010 (LSB first in bit field)
        CHECK(msg.get_bits(0, 4) == 0x02); // lower nibble
        CHECK(msg.get_bits(4, 4) == 0x01); // upper nibble
    }

    SUBCASE("is_broadcast") {
        msg.destination = BROADCAST_ADDRESS;
        CHECK(msg.is_broadcast());
        msg.destination = 0x20;
        CHECK(!msg.is_broadcast());
    }
}

TEST_CASE("Message data setters") {
    Message msg;

    SUBCASE("set_u8") {
        msg.set_u8(0, 0xAB);
        CHECK(msg.data.size() >= 1);
        CHECK(msg.data[0] == 0xAB);
    }

    SUBCASE("set_u16_le") {
        msg.set_u16_le(0, 0x1234);
        CHECK(msg.data[0] == 0x34);
        CHECK(msg.data[1] == 0x12);
    }

    SUBCASE("set_u32_le") {
        msg.set_u32_le(0, 0x12345678);
        CHECK(msg.data[0] == 0x78);
        CHECK(msg.data[1] == 0x56);
        CHECK(msg.data[2] == 0x34);
        CHECK(msg.data[3] == 0x12);
    }

    SUBCASE("auto-resize on set beyond current size") {
        msg.set_u8(10, 0xCC);
        CHECK(msg.data.size() >= 11);
        CHECK(msg.data[10] == 0xCC);
    }
}

TEST_CASE("Message construction") {
    Message msg(0xFEF1, {0x01, 0x02, 0x03}, 0x28, BROADCAST_ADDRESS, Priority::Default);
    CHECK(msg.pgn == 0xFEF1);
    CHECK(msg.source == 0x28);
    CHECK(msg.destination == BROADCAST_ADDRESS);
    CHECK(msg.data.size() == 3);
    CHECK(msg.priority == Priority::Default);
}
