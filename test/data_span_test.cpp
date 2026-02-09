#include <doctest/doctest.h>
#include <agrobus/net/data_span.hpp>

using namespace agrobus::net;

// ═════════════════════════════════════════════════════════════════════════════
// Construction Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("DataSpan construction") {
    SUBCASE("default construction") {
        DataSpan span;
        CHECK(span.size() == 0);
        CHECK(span.empty());
        CHECK(span.data() == nullptr);
    }

    SUBCASE("from pointer and size") {
        u8 data[] = {1, 2, 3, 4, 5};
        DataSpan span(data, 5);
        CHECK(span.size() == 5);
        CHECK_FALSE(span.empty());
        CHECK(span.data() == data);
    }

    SUBCASE("from dp::Vector") {
        dp::Vector<u8> vec = {0x10, 0x20, 0x30, 0x40};
        DataSpan span(vec);
        CHECK(span.size() == 4);
        CHECK(span[0] == 0x10);
        CHECK(span[3] == 0x40);
    }

    SUBCASE("from dp::Array") {
        dp::Array<u8, 6> arr = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
        DataSpan span(arr);
        CHECK(span.size() == 6);
        CHECK(span[0] == 0xAA);
        CHECK(span[5] == 0xFF);
    }

    SUBCASE("from empty vector") {
        dp::Vector<u8> vec;
        DataSpan span(vec);
        CHECK(span.size() == 0);
        CHECK(span.empty());
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Element Access Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("DataSpan element access") {
    u8 data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    DataSpan span(data, 5);

    SUBCASE("valid indexing") {
        CHECK(span[0] == 0x01);
        CHECK(span[1] == 0x02);
        CHECK(span[2] == 0x03);
        CHECK(span[3] == 0x04);
        CHECK(span[4] == 0x05);
    }

    SUBCASE("out of bounds returns 0xFF") {
        CHECK(span[5] == 0xFF);
        CHECK(span[10] == 0xFF);
        CHECK(span[100] == 0xFF);
    }

    SUBCASE("get_u8 method") {
        CHECK(span.get_u8(0) == 0x01);
        CHECK(span.get_u8(4) == 0x05);
        CHECK(span.get_u8(5) == 0xFF);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Subspan Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("DataSpan subspan") {
    u8 data[] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80};
    DataSpan span(data, 8);

    SUBCASE("subspan from middle") {
        auto sub = span.subspan(2, 3);
        CHECK(sub.size() == 3);
        CHECK(sub[0] == 0x30);
        CHECK(sub[1] == 0x40);
        CHECK(sub[2] == 0x50);
    }

    SUBCASE("subspan to end") {
        auto sub = span.subspan(5);
        CHECK(sub.size() == 3);
        CHECK(sub[0] == 0x60);
        CHECK(sub[1] == 0x70);
        CHECK(sub[2] == 0x80);
    }

    SUBCASE("subspan with count > remaining") {
        auto sub = span.subspan(6, 100);
        CHECK(sub.size() == 2);
        CHECK(sub[0] == 0x70);
        CHECK(sub[1] == 0x80);
    }

    SUBCASE("subspan from start") {
        auto sub = span.subspan(0, 4);
        CHECK(sub.size() == 4);
        CHECK(sub[0] == 0x10);
        CHECK(sub[3] == 0x40);
    }

    SUBCASE("subspan with offset >= size returns empty") {
        auto sub = span.subspan(8, 1);
        CHECK(sub.size() == 0);
        CHECK(sub.empty());
    }

    SUBCASE("subspan of subspan") {
        auto sub1 = span.subspan(2, 4);
        auto sub2 = sub1.subspan(1, 2);
        CHECK(sub2.size() == 2);
        CHECK(sub2[0] == 0x40);
        CHECK(sub2[1] == 0x50);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Little-Endian Extraction Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("DataSpan get_u16_le") {
    u8 data[] = {0x34, 0x12, 0x78, 0x56, 0xFF, 0xFF};
    DataSpan span(data, 6);

    SUBCASE("extract at offset 0") {
        CHECK(span.get_u16_le(0) == 0x1234);
    }

    SUBCASE("extract at offset 2") {
        CHECK(span.get_u16_le(2) == 0x5678);
    }

    SUBCASE("extract at offset 4") {
        CHECK(span.get_u16_le(4) == 0xFFFF);
    }

    SUBCASE("extract with insufficient data returns 0xFFFF") {
        CHECK(span.get_u16_le(5) == 0xFFFF);
        CHECK(span.get_u16_le(6) == 0xFFFF);
    }

    SUBCASE("zero value") {
        u8 zero_data[] = {0x00, 0x00};
        DataSpan zero_span(zero_data, 2);
        CHECK(zero_span.get_u16_le(0) == 0x0000);
    }
}

TEST_CASE("DataSpan get_u32_le") {
    u8 data[] = {0x78, 0x56, 0x34, 0x12, 0xEF, 0xBE, 0xAD, 0xDE, 0xFF};
    DataSpan span(data, 9);

    SUBCASE("extract at offset 0") {
        CHECK(span.get_u32_le(0) == 0x12345678);
    }

    SUBCASE("extract at offset 4") {
        CHECK(span.get_u32_le(4) == 0xDEADBEEF);
    }

    SUBCASE("extract with insufficient data returns 0xFFFFFFFF") {
        CHECK(span.get_u32_le(6) == 0xFFFFFFFF);
        CHECK(span.get_u32_le(9) == 0xFFFFFFFF);
    }

    SUBCASE("zero value") {
        u8 zero_data[] = {0x00, 0x00, 0x00, 0x00};
        DataSpan zero_span(zero_data, 4);
        CHECK(zero_span.get_u32_le(0) == 0x00000000);
    }
}

TEST_CASE("DataSpan get_u64_le") {
    u8 data[] = {0xF0, 0xDE, 0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12, 0xFF};
    DataSpan span(data, 9);

    SUBCASE("extract at offset 0") {
        CHECK(span.get_u64_le(0) == 0x123456789ABCDEF0ULL);
    }

    SUBCASE("extract with insufficient data returns 0xFFFFFFFFFFFFFFFF") {
        CHECK(span.get_u64_le(2) == 0xFFFFFFFFFFFFFFFFULL);
        CHECK(span.get_u64_le(9) == 0xFFFFFFFFFFFFFFFFULL);
    }

    SUBCASE("zero value") {
        u8 zero_data[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        DataSpan zero_span(zero_data, 8);
        CHECK(zero_span.get_u64_le(0) == 0x0000000000000000ULL);
    }

    SUBCASE("max value") {
        u8 max_data[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        DataSpan max_span(max_data, 8);
        CHECK(max_span.get_u64_le(0) == 0xFFFFFFFFFFFFFFFFULL);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Bit Access Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("DataSpan get_bit") {
    u8 data[] = {0b10101010, 0b11110000, 0b00001111};
    DataSpan span(data, 3);

    SUBCASE("check bits in first byte") {
        CHECK(span.get_bit(0, 0) == false);
        CHECK(span.get_bit(0, 1) == true);
        CHECK(span.get_bit(0, 2) == false);
        CHECK(span.get_bit(0, 3) == true);
        CHECK(span.get_bit(0, 7) == true);
    }

    SUBCASE("check bits in second byte") {
        CHECK(span.get_bit(1, 0) == false);
        CHECK(span.get_bit(1, 3) == false);
        CHECK(span.get_bit(1, 4) == true);
        CHECK(span.get_bit(1, 7) == true);
    }

    SUBCASE("check bits in third byte") {
        CHECK(span.get_bit(2, 0) == true);
        CHECK(span.get_bit(2, 3) == true);
        CHECK(span.get_bit(2, 4) == false);
        CHECK(span.get_bit(2, 7) == false);
    }

    SUBCASE("out of bounds byte returns false") {
        CHECK(span.get_bit(3, 0) == false);
        CHECK(span.get_bit(10, 5) == false);
    }

    SUBCASE("invalid bit index returns false") {
        CHECK(span.get_bit(0, 8) == false);
        CHECK(span.get_bit(0, 10) == false);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Iterator Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("DataSpan iterators") {
    u8 data[] = {1, 2, 3, 4, 5};
    DataSpan span(data, 5);

    SUBCASE("begin and end") {
        CHECK(span.begin() == data);
        CHECK(span.end() == data + 5);
    }

    SUBCASE("range-based for loop") {
        int sum = 0;
        for (u8 byte : span) {
            sum += byte;
        }
        CHECK(sum == 15);
    }

    SUBCASE("empty span iterators") {
        DataSpan empty;
        CHECK(empty.begin() == empty.end());
    }

    SUBCASE("distance") {
        CHECK(span.end() - span.begin() == 5);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Edge Case Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("DataSpan edge cases") {
    SUBCASE("single byte span") {
        u8 data[] = {0x42};
        DataSpan span(data, 1);
        CHECK(span.size() == 1);
        CHECK(span[0] == 0x42);
        CHECK(span[1] == 0xFF);
    }

    SUBCASE("all zeros") {
        u8 data[] = {0x00, 0x00, 0x00, 0x00};
        DataSpan span(data, 4);
        CHECK(span.get_u32_le(0) == 0x00000000);
        CHECK(span[0] == 0x00);
    }

    SUBCASE("all ones") {
        u8 data[] = {0xFF, 0xFF, 0xFF, 0xFF};
        DataSpan span(data, 4);
        CHECK(span.get_u32_le(0) == 0xFFFFFFFF);
        CHECK(span[0] == 0xFF);
    }

    SUBCASE("alternating pattern") {
        u8 data[] = {0xAA, 0x55, 0xAA, 0x55};
        DataSpan span(data, 4);
        CHECK(span[0] == 0xAA);
        CHECK(span[1] == 0x55);
        CHECK(span.get_u16_le(0) == 0x55AA);
        CHECK(span.get_u16_le(2) == 0x55AA);
    }

    SUBCASE("large span") {
        dp::Vector<u8> large(1000, 0x42);
        DataSpan span(large);
        CHECK(span.size() == 1000);
        CHECK(span[0] == 0x42);
        CHECK(span[999] == 0x42);
        CHECK(span[1000] == 0xFF);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// CAN Message Simulation Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("DataSpan CAN message patterns") {
    SUBCASE("8-byte CAN frame") {
        u8 can_data[] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};
        DataSpan span(can_data, 8);

        CHECK(span.size() == 8);
        CHECK(span.get_u16_le(0) == 0x3412);
        CHECK(span.get_u32_le(4) == 0xF0DEBC9A);
        CHECK(span.get_u64_le(0) == 0xF0DEBC9A78563412ULL);
    }

    SUBCASE("variable length message") {
        u8 msg_data[] = {0x01, 0x02, 0x03};
        DataSpan span(msg_data, 3);

        auto sub = span.subspan(1);
        CHECK(sub.size() == 2);
        CHECK(sub[0] == 0x02);
    }

    SUBCASE("multi-byte values extraction") {
        // Simulate extracting RPM (2 bytes) and speed (2 bytes)
        u8 data[] = {0xE8, 0x03, 0x64, 0x00}; // RPM: 1000, Speed: 100
        DataSpan span(data, 4);

        u16 rpm = span.get_u16_le(0);
        u16 speed = span.get_u16_le(2);

        CHECK(rpm == 1000);
        CHECK(speed == 100);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Bounds Safety Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("DataSpan bounds safety") {
    u8 data[] = {0x01, 0x02};
    DataSpan span(data, 2);

    SUBCASE("reading beyond size is safe") {
        CHECK(span[2] == 0xFF);
        CHECK(span[100] == 0xFF);
    }

    SUBCASE("get_u16_le beyond size is safe") {
        CHECK(span.get_u16_le(1) == 0xFFFF);
        CHECK(span.get_u16_le(2) == 0xFFFF);
    }

    SUBCASE("get_u32_le beyond size is safe") {
        CHECK(span.get_u32_le(0) == 0xFFFFFFFF);
    }

    SUBCASE("get_u64_le beyond size is safe") {
        CHECK(span.get_u64_le(0) == 0xFFFFFFFFFFFFFFFFULL);
    }

    SUBCASE("get_bit beyond size is safe") {
        CHECK(span.get_bit(2, 0) == false);
        CHECK(span.get_bit(0, 8) == false);
    }

    SUBCASE("subspan beyond size is safe") {
        auto sub = span.subspan(10);
        CHECK(sub.empty());
    }
}
