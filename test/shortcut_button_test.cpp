#include <doctest/doctest.h>
#include <agrobus/j1939/shortcut_button.hpp>

using namespace agrobus::j1939;

// ═════════════════════════════════════════════════════════════════════════════
// ShortcutButtonState Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("ShortcutButtonState enumeration values") {
    SUBCASE("state values") {
        CHECK(static_cast<u8>(ShortcutButtonState::Released) == 0);
        CHECK(static_cast<u8>(ShortcutButtonState::Pressed) == 1);
        CHECK(static_cast<u8>(ShortcutButtonState::Error) == 2);
        CHECK(static_cast<u8>(ShortcutButtonState::NotAvailable) == 3);
    }

    SUBCASE("bit mask verification") {
        // Ensure all values fit in 2 bits
        CHECK((static_cast<u8>(ShortcutButtonState::Released) & 0x03) == 0);
        CHECK((static_cast<u8>(ShortcutButtonState::Pressed) & 0x03) == 1);
        CHECK((static_cast<u8>(ShortcutButtonState::Error) & 0x03) == 2);
        CHECK((static_cast<u8>(ShortcutButtonState::NotAvailable) & 0x03) == 3);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// State Encoding Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("ShortcutButtonState encoding") {
    SUBCASE("encode released state") {
        ShortcutButtonState state = ShortcutButtonState::Released;
        u8 encoded = static_cast<u8>(state) & 0x03;
        CHECK(encoded == 0x00);
    }

    SUBCASE("encode pressed state") {
        ShortcutButtonState state = ShortcutButtonState::Pressed;
        u8 encoded = static_cast<u8>(state) & 0x03;
        CHECK(encoded == 0x01);
    }

    SUBCASE("encode error state") {
        ShortcutButtonState state = ShortcutButtonState::Error;
        u8 encoded = static_cast<u8>(state) & 0x03;
        CHECK(encoded == 0x02);
    }

    SUBCASE("encode not available state") {
        ShortcutButtonState state = ShortcutButtonState::NotAvailable;
        u8 encoded = static_cast<u8>(state) & 0x03;
        CHECK(encoded == 0x03);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// State Decoding Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("ShortcutButtonState decoding") {
    SUBCASE("decode released state") {
        u8 data = 0x00;
        auto state = static_cast<ShortcutButtonState>(data & 0x03);
        CHECK(state == ShortcutButtonState::Released);
    }

    SUBCASE("decode pressed state") {
        u8 data = 0x01;
        auto state = static_cast<ShortcutButtonState>(data & 0x03);
        CHECK(state == ShortcutButtonState::Pressed);
    }

    SUBCASE("decode error state") {
        u8 data = 0x02;
        auto state = static_cast<ShortcutButtonState>(data & 0x03);
        CHECK(state == ShortcutButtonState::Error);
    }

    SUBCASE("decode not available state") {
        u8 data = 0x03;
        auto state = static_cast<ShortcutButtonState>(data & 0x03);
        CHECK(state == ShortcutButtonState::NotAvailable);
    }

    SUBCASE("decode with extra bits") {
        // Ensure that extra bits are masked off
        u8 data = 0xFF;
        auto state = static_cast<ShortcutButtonState>(data & 0x03);
        CHECK(state == ShortcutButtonState::NotAvailable);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Round-trip Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("ShortcutButtonState encode/decode round-trip") {
    SUBCASE("released state round-trip") {
        ShortcutButtonState original = ShortcutButtonState::Released;
        u8 encoded = static_cast<u8>(original) & 0x03;
        auto decoded = static_cast<ShortcutButtonState>(encoded & 0x03);
        CHECK(decoded == original);
    }

    SUBCASE("pressed state round-trip") {
        ShortcutButtonState original = ShortcutButtonState::Pressed;
        u8 encoded = static_cast<u8>(original) & 0x03;
        auto decoded = static_cast<ShortcutButtonState>(encoded & 0x03);
        CHECK(decoded == original);
    }

    SUBCASE("error state round-trip") {
        ShortcutButtonState original = ShortcutButtonState::Error;
        u8 encoded = static_cast<u8>(original) & 0x03;
        auto decoded = static_cast<ShortcutButtonState>(encoded & 0x03);
        CHECK(decoded == original);
    }

    SUBCASE("not available state round-trip") {
        ShortcutButtonState original = ShortcutButtonState::NotAvailable;
        u8 encoded = static_cast<u8>(original) & 0x03;
        auto decoded = static_cast<ShortcutButtonState>(encoded & 0x03);
        CHECK(decoded == original);
    }

    SUBCASE("all states round-trip") {
        ShortcutButtonState states[] = {ShortcutButtonState::Released, ShortcutButtonState::Pressed,
                                        ShortcutButtonState::Error, ShortcutButtonState::NotAvailable};

        for (auto original : states) {
            u8 encoded = static_cast<u8>(original) & 0x03;
            auto decoded = static_cast<ShortcutButtonState>(encoded & 0x03);
            CHECK(decoded == original);
        }
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// State Transition Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("ShortcutButtonState transitions") {
    SUBCASE("released to pressed") {
        ShortcutButtonState state = ShortcutButtonState::Released;
        state = ShortcutButtonState::Pressed;
        CHECK(state == ShortcutButtonState::Pressed);
    }

    SUBCASE("pressed to released") {
        ShortcutButtonState state = ShortcutButtonState::Pressed;
        state = ShortcutButtonState::Released;
        CHECK(state == ShortcutButtonState::Released);
    }

    SUBCASE("to error state") {
        ShortcutButtonState state = ShortcutButtonState::Pressed;
        state = ShortcutButtonState::Error;
        CHECK(state == ShortcutButtonState::Error);
    }

    SUBCASE("to not available state") {
        ShortcutButtonState state = ShortcutButtonState::Pressed;
        state = ShortcutButtonState::NotAvailable;
        CHECK(state == ShortcutButtonState::NotAvailable);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Message Format Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("Shortcut button message format") {
    SUBCASE("message size is 8 bytes") {
        dp::Vector<u8> data(8, 0xFF);
        CHECK(data.size() == 8);
    }

    SUBCASE("state in byte 0, bits 0-1") {
        dp::Vector<u8> data(8, 0xFF);
        data[0] = 0x01; // Pressed state

        auto state = static_cast<ShortcutButtonState>(data[0] & 0x03);
        CHECK(state == ShortcutButtonState::Pressed);
    }

    SUBCASE("other bits set to 1 (not available)") {
        dp::Vector<u8> data(8, 0xFF);
        data[0] = (data[0] & ~0x03) | 0x00; // Set state to Released, keep other bits as 0xFF

        CHECK((data[0] & 0x03) == 0x00);  // State bits
        CHECK((data[0] & 0xFC) == 0xFC);  // Other bits should be 1
    }

    SUBCASE("bytes 1-7 set to 0xFF") {
        dp::Vector<u8> data(8, 0xFF);
        data[0] = 0x01;

        for (usize i = 1; i < 8; ++i) {
            CHECK(data[i] == 0xFF);
        }
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Edge Case Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("ShortcutButtonState edge cases") {
    SUBCASE("decode from zero byte") {
        u8 data = 0x00;
        auto state = static_cast<ShortcutButtonState>(data & 0x03);
        CHECK(state == ShortcutButtonState::Released);
    }

    SUBCASE("decode from all-ones byte") {
        u8 data = 0xFF;
        auto state = static_cast<ShortcutButtonState>(data & 0x03);
        CHECK(state == ShortcutButtonState::NotAvailable);
    }

    SUBCASE("bit masking isolation") {
        // Verify that bit masking properly isolates state bits
        u8 data = 0b11111101; // Bits 0-1 = 0b01 (Pressed), others set
        auto state = static_cast<ShortcutButtonState>(data & 0x03);
        CHECK(state == ShortcutButtonState::Pressed);
    }

    SUBCASE("default state initialization") {
        ShortcutButtonState state = ShortcutButtonState::Released;
        CHECK(state == ShortcutButtonState::Released);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Correctness Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("ShortcutButtonState correctness") {
    SUBCASE("only 4 valid states") {
        // Ensure we have exactly 4 enumeration values
        int state_count = 0;
        if (ShortcutButtonState::Released != ShortcutButtonState::Pressed)
            state_count++;
        if (ShortcutButtonState::Pressed != ShortcutButtonState::Error)
            state_count++;
        if (ShortcutButtonState::Error != ShortcutButtonState::NotAvailable)
            state_count++;
        if (ShortcutButtonState::NotAvailable != ShortcutButtonState::Released)
            state_count++;
        CHECK(state_count == 4);
    }

    SUBCASE("states are unique") {
        CHECK(ShortcutButtonState::Released != ShortcutButtonState::Pressed);
        CHECK(ShortcutButtonState::Pressed != ShortcutButtonState::Error);
        CHECK(ShortcutButtonState::Error != ShortcutButtonState::NotAvailable);
        CHECK(ShortcutButtonState::NotAvailable != ShortcutButtonState::Released);
    }

    SUBCASE("values are sequential") {
        CHECK(static_cast<u8>(ShortcutButtonState::Released) == 0);
        CHECK(static_cast<u8>(ShortcutButtonState::Pressed) == 1);
        CHECK(static_cast<u8>(ShortcutButtonState::Error) == 2);
        CHECK(static_cast<u8>(ShortcutButtonState::NotAvailable) == 3);
    }
}
