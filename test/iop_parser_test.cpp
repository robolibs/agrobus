#include <doctest/doctest.h>
#include <agrobus/net/iop_parser.hpp>

using namespace agrobus::net;
using namespace agrobus::isobus::vt;

TEST_CASE("IOPParser - hash_to_version") {
    dp::Vector<u8> data = {0x01, 0x02, 0x03, 0x04, 0x05};
    auto result = IOPParser::hash_to_version(data);
    CHECK(result.is_ok());
    CHECK(result.value().size() == 7);

    // Same data should produce same hash
    auto result2 = IOPParser::hash_to_version(data);
    CHECK(result.value() == result2.value());

    // Different data should produce different hash
    dp::Vector<u8> data2 = {0x05, 0x04, 0x03, 0x02, 0x01};
    auto result3 = IOPParser::hash_to_version(data2);
    CHECK(result.value() != result3.value());
}

TEST_CASE("IOPParser - validate empty data") {
    dp::Vector<u8> empty;
    auto result = IOPParser::validate(empty);
    CHECK(result.is_ok());
    CHECK_FALSE(result.value());
}

TEST_CASE("IOPParser - validate too small") {
    dp::Vector<u8> tiny = {0x01, 0x02, 0x03};
    auto result = IOPParser::validate(tiny);
    CHECK(result.is_ok());
    CHECK_FALSE(result.value());
}

TEST_CASE("IOPParser - parse minimal object") {
    // Create a minimal WorkingSet object:
    // ID=0x0001, Type=WorkingSet(0), Width=200, Height=200, + 4 bytes object data
    dp::Vector<u8> data = {
        0x01, 0x00,  // Object ID = 1
        0x00,        // Object Type = WorkingSet
        0xC8, 0x00,  // Width = 200
        0xC8, 0x00,  // Height = 200
        0x00,        // background_color
        0x01,        // selectable
        0x02, 0x00   // active_mask = 2
    };

    auto result = IOPParser::parse_iop_data(data);
    CHECK(result.is_ok());

    auto& pool = result.value();
    CHECK(pool.size() == 1);

    auto obj = pool.find(1);
    CHECK(obj.has_value());
    CHECK((*obj)->type == ObjectType::WorkingSet);
    // Width and height are stored as first 4 bytes of body
    CHECK((*obj)->body.size() >= 4);
    u16 w = static_cast<u16>((*obj)->body[0]) | (static_cast<u16>((*obj)->body[1]) << 8);
    u16 h = static_cast<u16>((*obj)->body[2]) | (static_cast<u16>((*obj)->body[3]) << 8);
    CHECK(w == 200);
    CHECK(h == 200);
}

TEST_CASE("IOPParser - parse multiple objects") {
    dp::Vector<u8> data;

    // WorkingSet (ID=1)
    dp::Vector<u8> ws = {0x01, 0x00, 0x00, 0xC8, 0x00, 0xC8, 0x00, 0x00, 0x01, 0x02, 0x00};
    data.insert(data.end(), ws.begin(), ws.end());

    // NumberVariable (ID=2) - 4 bytes value
    dp::Vector<u8> nv = {0x02, 0x00, 0x15, 0x00, 0x00, 0x00, 0x00, 0x2A, 0x00, 0x00, 0x00};
    data.insert(data.end(), nv.begin(), nv.end());

    auto result = IOPParser::parse_iop_data(data);
    CHECK(result.is_ok());
    CHECK(result.value().size() >= 1);
}

TEST_CASE("IOPParser - validate valid structure") {
    // WorkingSet object
    dp::Vector<u8> data = {0x01, 0x00, 0x00, 0xC8, 0x00, 0xC8, 0x00, 0x00, 0x01, 0x02, 0x00};

    auto result = IOPParser::validate(data);
    CHECK(result.is_ok());
    CHECK(result.value());
}

TEST_CASE("IOPParser - read_iop_file nonexistent") {
    auto result = IOPParser::read_iop_file("/nonexistent/path/test.iop");
    CHECK_FALSE(result.is_ok());
}
