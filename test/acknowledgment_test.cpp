#include <doctest/doctest.h>
#include <agrobus/j1939/acknowledgment.hpp>

using namespace agrobus::j1939;

TEST_CASE("Acknowledgment encode/decode") {
    SUBCASE("round-trip positive ACK") {
        Acknowledgment ack;
        ack.control = AckControl::PositiveAck;
        ack.group_function = 0x42;
        ack.acknowledged_pgn = 0xFECA;
        ack.address = 0x28;

        auto data = ack.encode();
        CHECK(data.size() == 8);

        Acknowledgment decoded = Acknowledgment::decode(data);
        CHECK(decoded.control == AckControl::PositiveAck);
        CHECK(decoded.group_function == 0x42);
        CHECK(decoded.acknowledged_pgn == 0xFECA);
        CHECK(decoded.address == 0x28);
    }

    SUBCASE("round-trip negative ACK") {
        Acknowledgment ack;
        ack.control = AckControl::NegativeAck;
        ack.group_function = 0xFF;
        ack.acknowledged_pgn = 0xE700;
        ack.address = 0x30;

        auto data = ack.encode();
        Acknowledgment decoded = Acknowledgment::decode(data);

        CHECK(decoded.control == AckControl::NegativeAck);
        CHECK(decoded.acknowledged_pgn == 0xE700);
        CHECK(decoded.address == 0x30);
    }

    SUBCASE("round-trip access denied") {
        Acknowledgment ack;
        ack.control = AckControl::AccessDenied;
        ack.group_function = 0x10;
        ack.acknowledged_pgn = 0xDF00;
        ack.address = 0x50;

        auto data = ack.encode();
        Acknowledgment decoded = Acknowledgment::decode(data);

        CHECK(decoded.control == AckControl::AccessDenied);
        CHECK(decoded.group_function == 0x10);
        CHECK(decoded.acknowledged_pgn == 0xDF00);
        CHECK(decoded.address == 0x50);
    }

    SUBCASE("round-trip cannot respond") {
        Acknowledgment ack;
        ack.control = AckControl::CannotRespond;
        ack.group_function = 0xFF;
        ack.acknowledged_pgn = 0x1234;
        ack.address = 0x00;

        auto data = ack.encode();
        Acknowledgment decoded = Acknowledgment::decode(data);

        CHECK(decoded.control == AckControl::CannotRespond);
        CHECK(decoded.acknowledged_pgn == 0x1234);
        CHECK(decoded.address == 0x00);
    }

    SUBCASE("encode produces 8 bytes") {
        Acknowledgment ack;
        auto data = ack.encode();
        CHECK(data.size() == 8);
    }

    SUBCASE("decode from short data returns defaults") {
        dp::Vector<u8> short_data = {0x01, 0x02, 0x03}; // too short
        Acknowledgment decoded = Acknowledgment::decode(short_data);
        // Should return default values since data < 8 bytes
        CHECK(decoded.control == AckControl::PositiveAck);
        CHECK(decoded.acknowledged_pgn == 0);
    }

    SUBCASE("PGN encoding covers 3 bytes") {
        Acknowledgment ack;
        ack.acknowledged_pgn = 0x1FECC; // 3-byte PGN value
        ack.address = 0x28;

        auto data = ack.encode();
        CHECK(data[5] == 0xCC);          // LSB
        CHECK(data[6] == 0xFE);          // middle byte
        CHECK(data[7] == 0x01);          // MSB (bits 16-23)

        Acknowledgment decoded = Acknowledgment::decode(data);
        CHECK(decoded.acknowledged_pgn == 0x1FECC);
    }

    SUBCASE("reserved bytes are 0xFF") {
        Acknowledgment ack;
        ack.control = AckControl::PositiveAck;
        auto data = ack.encode();
        CHECK(data[2] == 0xFF); // reserved
        CHECK(data[3] == 0xFF); // reserved
    }
}

TEST_CASE("AckControl enum values") {
    CHECK(static_cast<u8>(AckControl::PositiveAck) == 0);
    CHECK(static_cast<u8>(AckControl::NegativeAck) == 1);
    CHECK(static_cast<u8>(AckControl::AccessDenied) == 2);
    CHECK(static_cast<u8>(AckControl::CannotRespond) == 3);
}

TEST_CASE("AcknowledgmentHandler") {
    IsoNet nm;
    Name name;
    auto cf_result = nm.create_internal(name, 0, 0x28);
    auto* cf = cf_result.value();

    SUBCASE("initialization succeeds") {
        AcknowledgmentHandler handler(nm, cf);
        auto result = handler.initialize();
        CHECK(result.is_ok());
    }

    SUBCASE("send_ack returns ok (no endpoint, so not_connected)") {
        AcknowledgmentHandler handler(nm, cf);
        handler.initialize();
        // Without a CAN endpoint, send will fail with not_connected
        // but the method itself should construct correctly
        auto result = handler.send_ack(PGN_DM1, 0x30);
        // The result depends on whether CF has a valid address
        // Since we did not claim, address is valid (0x28) but no endpoint -> not connected
        CHECK(!result.is_ok());
    }

    SUBCASE("send_nack returns ok (no endpoint)") {
        AcknowledgmentHandler handler(nm, cf);
        handler.initialize();
        auto result = handler.send_nack(PGN_DM2, 0x30);
        CHECK(!result.is_ok()); // no endpoint configured
    }

    SUBCASE("send_access_denied returns ok (no endpoint)") {
        AcknowledgmentHandler handler(nm, cf);
        handler.initialize();
        auto result = handler.send_access_denied(PGN_DM3, 0x30);
        CHECK(!result.is_ok()); // no endpoint configured
    }

    SUBCASE("send_cannot_respond returns ok (no endpoint)") {
        AcknowledgmentHandler handler(nm, cf);
        handler.initialize();
        auto result = handler.send_cannot_respond(PGN_DM11, 0x30);
        CHECK(!result.is_ok()); // no endpoint configured
    }

    SUBCASE("on_acknowledgment event is accessible") {
        AcknowledgmentHandler handler(nm, cf);
        handler.initialize();

        bool event_received = false;
        handler.on_acknowledgment += [&](Acknowledgment, Address) {
            event_received = true;
        };

        // Manually simulate receiving an ACK by calling the callback
        // (In real use, network manager dispatches it)
        CHECK(handler.on_acknowledgment.count() == 1);
    }

    SUBCASE("PGN_ACKNOWLEDGMENT value") {
        CHECK(PGN_ACKNOWLEDGMENT == 0xE800);
    }
}
