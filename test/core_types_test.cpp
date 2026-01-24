#include <doctest/doctest.h>
#include <agrobus/net/types.hpp>
#include <agrobus/net/name.hpp>
#include <agrobus/net/identifier.hpp>
#include <agrobus/net/constants.hpp>

using namespace agrobus::net;

TEST_CASE("Name encoding/decoding") {
    SUBCASE("default name is zero") {
        Name n;
        CHECK(n.raw == 0);
        CHECK(n.identity_number() == 0);
        CHECK(n.manufacturer_code() == 0);
    }

    SUBCASE("set/get identity number") {
        Name n;
        n.set_identity_number(0x1ABCDE);
        CHECK(n.identity_number() == (0x1ABCDE & 0x1FFFFF));
    }

    SUBCASE("set/get manufacturer code") {
        Name n;
        n.set_manufacturer_code(0x2AB);
        CHECK(n.manufacturer_code() == 0x2AB);
    }

    SUBCASE("set/get function code") {
        Name n;
        n.set_function_code(0x42);
        CHECK(n.function_code() == 0x42);
    }

    SUBCASE("set/get device class") {
        Name n;
        n.set_device_class(25);
        CHECK(n.device_class() == 25);
    }

    SUBCASE("set/get industry group") {
        Name n;
        n.set_industry_group(2); // Agriculture
        CHECK(n.industry_group() == 2);
    }

    SUBCASE("self configurable bit") {
        Name n;
        CHECK(!n.self_configurable());
        n.set_self_configurable(true);
        CHECK(n.self_configurable());
    }

    SUBCASE("to_bytes and from_bytes roundtrip") {
        Name n;
        n.set_identity_number(12345);
        n.set_manufacturer_code(555);
        n.set_function_code(100);
        n.set_device_class(7);
        n.set_industry_group(2);
        n.set_self_configurable(true);

        auto bytes = n.to_bytes();
        Name reconstructed = Name::from_bytes(bytes.data());
        CHECK(reconstructed.raw == n.raw);
        CHECK(reconstructed.identity_number() == 12345);
        CHECK(reconstructed.manufacturer_code() == 555);
        CHECK(reconstructed.function_code() == 100);
    }

    SUBCASE("comparison (lower NAME wins)") {
        Name a(100);
        Name b(200);
        CHECK(a < b);
        CHECK(!(b < a));
    }

    SUBCASE("builder pattern") {
        Name n = Name::build()
            .set_identity_number(1)
            .set_manufacturer_code(2)
            .set_industry_group(3);
        CHECK(n.identity_number() == 1);
        CHECK(n.manufacturer_code() == 2);
        CHECK(n.industry_group() == 3);
    }
}

TEST_CASE("Identifier encode/decode") {
    SUBCASE("encode PDU2 broadcast") {
        auto id = Identifier::encode(Priority::Default, PGN_VEHICLE_SPEED, 0x28, BROADCAST_ADDRESS);
        CHECK(id.priority() == Priority::Default);
        CHECK(id.pgn() == PGN_VEHICLE_SPEED);
        CHECK(id.source() == 0x28);
        CHECK(id.is_broadcast());
    }

    SUBCASE("encode PDU1 destination-specific") {
        auto id = Identifier::encode(Priority::Normal, PGN_REQUEST, 0x10, 0x20);
        CHECK(id.priority() == Priority::Normal);
        CHECK(id.pgn() == PGN_REQUEST);
        CHECK(id.source() == 0x10);
        CHECK(id.destination() == 0x20);
        CHECK(!id.is_broadcast());
    }

    SUBCASE("PDU format >= 240 is broadcast") {
        auto id = Identifier::encode(Priority::Default, PGN_DM1, 0x00, BROADCAST_ADDRESS);
        CHECK(id.is_broadcast());
        CHECK(id.is_pdu2());
    }

    SUBCASE("roundtrip encode/decode") {
        auto id = Identifier::encode(Priority::High, 0xFE49, 0x42, BROADCAST_ADDRESS);
        CHECK(id.priority() == Priority::High);
        CHECK(id.pgn() == 0xFE49);
        CHECK(id.source() == 0x42);
    }

    SUBCASE("data page bit") {
        Identifier id(0x09FF0028); // data page = 1 (bit 24 set)
        CHECK(id.data_page());
        Identifier id2(0x18FF0028); // data page = 0
        CHECK(!id2.data_page());
    }
}

TEST_CASE("Priority values") {
    CHECK(static_cast<u8>(Priority::Highest) == 0);
    CHECK(static_cast<u8>(Priority::Default) == 6);
    CHECK(static_cast<u8>(Priority::Lowest) == 7);
}

TEST_CASE("Address constants") {
    CHECK(NULL_ADDRESS == 0xFE);
    CHECK(BROADCAST_ADDRESS == 0xFF);
    CHECK(MAX_ADDRESS == 0xFD);
}
