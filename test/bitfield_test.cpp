#include <doctest/doctest.h>
#include <agrobus/net/bitfield.hpp>

using namespace agrobus::net;
using namespace agrobus::net::bitfield;

// ═════════════════════════════════════════════════════════════════════════════
// get_bits Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("get_bits u8") {
    SUBCASE("extract single bit") {
        u8 value = 0b10101010;
        CHECK(get_bits(value, 0, 1) == 0);
        CHECK(get_bits(value, 1, 1) == 1);
        CHECK(get_bits(value, 2, 1) == 0);
        CHECK(get_bits(value, 3, 1) == 1);
    }

    SUBCASE("extract multiple bits") {
        u8 value = 0b11110000;
        CHECK(get_bits(value, 0, 4) == 0b0000);
        CHECK(get_bits(value, 4, 4) == 0b1111);
        CHECK(get_bits(value, 2, 4) == 0b1100);
    }

    SUBCASE("extract all bits") {
        u8 value = 0b10101010;
        CHECK(get_bits(value, 0, 8) == 0b10101010);
    }

    SUBCASE("zero length returns 0") {
        u8 value = 0xFF;
        CHECK(get_bits(value, 0, 0) == 0);
        CHECK(get_bits(value, 4, 0) == 0);
    }

    SUBCASE("out of bounds start_bit returns 0") {
        u8 value = 0xFF;
        CHECK(get_bits(value, 8, 1) == 0);
        CHECK(get_bits(value, 10, 2) == 0);
    }

    SUBCASE("length >= bit_width extracts from start") {
        u8 value = 0b11110000;
        CHECK(get_bits(value, 0, 8) == 0b11110000);
        CHECK(get_bits(value, 4, 8) == 0b00001111);
    }
}

TEST_CASE("get_bits u16") {
    SUBCASE("extract bits across byte boundary") {
        u16 value = 0b1111000011110000;
        CHECK(get_bits(value, 0, 4) == 0b0000);
        CHECK(get_bits(value, 4, 4) == 0b1111);
        CHECK(get_bits(value, 8, 4) == 0b0000);
        CHECK(get_bits(value, 12, 4) == 0b1111);
    }

    SUBCASE("extract middle bits") {
        u16 value = 0b1010101010101010;
        CHECK(get_bits(value, 2, 8) == 0b10101010); // Bits 2-9
        CHECK(get_bits(value, 4, 8) == 0b10101010); // Bits 4-11 (all alternating 1/0)
    }
}

TEST_CASE("get_bits u32") {
    SUBCASE("extract from large value") {
        u32 value = 0xFF00FF00;
        CHECK(get_bits(value, 0, 8) == 0x00);
        CHECK(get_bits(value, 8, 8) == 0xFF);
        CHECK(get_bits(value, 16, 8) == 0x00);
        CHECK(get_bits(value, 24, 8) == 0xFF);
    }
}

TEST_CASE("get_bits u64") {
    SUBCASE("extract from 64-bit value") {
        u64 value = 0x123456789ABCDEF0ULL;
        CHECK(get_bits(value, 0, 8) == 0xF0);
        CHECK(get_bits(value, 8, 8) == 0xDE);
        CHECK(get_bits(value, 56, 8) == 0x12);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// set_bits Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("set_bits u8") {
    SUBCASE("set single bit") {
        u8 value = 0b00000000;
        value = set_bits<u8>(value, 0, 1, 1);
        CHECK(value == 0b00000001);
        value = set_bits<u8>(value, 2, 1, 1);
        CHECK(value == 0b00000101);
    }

    SUBCASE("set multiple bits") {
        u8 value = 0b00000000;
        value = set_bits<u8>(value, 0, 4, 0b1010);
        CHECK(value == 0b00001010);
        value = set_bits<u8>(value, 4, 4, 0b0101);
        CHECK(value == 0b01011010);
    }

    SUBCASE("overwrite existing bits") {
        u8 value = 0b11111111;
        value = set_bits<u8>(value, 2, 4, 0b0000);
        CHECK(value == 0b11000011);
    }

    SUBCASE("zero length does nothing") {
        u8 value = 0b10101010;
        u8 orig = value;
        value = set_bits<u8>(value, 0, 0, 0xFF);
        CHECK(value == orig);
    }

    SUBCASE("out of bounds start_bit does nothing") {
        u8 value = 0b10101010;
        u8 orig = value;
        value = set_bits<u8>(value, 8, 1, 1);
        CHECK(value == orig);
    }
}

TEST_CASE("set_bits u16") {
    SUBCASE("set bits across byte boundary") {
        u16 value = 0x0000;
        value = set_bits(value, 4, 8, static_cast<u16>(0xFF));
        CHECK(value == 0x0FF0);
    }

    SUBCASE("round trip get/set") {
        u16 value = 0x1234;
        u16 extracted = get_bits(value, 4, 8);
        u16 new_value = 0x0000;
        new_value = set_bits(new_value, 4, 8, extracted);
        CHECK(get_bits(value, 4, 8) == get_bits(new_value, 4, 8));
    }
}

TEST_CASE("set_bits u32") {
    SUBCASE("set full byte") {
        u32 value = 0x00000000;
        value = set_bits(value, 8, 8, static_cast<u32>(0xAB));
        CHECK(value == 0x0000AB00);
    }
}

TEST_CASE("set_bits u64") {
    SUBCASE("set bits in 64-bit value") {
        u64 value = 0x0;
        value = set_bits(value, 32, 16, static_cast<u64>(0x1234));
        CHECK(value == 0x0000123400000000ULL);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// get_bit / set_bit Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("get_bit") {
    SUBCASE("check individual bits") {
        u8 value = 0b10101010;
        CHECK(get_bit(value, 0) == false);
        CHECK(get_bit(value, 1) == true);
        CHECK(get_bit(value, 2) == false);
        CHECK(get_bit(value, 3) == true);
        CHECK(get_bit(value, 7) == true);
    }

    SUBCASE("out of bounds returns false") {
        u8 value = 0xFF;
        CHECK(get_bit(value, 8) == false);
        CHECK(get_bit(value, 100) == false);
    }

    SUBCASE("u16 bit access") {
        u16 value = 0x8000;
        CHECK(get_bit(value, 15) == true);
        CHECK(get_bit(value, 14) == false);
    }
}

TEST_CASE("set_bit") {
    SUBCASE("set bits to true") {
        u8 value = 0b00000000;
        value = set_bit(value, 0, true);
        CHECK(value == 0b00000001);
        value = set_bit(value, 2, true);
        CHECK(value == 0b00000101);
        value = set_bit(value, 7, true);
        CHECK(value == 0b10000101);
    }

    SUBCASE("set bits to false") {
        u8 value = 0b11111111;
        value = set_bit(value, 0, false);
        CHECK(value == 0b11111110);
        value = set_bit(value, 3, false);
        CHECK(value == 0b11110110);
    }

    SUBCASE("toggle bits") {
        u8 value = 0b10101010;
        value = set_bit(value, 0, true);
        CHECK(value == 0b10101011);
        value = set_bit(value, 1, false);
        CHECK(value == 0b10101001);
    }

    SUBCASE("out of bounds does nothing") {
        u8 value = 0b10101010;
        u8 orig = value;
        value = set_bit(value, 8, true);
        CHECK(value == orig);
        value = set_bit(value, 100, false);
        CHECK(value == orig);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Little-Endian Packing Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("pack_u16_le") {
    SUBCASE("pack zero") {
        u8 data[2] = {0xFF, 0xFF};
        pack_u16_le(data, 0x0000);
        CHECK(data[0] == 0x00);
        CHECK(data[1] == 0x00);
    }

    SUBCASE("pack 0x1234") {
        u8 data[2];
        pack_u16_le(data, 0x1234);
        CHECK(data[0] == 0x34); // Low byte first
        CHECK(data[1] == 0x12); // High byte second
    }

    SUBCASE("pack max value") {
        u8 data[2];
        pack_u16_le(data, 0xFFFF);
        CHECK(data[0] == 0xFF);
        CHECK(data[1] == 0xFF);
    }
}

TEST_CASE("pack_u32_le") {
    SUBCASE("pack zero") {
        u8 data[4];
        pack_u32_le(data, 0x00000000);
        CHECK(data[0] == 0x00);
        CHECK(data[1] == 0x00);
        CHECK(data[2] == 0x00);
        CHECK(data[3] == 0x00);
    }

    SUBCASE("pack 0x12345678") {
        u8 data[4];
        pack_u32_le(data, 0x12345678);
        CHECK(data[0] == 0x78); // LSB first
        CHECK(data[1] == 0x56);
        CHECK(data[2] == 0x34);
        CHECK(data[3] == 0x12); // MSB last
    }

    SUBCASE("pack max value") {
        u8 data[4];
        pack_u32_le(data, 0xFFFFFFFF);
        CHECK(data[0] == 0xFF);
        CHECK(data[1] == 0xFF);
        CHECK(data[2] == 0xFF);
        CHECK(data[3] == 0xFF);
    }
}

TEST_CASE("pack_u64_le") {
    SUBCASE("pack zero") {
        u8 data[8];
        pack_u64_le(data, 0x0000000000000000ULL);
        for (int i = 0; i < 8; i++) {
            CHECK(data[i] == 0x00);
        }
    }

    SUBCASE("pack 0x123456789ABCDEF0") {
        u8 data[8];
        pack_u64_le(data, 0x123456789ABCDEF0ULL);
        CHECK(data[0] == 0xF0); // LSB first
        CHECK(data[1] == 0xDE);
        CHECK(data[2] == 0xBC);
        CHECK(data[3] == 0x9A);
        CHECK(data[4] == 0x78);
        CHECK(data[5] == 0x56);
        CHECK(data[6] == 0x34);
        CHECK(data[7] == 0x12); // MSB last
    }

    SUBCASE("pack max value") {
        u8 data[8];
        pack_u64_le(data, 0xFFFFFFFFFFFFFFFFULL);
        for (int i = 0; i < 8; i++) {
            CHECK(data[i] == 0xFF);
        }
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Little-Endian Unpacking Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("unpack_u16_le") {
    SUBCASE("unpack zero") {
        u8 data[2] = {0x00, 0x00};
        CHECK(unpack_u16_le(data) == 0x0000);
    }

    SUBCASE("unpack 0x1234") {
        u8 data[2] = {0x34, 0x12}; // Little-endian
        CHECK(unpack_u16_le(data) == 0x1234);
    }

    SUBCASE("unpack max value") {
        u8 data[2] = {0xFF, 0xFF};
        CHECK(unpack_u16_le(data) == 0xFFFF);
    }
}

TEST_CASE("unpack_u32_le") {
    SUBCASE("unpack zero") {
        u8 data[4] = {0x00, 0x00, 0x00, 0x00};
        CHECK(unpack_u32_le(data) == 0x00000000);
    }

    SUBCASE("unpack 0x12345678") {
        u8 data[4] = {0x78, 0x56, 0x34, 0x12}; // Little-endian
        CHECK(unpack_u32_le(data) == 0x12345678);
    }

    SUBCASE("unpack max value") {
        u8 data[4] = {0xFF, 0xFF, 0xFF, 0xFF};
        CHECK(unpack_u32_le(data) == 0xFFFFFFFF);
    }
}

TEST_CASE("unpack_u64_le") {
    SUBCASE("unpack zero") {
        u8 data[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        CHECK(unpack_u64_le(data) == 0x0000000000000000ULL);
    }

    SUBCASE("unpack 0x123456789ABCDEF0") {
        u8 data[8] = {0xF0, 0xDE, 0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12}; // Little-endian
        CHECK(unpack_u64_le(data) == 0x123456789ABCDEF0ULL);
    }

    SUBCASE("unpack max value") {
        u8 data[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        CHECK(unpack_u64_le(data) == 0xFFFFFFFFFFFFFFFFULL);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Round-Trip Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("pack/unpack round trip u16") {
    dp::Vector<u16> test_values = {0x0000, 0x0001, 0x00FF, 0xFF00, 0x1234, 0x5678, 0xABCD, 0xFFFF};

    for (u16 original : test_values) {
        u8 data[2];
        pack_u16_le(data, original);
        u16 unpacked = unpack_u16_le(data);
        CHECK(unpacked == original);
    }
}

TEST_CASE("pack/unpack round trip u32") {
    dp::Vector<u32> test_values = {0x00000000, 0x00000001, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000,
                                    0x12345678, 0x9ABCDEF0, 0xFFFFFFFF};

    for (u32 original : test_values) {
        u8 data[4];
        pack_u32_le(data, original);
        u32 unpacked = unpack_u32_le(data);
        CHECK(unpacked == original);
    }
}

TEST_CASE("pack/unpack round trip u64") {
    dp::Vector<u64> test_values = {0x0000000000000000ULL, 0x0000000000000001ULL, 0x00000000000000FFULL,
                                    0x000000000000FF00ULL, 0x0000000000FF0000ULL, 0x00000000FF000000ULL,
                                    0x000000FF00000000ULL, 0x0000FF0000000000ULL, 0x00FF000000000000ULL,
                                    0xFF00000000000000ULL, 0x123456789ABCDEF0ULL, 0xFFFFFFFFFFFFFFFFULL};

    for (u64 original : test_values) {
        u8 data[8];
        pack_u64_le(data, original);
        u64 unpacked = unpack_u64_le(data);
        CHECK(unpacked == original);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Edge Case Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("edge cases") {
    SUBCASE("all zeros") {
        u32 value = 0x00000000;
        CHECK(get_bits(value, 0, 32) == 0);
        CHECK(get_bit(value, 0) == false);
        CHECK(get_bit(value, 31) == false);
    }

    SUBCASE("all ones") {
        u32 value = 0xFFFFFFFF;
        CHECK(get_bits(value, 0, 32) == 0xFFFFFFFF);
        CHECK(get_bit(value, 0) == true);
        CHECK(get_bit(value, 31) == true);
    }

    SUBCASE("single bit patterns") {
        u8 value = 0b00000001;
        for (int i = 0; i < 8; i++) {
            CHECK(get_bit(value, i) == (i == 0));
        }

        value = 0b10000000;
        for (int i = 0; i < 8; i++) {
            CHECK(get_bit(value, i) == (i == 7));
        }
    }

    SUBCASE("alternating patterns") {
        u16 value = 0b1010101010101010;
        for (int i = 0; i < 16; i++) {
            CHECK(get_bit(value, i) == (i % 2 == 1));
        }
    }
}
