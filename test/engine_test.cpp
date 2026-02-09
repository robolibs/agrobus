#include <doctest/doctest.h>
#include <agrobus/j1939/engine.hpp>

using namespace agrobus::j1939;

// ═════════════════════════════════════════════════════════════════════════════
// EEC1 (Electronic Engine Controller 1) Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("EEC1 encode and decode") {
    SUBCASE("typical values") {
        EEC1 msg;
        msg.engine_torque_percent = 50.0;
        msg.driver_demand_percent = 75.0;
        msg.actual_engine_percent = 60.0;
        msg.engine_speed_rpm = 1500.0;
        msg.starter_mode = 0x02;
        msg.source_address = 0x00;

        auto encoded = msg.encode();
        CHECK(encoded.size() == 8);

        auto decoded = EEC1::decode(encoded);
        CHECK(decoded.engine_torque_percent == doctest::Approx(50.0));
        CHECK(decoded.driver_demand_percent == doctest::Approx(75.0));
        CHECK(decoded.actual_engine_percent == doctest::Approx(60.0));
        CHECK(decoded.engine_speed_rpm == doctest::Approx(1500.0).epsilon(0.2));
        CHECK(decoded.starter_mode == 0x02);
        CHECK(decoded.source_address == 0x00);
    }

    SUBCASE("zero values") {
        EEC1 msg;
        msg.engine_torque_percent = 0.0;
        msg.driver_demand_percent = 0.0;
        msg.actual_engine_percent = 0.0;
        msg.engine_speed_rpm = 0.0;

        auto encoded = msg.encode();
        auto decoded = EEC1::decode(encoded);
        CHECK(decoded.engine_torque_percent == doctest::Approx(0.0));
        CHECK(decoded.engine_speed_rpm == doctest::Approx(0.0));
    }

    SUBCASE("negative torque") {
        EEC1 msg;
        msg.engine_torque_percent = -50.0;
        msg.driver_demand_percent = -25.0;

        auto encoded = msg.encode();
        auto decoded = EEC1::decode(encoded);
        CHECK(decoded.engine_torque_percent == doctest::Approx(-50.0));
        CHECK(decoded.driver_demand_percent == doctest::Approx(-25.0));
    }

    SUBCASE("max RPM") {
        EEC1 msg;
        msg.engine_speed_rpm = 8000.0;

        auto encoded = msg.encode();
        auto decoded = EEC1::decode(encoded);
        CHECK(decoded.engine_speed_rpm == doctest::Approx(8000.0).epsilon(0.2));
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// EEC2 (Electronic Engine Controller 2) Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("EEC2 encode and decode") {
    SUBCASE("typical values") {
        EEC2 msg;
        msg.accel_pedal_position = 128;
        msg.engine_load_percent = 75.0;
        msg.accel_pedal_low_idle = 0x01;
        msg.accel_pedal_kickdown = 0x02;
        msg.road_speed_limit = 120;

        auto encoded = msg.encode();
        CHECK(encoded.size() == 8);

        auto decoded = EEC2::decode(encoded);
        CHECK(decoded.accel_pedal_position == 128);
        CHECK(decoded.engine_load_percent == doctest::Approx(75.0));
        CHECK(decoded.accel_pedal_low_idle == 0x01);
        CHECK(decoded.accel_pedal_kickdown == 0x02);
        CHECK(decoded.road_speed_limit == 120);
    }

    SUBCASE("zero load") {
        EEC2 msg;
        msg.engine_load_percent = 0.0;
        msg.accel_pedal_position = 0;

        auto encoded = msg.encode();
        auto decoded = EEC2::decode(encoded);
        CHECK(decoded.engine_load_percent == doctest::Approx(0.0));
        CHECK(decoded.accel_pedal_position == 0);
    }

    SUBCASE("max load") {
        EEC2 msg;
        msg.engine_load_percent = 100.0;

        auto encoded = msg.encode();
        auto decoded = EEC2::decode(encoded);
        CHECK(decoded.engine_load_percent == doctest::Approx(100.0));
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// EngineTemp1 (Engine Temperature 1) Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("EngineTemp1 encode and decode") {
    SUBCASE("typical temperatures") {
        EngineTemp1 msg;
        msg.coolant_temp_c = 90.0;
        msg.fuel_temp_c = 50.0;
        msg.oil_temp_c = 100.0;
        msg.turbo_oil_temp_c = 120.0;
        msg.intercooler_temp_c = 60.0;

        auto encoded = msg.encode();
        CHECK(encoded.size() == 8);

        auto decoded = EngineTemp1::decode(encoded);
        CHECK(decoded.coolant_temp_c == doctest::Approx(90.0));
        CHECK(decoded.fuel_temp_c == doctest::Approx(50.0));
        CHECK(decoded.oil_temp_c == doctest::Approx(100.0).epsilon(0.1));
        CHECK(decoded.turbo_oil_temp_c == doctest::Approx(120.0).epsilon(0.1));
        CHECK(decoded.intercooler_temp_c == doctest::Approx(60.0));
    }

    SUBCASE("cold temperatures") {
        EngineTemp1 msg;
        msg.coolant_temp_c = -20.0;
        msg.fuel_temp_c = -10.0;
        msg.oil_temp_c = 0.0;

        auto encoded = msg.encode();
        auto decoded = EngineTemp1::decode(encoded);
        CHECK(decoded.coolant_temp_c == doctest::Approx(-20.0));
        CHECK(decoded.fuel_temp_c == doctest::Approx(-10.0));
        CHECK(decoded.oil_temp_c == doctest::Approx(0.0).epsilon(0.1));
    }

    SUBCASE("hot temperatures") {
        EngineTemp1 msg;
        msg.coolant_temp_c = 120.0;
        msg.oil_temp_c = 150.0;
        msg.turbo_oil_temp_c = 180.0;

        auto encoded = msg.encode();
        auto decoded = EngineTemp1::decode(encoded);
        CHECK(decoded.coolant_temp_c == doctest::Approx(120.0));
        CHECK(decoded.oil_temp_c == doctest::Approx(150.0).epsilon(0.1));
        CHECK(decoded.turbo_oil_temp_c == doctest::Approx(180.0).epsilon(0.1));
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// EngineFluidLP (Engine Fluid Level/Pressure) Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("EngineFluidLP encode and decode") {
    SUBCASE("typical values") {
        EngineFluidLP msg;
        msg.oil_pressure_kpa = 400.0;
        msg.coolant_pressure_kpa = 100.0;
        msg.oil_level_percent = 200;
        msg.coolant_level_percent = 250;
        msg.fuel_delivery_pressure_kpa = 300.0;
        msg.crankcase_pressure_kpa = 10.0;

        auto encoded = msg.encode();
        CHECK(encoded.size() == 8);

        auto decoded = EngineFluidLP::decode(encoded);
        CHECK(decoded.oil_pressure_kpa == doctest::Approx(400.0));
        CHECK(decoded.coolant_pressure_kpa == doctest::Approx(100.0));
        CHECK(decoded.oil_level_percent == 200);
        CHECK(decoded.coolant_level_percent == 250);
        CHECK(decoded.fuel_delivery_pressure_kpa == doctest::Approx(300.0));
        CHECK(decoded.crankcase_pressure_kpa == doctest::Approx(10.0).epsilon(0.1));
    }

    SUBCASE("zero pressures") {
        EngineFluidLP msg;
        msg.oil_pressure_kpa = 0.0;
        msg.coolant_pressure_kpa = 0.0;
        msg.fuel_delivery_pressure_kpa = 0.0;
        msg.crankcase_pressure_kpa = 0.0;

        auto encoded = msg.encode();
        auto decoded = EngineFluidLP::decode(encoded);
        CHECK(decoded.oil_pressure_kpa == doctest::Approx(0.0));
        CHECK(decoded.coolant_pressure_kpa == doctest::Approx(0.0));
        CHECK(decoded.fuel_delivery_pressure_kpa == doctest::Approx(0.0));
        CHECK(decoded.crankcase_pressure_kpa == doctest::Approx(0.0).epsilon(0.1));
    }

    SUBCASE("high pressures") {
        EngineFluidLP msg;
        msg.oil_pressure_kpa = 800.0;
        msg.coolant_pressure_kpa = 200.0;
        msg.fuel_delivery_pressure_kpa = 600.0;

        auto encoded = msg.encode();
        auto decoded = EngineFluidLP::decode(encoded);
        CHECK(decoded.oil_pressure_kpa == doctest::Approx(800.0));
        CHECK(decoded.coolant_pressure_kpa == doctest::Approx(200.0));
        CHECK(decoded.fuel_delivery_pressure_kpa == doctest::Approx(600.0));
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// EngineHours Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("EngineHours encode and decode") {
    SUBCASE("typical values") {
        EngineHours msg;
        msg.total_hours = 1000.0;
        msg.total_revolutions = 50000000.0;

        auto encoded = msg.encode();
        CHECK(encoded.size() == 8);

        auto decoded = EngineHours::decode(encoded);
        CHECK(decoded.total_hours == doctest::Approx(1000.0).epsilon(0.1));
        CHECK(decoded.total_revolutions == doctest::Approx(50000000.0).epsilon(1000));
    }

    SUBCASE("new engine") {
        EngineHours msg;
        msg.total_hours = 0.0;
        msg.total_revolutions = 0.0;

        auto encoded = msg.encode();
        auto decoded = EngineHours::decode(encoded);
        CHECK(decoded.total_hours == doctest::Approx(0.0));
        CHECK(decoded.total_revolutions == doctest::Approx(0.0));
    }

    SUBCASE("high hours") {
        EngineHours msg;
        msg.total_hours = 10000.0;
        msg.total_revolutions = 500000000.0;

        auto encoded = msg.encode();
        auto decoded = EngineHours::decode(encoded);
        CHECK(decoded.total_hours == doctest::Approx(10000.0).epsilon(0.1));
        CHECK(decoded.total_revolutions == doctest::Approx(500000000.0).epsilon(1000));
    }

    SUBCASE("fractional hours") {
        EngineHours msg;
        msg.total_hours = 123.45;

        auto encoded = msg.encode();
        auto decoded = EngineHours::decode(encoded);
        CHECK(decoded.total_hours == doctest::Approx(123.45).epsilon(0.1));
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// FuelEconomy Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("FuelEconomy encode and decode") {
    SUBCASE("typical values") {
        FuelEconomy msg;
        msg.fuel_rate_lph = 20.0;
        msg.instantaneous_lph = 15.5;
        msg.throttle_position = 50.0;

        auto encoded = msg.encode();
        CHECK(encoded.size() == 8);

        auto decoded = FuelEconomy::decode(encoded);
        CHECK(decoded.fuel_rate_lph == doctest::Approx(20.0).epsilon(0.1));
        CHECK(decoded.instantaneous_lph == doctest::Approx(15.5).epsilon(0.01));
        CHECK(decoded.throttle_position == doctest::Approx(50.0).epsilon(0.5));
    }

    SUBCASE("idle") {
        FuelEconomy msg;
        msg.fuel_rate_lph = 2.0;
        msg.instantaneous_lph = 1.5;
        msg.throttle_position = 0.0;

        auto encoded = msg.encode();
        auto decoded = FuelEconomy::decode(encoded);
        CHECK(decoded.fuel_rate_lph == doctest::Approx(2.0).epsilon(0.1));
        CHECK(decoded.instantaneous_lph == doctest::Approx(1.5).epsilon(0.01));
        CHECK(decoded.throttle_position == doctest::Approx(0.0));
    }

    SUBCASE("full throttle") {
        FuelEconomy msg;
        msg.fuel_rate_lph = 50.0;
        msg.instantaneous_lph = 40.0;
        msg.throttle_position = 100.0;

        auto encoded = msg.encode();
        auto decoded = FuelEconomy::decode(encoded);
        CHECK(decoded.fuel_rate_lph == doctest::Approx(50.0).epsilon(0.1));
        CHECK(decoded.instantaneous_lph == doctest::Approx(40.0).epsilon(0.01));
        CHECK(decoded.throttle_position == doctest::Approx(100.0).epsilon(0.5));
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// EEC3 (Electronic Engine Controller 3) Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("EEC3 encode and decode") {
    SUBCASE("typical values") {
        EEC3 msg;
        msg.nominal_friction_percent = 10.0;
        msg.desired_operating_speed_rpm = 1800.0;
        msg.operating_speed_asymmetry = 50;

        auto encoded = msg.encode();
        CHECK(encoded.size() == 8);

        auto decoded = EEC3::decode(encoded);
        CHECK(decoded.nominal_friction_percent == doctest::Approx(10.0));
        CHECK(decoded.desired_operating_speed_rpm == doctest::Approx(1800.0).epsilon(0.2));
        CHECK(decoded.operating_speed_asymmetry == 50);
    }

    SUBCASE("zero friction") {
        EEC3 msg;
        msg.nominal_friction_percent = 0.0;
        msg.desired_operating_speed_rpm = 0.0;

        auto encoded = msg.encode();
        auto decoded = EEC3::decode(encoded);
        CHECK(decoded.nominal_friction_percent == doctest::Approx(0.0));
        CHECK(decoded.desired_operating_speed_rpm == doctest::Approx(0.0));
    }

    SUBCASE("negative friction") {
        EEC3 msg;
        msg.nominal_friction_percent = -50.0;

        auto encoded = msg.encode();
        auto decoded = EEC3::decode(encoded);
        CHECK(decoded.nominal_friction_percent == doctest::Approx(-50.0));
    }

    SUBCASE("high RPM") {
        EEC3 msg;
        msg.desired_operating_speed_rpm = 3000.0;

        auto encoded = msg.encode();
        auto decoded = EEC3::decode(encoded);
        CHECK(decoded.desired_operating_speed_rpm == doctest::Approx(3000.0).epsilon(0.2));
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Edge Case Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("Engine messages edge cases") {
    SUBCASE("EEC1 boundary values") {
        EEC1 msg;
        msg.engine_torque_percent = 125.0;
        msg.driver_demand_percent = -125.0;

        auto encoded = msg.encode();
        auto decoded = EEC1::decode(encoded);
        CHECK(decoded.engine_torque_percent == doctest::Approx(125.0));
        CHECK(decoded.driver_demand_percent == doctest::Approx(-125.0));
    }

    SUBCASE("EngineTemp1 extreme cold") {
        EngineTemp1 msg;
        msg.coolant_temp_c = -40.0;
        msg.fuel_temp_c = -40.0;
        msg.intercooler_temp_c = -40.0;

        auto encoded = msg.encode();
        auto decoded = EngineTemp1::decode(encoded);
        CHECK(decoded.coolant_temp_c == doctest::Approx(-40.0));
        CHECK(decoded.fuel_temp_c == doctest::Approx(-40.0));
        CHECK(decoded.intercooler_temp_c == doctest::Approx(-40.0));
    }

    SUBCASE("EngineTemp1 extreme hot") {
        EngineTemp1 msg;
        msg.coolant_temp_c = 200.0;
        msg.oil_temp_c = 200.0;

        auto encoded = msg.encode();
        auto decoded = EngineTemp1::decode(encoded);
        CHECK(decoded.coolant_temp_c == doctest::Approx(200.0));
        CHECK(decoded.oil_temp_c == doctest::Approx(200.0).epsilon(0.1));
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Round-Trip Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("Engine messages round-trip") {
    SUBCASE("EEC1 multiple encode/decode") {
        for (int i = 0; i < 10; i++) {
            EEC1 msg;
            msg.engine_speed_rpm = 1000.0 + i * 100.0;
            msg.engine_torque_percent = -50.0 + i * 10.0;

            auto encoded = msg.encode();
            auto decoded = EEC1::decode(encoded);

            CHECK(decoded.engine_speed_rpm == doctest::Approx(msg.engine_speed_rpm).epsilon(0.2));
            CHECK(decoded.engine_torque_percent == doctest::Approx(msg.engine_torque_percent));
        }
    }

    SUBCASE("EngineTemp1 multiple encode/decode") {
        for (int i = 0; i < 10; i++) {
            EngineTemp1 msg;
            msg.coolant_temp_c = 50.0 + i * 10.0;
            msg.oil_temp_c = 80.0 + i * 5.0;

            auto encoded = msg.encode();
            auto decoded = EngineTemp1::decode(encoded);

            CHECK(decoded.coolant_temp_c == doctest::Approx(msg.coolant_temp_c));
            CHECK(decoded.oil_temp_c == doctest::Approx(msg.oil_temp_c).epsilon(0.1));
        }
    }
}
