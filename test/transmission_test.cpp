#include <doctest/doctest.h>
#include <agrobus/j1939/transmission.hpp>

using namespace agrobus::j1939;

// ═════════════════════════════════════════════════════════════════════════════
// ETC1 (Electronic Transmission Controller 1) Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("ETC1 encode and decode") {
    SUBCASE("typical values") {
        ETC1 msg;
        msg.current_gear = 5;
        msg.selected_gear = 5;
        msg.output_shaft_speed_rpm = 1500.0;
        msg.shift_in_progress = 0x00;          // Not shifting
        msg.torque_converter_lockup = 0x01;     // Locked

        auto encoded = msg.encode();
        CHECK(encoded.size() == 8);

        auto decoded = ETC1::decode(encoded);
        CHECK(decoded.current_gear == 5);
        CHECK(decoded.selected_gear == 5);
        CHECK(decoded.output_shaft_speed_rpm == doctest::Approx(1500.0).epsilon(0.125));
        CHECK(decoded.shift_in_progress == 0x00);
        CHECK(decoded.torque_converter_lockup == 0x01);
    }

    SUBCASE("gear offset - negative gear") {
        ETC1 msg;
        msg.current_gear = -1;   // Reverse gear
        msg.selected_gear = -1;

        auto encoded = msg.encode();
        auto decoded = ETC1::decode(encoded);

        CHECK(decoded.current_gear == -1);
        CHECK(decoded.selected_gear == -1);
    }

    SUBCASE("gear offset - neutral") {
        ETC1 msg;
        msg.current_gear = 0;    // Neutral
        msg.selected_gear = 0;

        auto encoded = msg.encode();
        auto decoded = ETC1::decode(encoded);

        CHECK(decoded.current_gear == 0);
        CHECK(decoded.selected_gear == 0);
    }

    SUBCASE("gear offset - high gear") {
        ETC1 msg;
        msg.current_gear = 18;   // 18-speed transmission
        msg.selected_gear = 18;

        auto encoded = msg.encode();
        auto decoded = ETC1::decode(encoded);

        CHECK(decoded.current_gear == 18);
        CHECK(decoded.selected_gear == 18);
    }

    SUBCASE("shift in progress") {
        ETC1 msg;
        msg.current_gear = 5;
        msg.selected_gear = 6;
        msg.shift_in_progress = 0x01;  // Shifting
        msg.output_shaft_speed_rpm = 1200.0;

        auto encoded = msg.encode();
        auto decoded = ETC1::decode(encoded);

        CHECK(decoded.current_gear == 5);
        CHECK(decoded.selected_gear == 6);
        CHECK(decoded.shift_in_progress == 0x01);
    }

    SUBCASE("torque converter states") {
        SUBCASE("unlocked") {
            ETC1 msg;
            msg.torque_converter_lockup = 0x00;

            auto encoded = msg.encode();
            auto decoded = ETC1::decode(encoded);

            CHECK(decoded.torque_converter_lockup == 0x00);
        }

        SUBCASE("locked") {
            ETC1 msg;
            msg.torque_converter_lockup = 0x01;

            auto encoded = msg.encode();
            auto decoded = ETC1::decode(encoded);

            CHECK(decoded.torque_converter_lockup == 0x01);
        }

        SUBCASE("error") {
            ETC1 msg;
            msg.torque_converter_lockup = 0x02;

            auto encoded = msg.encode();
            auto decoded = ETC1::decode(encoded);

            CHECK(decoded.torque_converter_lockup == 0x02);
        }

        SUBCASE("not available") {
            ETC1 msg;
            msg.torque_converter_lockup = 0x03;

            auto encoded = msg.encode();
            auto decoded = ETC1::decode(encoded);

            CHECK(decoded.torque_converter_lockup == 0x03);
        }
    }

    SUBCASE("zero output shaft speed") {
        ETC1 msg;
        msg.output_shaft_speed_rpm = 0.0;
        msg.current_gear = 0;

        auto encoded = msg.encode();
        auto decoded = ETC1::decode(encoded);

        CHECK(decoded.output_shaft_speed_rpm == doctest::Approx(0.0));
    }

    SUBCASE("high output shaft speed") {
        ETC1 msg;
        msg.output_shaft_speed_rpm = 4000.0;

        auto encoded = msg.encode();
        auto decoded = ETC1::decode(encoded);

        CHECK(decoded.output_shaft_speed_rpm == doctest::Approx(4000.0).epsilon(0.125));
    }

    SUBCASE("speed resolution - 0.125 rpm/bit") {
        ETC1 msg;
        msg.output_shaft_speed_rpm = 1500.375;  // Should round to nearest 0.125

        auto encoded = msg.encode();
        auto decoded = ETC1::decode(encoded);

        CHECK(decoded.output_shaft_speed_rpm == doctest::Approx(1500.375).epsilon(0.125));
    }

    SUBCASE("bit packing - shift and lockup in byte 0") {
        ETC1 msg;
        msg.shift_in_progress = 0x02;          // Bits 0-1
        msg.torque_converter_lockup = 0x01;     // Bits 2-3

        auto encoded = msg.encode();

        // Check bit packing: shift_in_progress (bits 0-1) | torque_converter_lockup (bits 2-3)
        CHECK((encoded[0] & 0x03) == 0x02);        // Bits 0-1
        CHECK(((encoded[0] >> 2) & 0x03) == 0x01); // Bits 2-3

        auto decoded = ETC1::decode(encoded);
        CHECK(decoded.shift_in_progress == 0x02);
        CHECK(decoded.torque_converter_lockup == 0x01);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// TransmissionOilTemp Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("TransmissionOilTemp encode and decode") {
    SUBCASE("typical operating temperature") {
        TransmissionOilTemp msg;
        msg.oil_temp_c = 85.0;  // Normal operating temperature

        auto encoded = msg.encode();
        CHECK(encoded.size() == 8);

        auto decoded = TransmissionOilTemp::decode(encoded);
        CHECK(decoded.oil_temp_c == doctest::Approx(85.0).epsilon(0.03125));
    }

    SUBCASE("cold temperature") {
        TransmissionOilTemp msg;
        msg.oil_temp_c = -20.0;

        auto encoded = msg.encode();
        auto decoded = TransmissionOilTemp::decode(encoded);

        CHECK(decoded.oil_temp_c == doctest::Approx(-20.0).epsilon(0.03125));
    }

    SUBCASE("minimum temperature - absolute zero offset") {
        TransmissionOilTemp msg;
        msg.oil_temp_c = -40.0;  // Default value

        auto encoded = msg.encode();
        auto decoded = TransmissionOilTemp::decode(encoded);

        CHECK(decoded.oil_temp_c == doctest::Approx(-40.0).epsilon(0.03125));
    }

    SUBCASE("high temperature - overheating") {
        TransmissionOilTemp msg;
        msg.oil_temp_c = 150.0;

        auto encoded = msg.encode();
        auto decoded = TransmissionOilTemp::decode(encoded);

        CHECK(decoded.oil_temp_c == doctest::Approx(150.0).epsilon(0.03125));
    }

    SUBCASE("zero celsius") {
        TransmissionOilTemp msg;
        msg.oil_temp_c = 0.0;

        auto encoded = msg.encode();
        auto decoded = TransmissionOilTemp::decode(encoded);

        CHECK(decoded.oil_temp_c == doctest::Approx(0.0).epsilon(0.03125));
    }

    SUBCASE("temperature resolution - 0.03125 C/bit") {
        TransmissionOilTemp msg;
        msg.oil_temp_c = 85.5;

        auto encoded = msg.encode();
        auto decoded = TransmissionOilTemp::decode(encoded);

        // 0.03125 resolution means values should be within 0.03125 of original
        CHECK(decoded.oil_temp_c == doctest::Approx(85.5).epsilon(0.03125));
    }

    SUBCASE("offset encoding - -273 offset") {
        // Temperature is stored with offset -273 (to accommodate absolute zero)
        TransmissionOilTemp msg;
        msg.oil_temp_c = 100.0;

        auto encoded = msg.encode();

        // Check raw encoding: (temp + 273) / 0.03125
        u16 expected_raw = static_cast<u16>((100.0 + 273.0) / 0.03125);
        u16 actual_raw = static_cast<u16>(encoded[0]) | (static_cast<u16>(encoded[1]) << 8);
        CHECK(actual_raw == expected_raw);

        auto decoded = TransmissionOilTemp::decode(encoded);
        CHECK(decoded.oil_temp_c == doctest::Approx(100.0).epsilon(0.03125));
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// CruiseControl (CCVS) Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("CruiseControl encode and decode") {
    SUBCASE("typical highway cruise") {
        CruiseControl msg;
        msg.wheel_speed_kmh = 100.0;
        msg.cc_active = 0x01;        // Cruise control on
        msg.brake_switch = 0x00;     // Not pressed
        msg.clutch_switch = 0x00;    // Not pressed
        msg.park_brake = 0x00;       // Released
        msg.cc_set_speed_kmh = 100.0;

        auto encoded = msg.encode();
        CHECK(encoded.size() == 8);

        auto decoded = CruiseControl::decode(encoded);
        CHECK(decoded.wheel_speed_kmh == doctest::Approx(100.0).epsilon(1.0 / 256.0));
        CHECK(decoded.cc_active == 0x01);
        CHECK(decoded.brake_switch == 0x00);
        CHECK(decoded.clutch_switch == 0x00);
        CHECK(decoded.park_brake == 0x00);
        CHECK(decoded.cc_set_speed_kmh == doctest::Approx(100.0).epsilon(1.0 / 256.0));
    }

    SUBCASE("cruise control off") {
        CruiseControl msg;
        msg.wheel_speed_kmh = 80.0;
        msg.cc_active = 0x00;  // Off

        auto encoded = msg.encode();
        auto decoded = CruiseControl::decode(encoded);

        CHECK(decoded.wheel_speed_kmh == doctest::Approx(80.0).epsilon(1.0 / 256.0));
        CHECK(decoded.cc_active == 0x00);
    }

    SUBCASE("brake switch pressed") {
        CruiseControl msg;
        msg.wheel_speed_kmh = 60.0;
        msg.cc_active = 0x01;
        msg.brake_switch = 0x01;  // Pressed

        auto encoded = msg.encode();
        auto decoded = CruiseControl::decode(encoded);

        CHECK(decoded.brake_switch == 0x01);
        CHECK(decoded.cc_active == 0x01);
    }

    SUBCASE("clutch switch pressed") {
        CruiseControl msg;
        msg.wheel_speed_kmh = 50.0;
        msg.cc_active = 0x01;
        msg.clutch_switch = 0x01;  // Pressed

        auto encoded = msg.encode();
        auto decoded = CruiseControl::decode(encoded);

        CHECK(decoded.clutch_switch == 0x01);
    }

    SUBCASE("park brake engaged") {
        CruiseControl msg;
        msg.wheel_speed_kmh = 0.0;
        msg.park_brake = 0x01;  // Engaged

        auto encoded = msg.encode();
        auto decoded = CruiseControl::decode(encoded);

        CHECK(decoded.park_brake == 0x01);
        CHECK(decoded.wheel_speed_kmh == doctest::Approx(0.0).epsilon(1.0 / 256.0));
    }

    SUBCASE("all switches not available") {
        CruiseControl msg;
        msg.wheel_speed_kmh = 70.0;
        msg.cc_active = 0x03;        // Not available
        msg.brake_switch = 0x03;     // Not available
        msg.clutch_switch = 0x03;    // Not available
        msg.park_brake = 0x03;       // Not available

        auto encoded = msg.encode();
        auto decoded = CruiseControl::decode(encoded);

        CHECK(decoded.cc_active == 0x03);
        CHECK(decoded.brake_switch == 0x03);
        CHECK(decoded.clutch_switch == 0x03);
        CHECK(decoded.park_brake == 0x03);
    }

    SUBCASE("zero speed") {
        CruiseControl msg;
        msg.wheel_speed_kmh = 0.0;
        msg.cc_set_speed_kmh = 0.0;

        auto encoded = msg.encode();
        auto decoded = CruiseControl::decode(encoded);

        CHECK(decoded.wheel_speed_kmh == doctest::Approx(0.0).epsilon(1.0 / 256.0));
        CHECK(decoded.cc_set_speed_kmh == doctest::Approx(0.0).epsilon(1.0 / 256.0));
    }

    SUBCASE("high speed") {
        CruiseControl msg;
        msg.wheel_speed_kmh = 250.0;
        msg.cc_set_speed_kmh = 250.0;

        auto encoded = msg.encode();
        auto decoded = CruiseControl::decode(encoded);

        CHECK(decoded.wheel_speed_kmh == doctest::Approx(250.0).epsilon(1.0 / 256.0));
        CHECK(decoded.cc_set_speed_kmh == doctest::Approx(250.0).epsilon(1.0 / 256.0));
    }

    SUBCASE("speed resolution - 1/256 km/h per bit") {
        CruiseControl msg;
        msg.wheel_speed_kmh = 88.5;
        msg.cc_set_speed_kmh = 88.5;

        auto encoded = msg.encode();
        auto decoded = CruiseControl::decode(encoded);

        // Resolution is 1/256 km/h = ~0.00391 km/h
        CHECK(decoded.wheel_speed_kmh == doctest::Approx(88.5).epsilon(1.0 / 256.0));
        CHECK(decoded.cc_set_speed_kmh == doctest::Approx(88.5).epsilon(1.0 / 256.0));
    }

    SUBCASE("bit packing in byte 2") {
        CruiseControl msg;
        msg.cc_active = 0x01;        // Bits 0-1
        msg.brake_switch = 0x02;     // Bits 2-3
        msg.clutch_switch = 0x01;    // Bits 4-5
        msg.park_brake = 0x02;       // Bits 6-7

        auto encoded = msg.encode();

        // Check bit packing in byte 2
        CHECK((encoded[2] & 0x03) == 0x01);        // Bits 0-1: cc_active
        CHECK(((encoded[2] >> 2) & 0x03) == 0x02); // Bits 2-3: brake_switch
        CHECK(((encoded[2] >> 4) & 0x03) == 0x01); // Bits 4-5: clutch_switch
        CHECK(((encoded[2] >> 6) & 0x03) == 0x02); // Bits 6-7: park_brake

        auto decoded = CruiseControl::decode(encoded);
        CHECK(decoded.cc_active == 0x01);
        CHECK(decoded.brake_switch == 0x02);
        CHECK(decoded.clutch_switch == 0x01);
        CHECK(decoded.park_brake == 0x02);
    }

    SUBCASE("set speed different from current speed") {
        CruiseControl msg;
        msg.wheel_speed_kmh = 95.0;   // Accelerating to set speed
        msg.cc_set_speed_kmh = 100.0;
        msg.cc_active = 0x01;

        auto encoded = msg.encode();
        auto decoded = CruiseControl::decode(encoded);

        CHECK(decoded.wheel_speed_kmh == doctest::Approx(95.0).epsilon(1.0 / 256.0));
        CHECK(decoded.cc_set_speed_kmh == doctest::Approx(100.0).epsilon(1.0 / 256.0));
        CHECK(decoded.cc_active == 0x01);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Round-trip Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("Transmission messages round-trip") {
    SUBCASE("ETC1 various gear configurations") {
        struct TestCase {
            i8 current_gear;
            i8 selected_gear;
            f64 speed;
        };

        dp::Vector<TestCase> cases = {
            {-1, -1, 500.0},   // Reverse
            {0, 0, 0.0},       // Neutral stopped
            {0, 1, 100.0},     // Neutral coasting, selecting 1st
            {1, 1, 800.0},     // 1st gear
            {5, 5, 2000.0},    // 5th gear highway
            {10, 10, 1500.0},  // 10th gear (heavy truck)
            {18, 18, 1200.0},  // 18th gear (18-speed)
        };

        for (const auto &tc : cases) {
            ETC1 original;
            original.current_gear = tc.current_gear;
            original.selected_gear = tc.selected_gear;
            original.output_shaft_speed_rpm = tc.speed;

            auto encoded = original.encode();
            auto decoded = ETC1::decode(encoded);

            CHECK(decoded.current_gear == tc.current_gear);
            CHECK(decoded.selected_gear == tc.selected_gear);
            CHECK(decoded.output_shaft_speed_rpm == doctest::Approx(tc.speed).epsilon(0.125));
        }
    }

    SUBCASE("TransmissionOilTemp various temperatures") {
        dp::Vector<f64> temps = {-40.0, -20.0, 0.0, 25.0, 60.0, 85.0, 100.0, 120.0, 150.0};

        for (f64 temp : temps) {
            TransmissionOilTemp original;
            original.oil_temp_c = temp;

            auto encoded = original.encode();
            auto decoded = TransmissionOilTemp::decode(encoded);

            CHECK(decoded.oil_temp_c == doctest::Approx(temp).epsilon(0.03125));
        }
    }

    SUBCASE("CruiseControl various speeds and states") {
        struct TestCase {
            f64 wheel_speed;
            f64 set_speed;
            u8 cc_active;
            u8 brake;
        };

        dp::Vector<TestCase> cases = {
            {0.0, 0.0, 0x00, 0x01},       // Stopped, brake on
            {30.0, 0.0, 0x00, 0x00},      // City driving, CC off
            {80.0, 80.0, 0x01, 0x00},     // CC active at 80
            {100.0, 100.0, 0x01, 0x00},   // CC active at 100
            {120.0, 120.0, 0x01, 0x00},   // CC active at 120
            {100.0, 110.0, 0x01, 0x00},   // Accelerating to set speed
            {90.0, 100.0, 0x00, 0x01},    // CC off, braking
        };

        for (const auto &tc : cases) {
            CruiseControl original;
            original.wheel_speed_kmh = tc.wheel_speed;
            original.cc_set_speed_kmh = tc.set_speed;
            original.cc_active = tc.cc_active;
            original.brake_switch = tc.brake;

            auto encoded = original.encode();
            auto decoded = CruiseControl::decode(encoded);

            CHECK(decoded.wheel_speed_kmh == doctest::Approx(tc.wheel_speed).epsilon(1.0 / 256.0));
            CHECK(decoded.cc_set_speed_kmh == doctest::Approx(tc.set_speed).epsilon(1.0 / 256.0));
            CHECK(decoded.cc_active == tc.cc_active);
            CHECK(decoded.brake_switch == tc.brake);
        }
    }
}
