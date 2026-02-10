#include <doctest/doctest.h>
#include <agrobus/j1939/maintain_power.hpp>

using namespace agrobus::j1939;
using namespace agrobus::net;

// ═════════════════════════════════════════════════════════════════════════════
// MaintainPowerData Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("MaintainPowerData encode/decode") {
    SUBCASE("basic encoding") {
        MaintainPowerData mpd;
        mpd.key_switch = KeySwitchState::NotOff;
        mpd.maintain_request = MaintainPowerRequest::ECURequest;
        mpd.max_time_min = 5;

        auto encoded = mpd.encode();
        CHECK(encoded.size() == 8);
        CHECK(encoded[0] == 0x05); // key_switch (1) | (maintain_request (1) << 2)
        CHECK(encoded[1] == 5);     // max_time_min
    }

    SUBCASE("basic decoding") {
        dp::Vector<u8> data(8, 0xFF);
        data[0] = 0x05; // key_switch=1, maintain_request=1
        data[1] = 10;

        auto mpd = MaintainPowerData::decode(data);
        CHECK(mpd.key_switch == KeySwitchState::NotOff);
        CHECK(mpd.maintain_request == MaintainPowerRequest::ECURequest);
        CHECK(mpd.max_time_min == 10);
    }

    SUBCASE("all key switch states") {
        MaintainPowerData mpd;
        mpd.maintain_request = MaintainPowerRequest::NoRequest;
        mpd.max_time_min = 0;

        mpd.key_switch = KeySwitchState::Off;
        CHECK(mpd.encode()[0] == 0x00);

        mpd.key_switch = KeySwitchState::NotOff;
        CHECK(mpd.encode()[0] == 0x01);

        mpd.key_switch = KeySwitchState::Error;
        CHECK(mpd.encode()[0] == 0x02);

        mpd.key_switch = KeySwitchState::NotAvailable;
        CHECK(mpd.encode()[0] == 0x03);
    }

    SUBCASE("all maintain request states") {
        MaintainPowerData mpd;
        mpd.key_switch = KeySwitchState::Off;
        mpd.max_time_min = 0;

        mpd.maintain_request = MaintainPowerRequest::NoRequest;
        CHECK(mpd.encode()[0] == 0x00);

        mpd.maintain_request = MaintainPowerRequest::ECURequest;
        CHECK(mpd.encode()[0] == 0x04);

        mpd.maintain_request = MaintainPowerRequest::Error;
        CHECK(mpd.encode()[0] == 0x08);

        mpd.maintain_request = MaintainPowerRequest::NotAvailable;
        CHECK(mpd.encode()[0] == 0x0C);
    }

    SUBCASE("combined states") {
        MaintainPowerData mpd;
        mpd.key_switch = KeySwitchState::NotOff;
        mpd.maintain_request = MaintainPowerRequest::ECURequest;
        mpd.max_time_min = 15;

        auto encoded = mpd.encode();
        auto decoded = MaintainPowerData::decode(encoded);

        CHECK(decoded.key_switch == KeySwitchState::NotOff);
        CHECK(decoded.maintain_request == MaintainPowerRequest::ECURequest);
        CHECK(decoded.max_time_min == 15);
    }

    SUBCASE("encode/decode round-trip") {
        MaintainPowerData original;
        original.key_switch = KeySwitchState::Error;
        original.maintain_request = MaintainPowerRequest::NoRequest;
        original.max_time_min = 3;

        auto encoded = original.encode();
        auto decoded = MaintainPowerData::decode(encoded);

        CHECK(decoded.key_switch == original.key_switch);
        CHECK(decoded.maintain_request == original.maintain_request);
        CHECK(decoded.max_time_min == original.max_time_min);
    }

    SUBCASE("short data handling") {
        dp::Vector<u8> short_data(1, 0xFF);
        auto mpd = MaintainPowerData::decode(short_data);

        // Should use default values for missing data
        CHECK(mpd.key_switch == KeySwitchState::NotAvailable);
        CHECK(mpd.maintain_request == MaintainPowerRequest::NotAvailable);
    }

    SUBCASE("not available encoding") {
        MaintainPowerData mpd;
        // Default values should encode to 0xFF for byte 1
        auto encoded = mpd.encode();
        CHECK(encoded[1] == 0xFF);
    }

    SUBCASE("maximum time boundary") {
        MaintainPowerData mpd;
        mpd.max_time_min = 0;
        CHECK(mpd.encode()[1] == 0);

        mpd.max_time_min = 180; // 3 hours
        CHECK(mpd.encode()[1] == 180);

        mpd.max_time_min = 0xFF; // Not available
        CHECK(mpd.encode()[1] == 0xFF);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// PowerState Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("PowerState enumerations") {
    SUBCASE("power state values") {
        CHECK(static_cast<u8>(PowerState::Running) == 0);
        CHECK(static_cast<u8>(PowerState::ShutdownPending) == 1);
        CHECK(static_cast<u8>(PowerState::Maintaining) == 2);
        CHECK(static_cast<u8>(PowerState::PowerOff) == 3);
    }

    SUBCASE("key switch state values") {
        CHECK(static_cast<u8>(KeySwitchState::Off) == 0);
        CHECK(static_cast<u8>(KeySwitchState::NotOff) == 1);
        CHECK(static_cast<u8>(KeySwitchState::Error) == 2);
        CHECK(static_cast<u8>(KeySwitchState::NotAvailable) == 3);
    }

    SUBCASE("maintain power request values") {
        CHECK(static_cast<u8>(MaintainPowerRequest::NoRequest) == 0);
        CHECK(static_cast<u8>(MaintainPowerRequest::ECURequest) == 1);
        CHECK(static_cast<u8>(MaintainPowerRequest::Error) == 2);
        CHECK(static_cast<u8>(MaintainPowerRequest::NotAvailable) == 3);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Bit Packing Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("MaintainPowerData bit packing") {
    SUBCASE("verify bit positions") {
        // Byte 0: bits 0-1 = key_switch, bits 2-3 = maintain_request
        MaintainPowerData mpd;
        mpd.key_switch = KeySwitchState::NotOff;        // 0b01
        mpd.maintain_request = MaintainPowerRequest::ECURequest; // 0b01
        mpd.max_time_min = 0;

        auto encoded = mpd.encode();
        // Expected: 0b00000101 = 0x05
        CHECK(encoded[0] == 0x05);
    }

    SUBCASE("verify bit mask isolation") {
        // Ensure fields don't interfere with each other
        dp::Vector<u8> data(8, 0xFF);
        data[0] = 0x0F; // All bits set in relevant fields

        auto mpd = MaintainPowerData::decode(data);
        CHECK(mpd.key_switch == KeySwitchState::NotAvailable);  // 0b11
        CHECK(mpd.maintain_request == MaintainPowerRequest::NotAvailable); // 0b11
    }

    SUBCASE("verify bit field boundaries") {
        // Test that setting max values doesn't overflow
        MaintainPowerData mpd;
        mpd.key_switch = KeySwitchState::NotAvailable;        // 0b11 (max 2 bits)
        mpd.maintain_request = MaintainPowerRequest::NotAvailable; // 0b11 (max 2 bits)

        auto encoded = mpd.encode();
        CHECK((encoded[0] & 0x03) == 0x03); // Key switch bits
        CHECK(((encoded[0] >> 2) & 0x03) == 0x03); // Maintain request bits
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Edge Case Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("MaintainPowerData edge cases") {
    SUBCASE("zero-initialized data") {
        dp::Vector<u8> zeros(8, 0x00);
        auto mpd = MaintainPowerData::decode(zeros);

        CHECK(mpd.key_switch == KeySwitchState::Off);
        CHECK(mpd.maintain_request == MaintainPowerRequest::NoRequest);
        CHECK(mpd.max_time_min == 0);
    }

    SUBCASE("all-ones data") {
        dp::Vector<u8> ones(8, 0xFF);
        auto mpd = MaintainPowerData::decode(ones);

        CHECK(mpd.key_switch == KeySwitchState::NotAvailable);
        CHECK(mpd.maintain_request == MaintainPowerRequest::NotAvailable);
        CHECK(mpd.max_time_min == 0xFF);
    }

    SUBCASE("empty data") {
        dp::Vector<u8> empty;
        auto mpd = MaintainPowerData::decode(empty);

        // Should handle gracefully with defaults
        CHECK(mpd.key_switch == KeySwitchState::NotAvailable);
        CHECK(mpd.maintain_request == MaintainPowerRequest::NotAvailable);
    }

    SUBCASE("excess data ignored") {
        dp::Vector<u8> excess(16, 0xAA);
        excess[0] = 0x05;
        excess[1] = 10;

        auto mpd = MaintainPowerData::decode(excess);
        CHECK(mpd.key_switch == KeySwitchState::NotOff);
        CHECK(mpd.maintain_request == MaintainPowerRequest::ECURequest);
        CHECK(mpd.max_time_min == 10);
    }
}
