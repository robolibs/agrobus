#include <doctest/doctest.h>
#include <agrobus/j1939/engine.hpp>
#include <agrobus/net/network_manager.hpp>
#include <echo/echo.hpp>

using namespace agrobus::j1939;
using namespace agrobus::net;

// ═════════════════════════════════════════════════════════════════════════════
// Engine Temperature 2 (ET2) Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("EngineTemp2 construction") {
    EngineTemp2 et2;
    CHECK(et2.engine_oil_temp_c == doctest::Approx(-40.0));
    CHECK(et2.turbo_oil_temp_c == doctest::Approx(-40.0));
    CHECK(et2.engine_intercooler_temp_c == doctest::Approx(-40.0));
    CHECK(et2.turbo_1_temp_c == doctest::Approx(-40.0));
}

TEST_CASE("EngineTemp2 encoding/decoding - normal temperatures") {
    EngineTemp2 send;
    send.engine_oil_temp_c = 95.0;
    send.turbo_oil_temp_c = 120.0;
    send.engine_intercooler_temp_c = 65.0;
    send.turbo_1_temp_c = 110.0;

    auto data = send.encode();
    CHECK(data.size() == 8);

    auto recv = EngineTemp2::decode(data);
    CHECK(recv.engine_oil_temp_c == doctest::Approx(95.0).epsilon(0.1));
    CHECK(recv.turbo_oil_temp_c == doctest::Approx(120.0).epsilon(0.1));
    CHECK(recv.engine_intercooler_temp_c == doctest::Approx(65.0).epsilon(1.0));
    CHECK(recv.turbo_1_temp_c == doctest::Approx(110.0).epsilon(0.1));
}

TEST_CASE("EngineTemp2 encoding/decoding - cold temperatures") {
    EngineTemp2 send;
    send.engine_oil_temp_c = -20.0;
    send.turbo_oil_temp_c = -15.0;
    send.engine_intercooler_temp_c = -30.0;
    send.turbo_1_temp_c = -25.0;

    auto data = send.encode();
    auto recv = EngineTemp2::decode(data);

    CHECK(recv.engine_oil_temp_c == doctest::Approx(-20.0).epsilon(0.1));
    CHECK(recv.turbo_oil_temp_c == doctest::Approx(-15.0).epsilon(0.1));
    CHECK(recv.engine_intercooler_temp_c == doctest::Approx(-30.0).epsilon(1.0));
    CHECK(recv.turbo_1_temp_c == doctest::Approx(-25.0).epsilon(0.1));
}

TEST_CASE("EngineTemp2 encoding/decoding - high temperatures") {
    EngineTemp2 send;
    send.engine_oil_temp_c = 150.0;
    send.turbo_oil_temp_c = 180.0;
    send.engine_intercooler_temp_c = 100.0;
    send.turbo_1_temp_c = 200.0;

    auto data = send.encode();
    auto recv = EngineTemp2::decode(data);

    CHECK(recv.engine_oil_temp_c == doctest::Approx(150.0).epsilon(0.1));
    CHECK(recv.turbo_oil_temp_c == doctest::Approx(180.0).epsilon(0.1));
    CHECK(recv.engine_intercooler_temp_c == doctest::Approx(100.0).epsilon(1.0));
    CHECK(recv.turbo_1_temp_c == doctest::Approx(200.0).epsilon(0.1));
}

// ═════════════════════════════════════════════════════════════════════════════
// Electronic Transmission Controller 1 (ETC1) Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("ETC1 construction") {
    ETC1 etc1;
    CHECK(etc1.trans_driveline == 0xFF);
    CHECK(etc1.trans_torque_mode == 0xFF);
    CHECK(etc1.trans_shift_in_progress == 0xFF);
    CHECK(etc1.trans_output_shaft_speed_rpm == -32127);
    CHECK(etc1.trans_percent_clutch_slip == doctest::Approx(0.0));
    CHECK(etc1.current_gear == 0xFF);
    CHECK(etc1.selected_gear == 0xFF);
}

TEST_CASE("ETC1 encoding/decoding - gear selection") {
    ETC1 send;
    send.trans_driveline = 0x01; // engaged
    send.trans_torque_mode = 0x01; // lockup engaged
    send.trans_shift_in_progress = 0x00; // not in progress
    send.trans_output_shaft_speed_rpm = 1500;
    send.trans_percent_clutch_slip = 5.0;
    send.selected_gear = 4; // 4th gear selected
    send.current_gear = 4; // 4th gear attained
    send.trans_requested_range = 10;
    send.trans_current_range = 10;

    auto data = send.encode();
    CHECK(data.size() == 8);

    auto recv = ETC1::decode(data);
    CHECK(recv.trans_driveline == 0x01);
    CHECK(recv.trans_torque_mode == 0x01);
    CHECK(recv.trans_shift_in_progress == 0x00);
    CHECK(recv.trans_output_shaft_speed_rpm == doctest::Approx(1500).epsilon(1.0));
    CHECK(recv.trans_percent_clutch_slip == doctest::Approx(5.0).epsilon(0.5));
    CHECK(recv.selected_gear == 4);
    CHECK(recv.current_gear == 4);
    CHECK(recv.trans_requested_range == 10);
    CHECK(recv.trans_current_range == 10);
}

TEST_CASE("ETC1 encoding/decoding - shift in progress") {
    ETC1 send;
    send.trans_driveline = 0x01;
    send.trans_shift_in_progress = 0x01; // shift in progress
    send.trans_output_shaft_speed_rpm = 1200;
    send.trans_percent_clutch_slip = 15.0; // higher slip during shift
    send.selected_gear = 5;
    send.current_gear = 4; // transitioning from 4 to 5

    auto data = send.encode();
    auto recv = ETC1::decode(data);

    CHECK(recv.trans_shift_in_progress == 0x01);
    CHECK(recv.trans_percent_clutch_slip == doctest::Approx(15.0).epsilon(0.5));
    CHECK(recv.selected_gear == 5);
    CHECK(recv.current_gear == 4);
}

TEST_CASE("ETC1 encoding/decoding - neutral position") {
    ETC1 send;
    send.trans_driveline = 0x00; // disengaged
    send.trans_torque_mode = 0x00; // lockup not engaged
    send.trans_output_shaft_speed_rpm = 0;
    send.trans_percent_clutch_slip = 0.0;
    send.selected_gear = 0xFF; // neutral
    send.current_gear = 0xFF; // neutral

    auto data = send.encode();
    auto recv = ETC1::decode(data);

    CHECK(recv.trans_driveline == 0x00);
    CHECK(recv.trans_output_shaft_speed_rpm == doctest::Approx(0.0).epsilon(1.0));
    CHECK(recv.selected_gear == 0xFF);
    CHECK(recv.current_gear == 0xFF);
}

// ═════════════════════════════════════════════════════════════════════════════
// Electronic Transmission Controller 2 (ETC2) Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("ETC2 construction") {
    ETC2 etc2;
    CHECK(etc2.trans_oil_temp_c == doctest::Approx(-40.0));
    CHECK(etc2.trans_oil_level == 0xFF);
    CHECK(etc2.trans_oil_pressure_kpa == doctest::Approx(0.0));
    CHECK(etc2.trans_oil_filter_diff_pressure_kpa == doctest::Approx(0.0));
    CHECK(etc2.trans_range_selected == 0xFF);
    CHECK(etc2.trans_range_attained == 0xFF);
}

TEST_CASE("ETC2 encoding/decoding - normal operation") {
    ETC2 send;
    send.trans_oil_temp_c = 85.0;
    send.trans_oil_level = 200; // ~80% (200 * 0.4 = 80%)
    send.trans_oil_pressure_kpa = 400.0;
    send.trans_oil_filter_diff_pressure_kpa = 50.0;
    send.trans_range_selected = 10;
    send.trans_range_attained = 10;

    auto data = send.encode();
    CHECK(data.size() == 8);

    auto recv = ETC2::decode(data);
    CHECK(recv.trans_oil_temp_c == doctest::Approx(85.0).epsilon(0.1));
    CHECK(recv.trans_oil_level == 200);
    CHECK(recv.trans_oil_pressure_kpa == doctest::Approx(400.0).epsilon(4.0));
    CHECK(recv.trans_oil_filter_diff_pressure_kpa == doctest::Approx(50.0).epsilon(0.5));
    CHECK(recv.trans_range_selected == 10);
    CHECK(recv.trans_range_attained == 10);
}

TEST_CASE("ETC2 encoding/decoding - cold start") {
    ETC2 send;
    send.trans_oil_temp_c = -10.0;
    send.trans_oil_level = 210; // ~84%
    send.trans_oil_pressure_kpa = 600.0; // higher pressure when cold
    send.trans_oil_filter_diff_pressure_kpa = 80.0;

    auto data = send.encode();
    auto recv = ETC2::decode(data);

    CHECK(recv.trans_oil_temp_c == doctest::Approx(-10.0).epsilon(0.1));
    CHECK(recv.trans_oil_pressure_kpa == doctest::Approx(600.0).epsilon(4.0));
    CHECK(recv.trans_oil_filter_diff_pressure_kpa == doctest::Approx(80.0).epsilon(0.5));
}

TEST_CASE("ETC2 encoding/decoding - high temperature warning") {
    ETC2 send;
    send.trans_oil_temp_c = 120.0; // high temperature
    send.trans_oil_level = 180; // ~72%, slightly low
    send.trans_oil_pressure_kpa = 250.0; // lower pressure when hot
    send.trans_oil_filter_diff_pressure_kpa = 100.0; // high differential, clogged filter

    auto data = send.encode();
    auto recv = ETC2::decode(data);

    CHECK(recv.trans_oil_temp_c == doctest::Approx(120.0).epsilon(0.1));
    CHECK(recv.trans_oil_level == 180);
    CHECK(recv.trans_oil_pressure_kpa == doctest::Approx(250.0).epsilon(4.0));
    CHECK(recv.trans_oil_filter_diff_pressure_kpa == doctest::Approx(100.0).epsilon(0.5));
}

// ═════════════════════════════════════════════════════════════════════════════
// Aftertreatment 1 (AT1) Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("Aftertreatment1 construction") {
    Aftertreatment1 at1;
    CHECK(at1.diesel_exhaust_fluid_tank_level == doctest::Approx(0.0));
    CHECK(at1.intake_nox_ppm == doctest::Approx(0.0));
    CHECK(at1.outlet_nox_ppm == doctest::Approx(0.0));
    CHECK(at1.intake_nox_reading_status == doctest::Approx(0xFF));
    CHECK(at1.outlet_nox_reading_status == doctest::Approx(0xFF));
}

TEST_CASE("Aftertreatment1 encoding/decoding - normal operation") {
    Aftertreatment1 send;
    send.diesel_exhaust_fluid_tank_level = 80.0; // 80% full
    send.intake_nox_ppm = 500.0;
    send.outlet_nox_ppm = 50.0; // 90% reduction
    send.intake_nox_reading_status = 0x00; // valid
    send.outlet_nox_reading_status = 0x00; // valid

    auto data = send.encode();
    CHECK(data.size() == 8);

    auto recv = Aftertreatment1::decode(data);
    CHECK(recv.diesel_exhaust_fluid_tank_level == doctest::Approx(80.0).epsilon(0.5));
    CHECK(recv.intake_nox_ppm == doctest::Approx(500.0).epsilon(1.0));
    CHECK(recv.outlet_nox_ppm == doctest::Approx(50.0).epsilon(1.0));
    CHECK(recv.intake_nox_reading_status == 0x00);
    CHECK(recv.outlet_nox_reading_status == 0x00);
}

TEST_CASE("Aftertreatment1 encoding/decoding - low DEF level") {
    Aftertreatment1 send;
    send.diesel_exhaust_fluid_tank_level = 15.0; // low, needs refill
    send.intake_nox_ppm = 600.0;
    send.outlet_nox_ppm = 80.0; // reduced efficiency
    send.intake_nox_reading_status = 0x00;
    send.outlet_nox_reading_status = 0x00;

    auto data = send.encode();
    auto recv = Aftertreatment1::decode(data);

    CHECK(recv.diesel_exhaust_fluid_tank_level == doctest::Approx(15.0).epsilon(0.5));
    CHECK(recv.intake_nox_ppm == doctest::Approx(600.0).epsilon(1.0));
    CHECK(recv.outlet_nox_ppm == doctest::Approx(80.0).epsilon(1.0));
}

TEST_CASE("Aftertreatment1 encoding/decoding - high NOx levels") {
    Aftertreatment1 send;
    send.diesel_exhaust_fluid_tank_level = 90.0;
    send.intake_nox_ppm = 1200.0; // high load
    send.outlet_nox_ppm = 150.0; // ~87.5% reduction
    send.intake_nox_reading_status = 0x00;
    send.outlet_nox_reading_status = 0x00;

    auto data = send.encode();
    auto recv = Aftertreatment1::decode(data);

    CHECK(recv.intake_nox_ppm == doctest::Approx(1200.0).epsilon(2.0));
    CHECK(recv.outlet_nox_ppm == doctest::Approx(150.0).epsilon(2.0));
}

TEST_CASE("Aftertreatment1 encoding/decoding - sensor faults") {
    Aftertreatment1 send;
    send.diesel_exhaust_fluid_tank_level = 50.0;
    send.intake_nox_ppm = 0.0;
    send.outlet_nox_ppm = 0.0;
    send.intake_nox_reading_status = 0x02; // fault
    send.outlet_nox_reading_status = 0x02; // fault

    auto data = send.encode();
    auto recv = Aftertreatment1::decode(data);

    CHECK(recv.intake_nox_reading_status == 0x02);
    CHECK(recv.outlet_nox_reading_status == 0x02);
}

// ═════════════════════════════════════════════════════════════════════════════
// Aftertreatment 2 (AT2) Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("Aftertreatment2 construction") {
    Aftertreatment2 at2;
    CHECK(at2.dpf_differential_pressure_kpa == doctest::Approx(0.0));
    CHECK(at2.diesel_exhaust_fluid_concentration == doctest::Approx(0.0));
    CHECK(at2.dpf_soot_load_percent == doctest::Approx(0.0));
    CHECK(at2.dpf_active_regeneration_status == 0xFF);
    CHECK(at2.dpf_passive_regeneration_status == 0xFF);
}

TEST_CASE("Aftertreatment2 encoding/decoding - normal operation") {
    Aftertreatment2 send;
    send.dpf_differential_pressure_kpa = 5.0; // clean filter
    send.diesel_exhaust_fluid_concentration = 32.0; // 32% urea concentration (spec)
    send.dpf_soot_load_percent = 20.0;
    send.dpf_active_regeneration_status = 0x00; // not active
    send.dpf_passive_regeneration_status = 0x00; // not active

    auto data = send.encode();
    CHECK(data.size() == 8);

    auto recv = Aftertreatment2::decode(data);
    CHECK(recv.dpf_differential_pressure_kpa == doctest::Approx(5.0).epsilon(0.2));
    CHECK(recv.diesel_exhaust_fluid_concentration == doctest::Approx(32.0).epsilon(0.5));
    CHECK(recv.dpf_soot_load_percent == doctest::Approx(20.0).epsilon(0.5));
    CHECK(recv.dpf_active_regeneration_status == 0x00);
    CHECK(recv.dpf_passive_regeneration_status == 0x00);
}

TEST_CASE("Aftertreatment2 encoding/decoding - high soot load") {
    Aftertreatment2 send;
    send.dpf_differential_pressure_kpa = 15.0; // elevated pressure
    send.diesel_exhaust_fluid_concentration = 32.0;
    send.dpf_soot_load_percent = 75.0; // high soot, needs regeneration
    send.dpf_active_regeneration_status = 0x00;
    send.dpf_passive_regeneration_status = 0x00;

    auto data = send.encode();
    auto recv = Aftertreatment2::decode(data);

    CHECK(recv.dpf_differential_pressure_kpa == doctest::Approx(15.0).epsilon(0.2));
    CHECK(recv.dpf_soot_load_percent == doctest::Approx(75.0).epsilon(0.5));
}

TEST_CASE("Aftertreatment2 encoding/decoding - active regeneration") {
    Aftertreatment2 send;
    send.dpf_differential_pressure_kpa = 20.0; // high during regeneration
    send.diesel_exhaust_fluid_concentration = 32.0;
    send.dpf_soot_load_percent = 80.0; // triggering regeneration
    send.dpf_active_regeneration_status = 0x01; // active regeneration in progress
    send.dpf_passive_regeneration_status = 0x00;

    auto data = send.encode();
    auto recv = Aftertreatment2::decode(data);

    CHECK(recv.dpf_differential_pressure_kpa == doctest::Approx(20.0).epsilon(0.2));
    CHECK(recv.dpf_soot_load_percent == doctest::Approx(80.0).epsilon(0.5));
    CHECK(recv.dpf_active_regeneration_status == 0x01);
}

TEST_CASE("Aftertreatment2 encoding/decoding - passive regeneration") {
    Aftertreatment2 send;
    send.dpf_differential_pressure_kpa = 8.0;
    send.diesel_exhaust_fluid_concentration = 32.0;
    send.dpf_soot_load_percent = 45.0;
    send.dpf_active_regeneration_status = 0x00;
    send.dpf_passive_regeneration_status = 0x01; // passive regeneration occurring

    auto data = send.encode();
    auto recv = Aftertreatment2::decode(data);

    CHECK(recv.dpf_passive_regeneration_status == 0x01);
}

TEST_CASE("Aftertreatment2 encoding/decoding - low DEF concentration") {
    Aftertreatment2 send;
    send.dpf_differential_pressure_kpa = 6.0;
    send.diesel_exhaust_fluid_concentration = 20.0; // too low, diluted
    send.dpf_soot_load_percent = 30.0;
    send.dpf_active_regeneration_status = 0x00;
    send.dpf_passive_regeneration_status = 0x00;

    auto data = send.encode();
    auto recv = Aftertreatment2::decode(data);

    CHECK(recv.diesel_exhaust_fluid_concentration == doctest::Approx(20.0).epsilon(0.5));
}

// ═════════════════════════════════════════════════════════════════════════════
// Integration Tests with EngineInterface
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("EngineInterface - ET2 message handling") {
    IsoNet net;
    Name name = Name::build().set_identity_number(200);
    auto cf = net.create_internal(name, 0, 0x50).value();

    EngineInterface ei(net, cf);
    ei.initialize();

    bool received = false;
    ei.on_engine_temp2.subscribe([&](const EngineTemp2 &msg, Address src) {
        CHECK(msg.engine_oil_temp_c == doctest::Approx(100.0).epsilon(0.1));
        CHECK(src == 0x51);
        received = true;
    });

    // Simulate receiving ET2 message
    EngineTemp2 send;
    send.engine_oil_temp_c = 100.0;
    auto data = send.encode();

    Message msg;
    msg.pgn = PGN_ET2;
    msg.source = 0x51;
    msg.data = data;
    net.inject_message(msg);

    CHECK(received);
}

TEST_CASE("EngineInterface - ETC1 message handling") {
    IsoNet net;
    Name name = Name::build().set_identity_number(201);
    auto cf = net.create_internal(name, 0, 0x52).value();

    EngineInterface ei(net, cf);
    ei.initialize();

    bool received = false;
    ei.on_etc1.subscribe([&](const ETC1 &msg, Address src) {
        CHECK(msg.current_gear == 5);
        CHECK(msg.selected_gear == 5);
        CHECK(src == 0x53);
        received = true;
    });

    ETC1 send;
    send.current_gear = 5;
    send.selected_gear = 5;
    auto data = send.encode();

    Message msg;
    msg.pgn = PGN_ETC1;
    msg.source = 0x53;
    msg.data = data;
    net.inject_message(msg);

    CHECK(received);
}

TEST_CASE("EngineInterface - ETC2 message handling") {
    IsoNet net;
    Name name = Name::build().set_identity_number(202);
    auto cf = net.create_internal(name, 0, 0x54).value();

    EngineInterface ei(net, cf);
    ei.initialize();

    bool received = false;
    ei.on_etc2.subscribe([&](const ETC2 &msg, Address src) {
        CHECK(msg.trans_oil_temp_c == doctest::Approx(85.0).epsilon(0.1));
        CHECK(msg.trans_oil_pressure_kpa == doctest::Approx(400.0).epsilon(4.0));
        received = true;
    });

    ETC2 send;
    send.trans_oil_temp_c = 85.0;
    send.trans_oil_pressure_kpa = 400.0;
    auto data = send.encode();

    Message msg;
    msg.pgn = PGN_ETC2;
    msg.source = 0x55;
    msg.data = data;
    net.inject_message(msg);

    CHECK(received);
}

TEST_CASE("EngineInterface - AT1 message handling") {
    IsoNet net;
    Name name = Name::build().set_identity_number(203);
    auto cf = net.create_internal(name, 0, 0x56).value();

    EngineInterface ei(net, cf);
    ei.initialize();

    bool received = false;
    ei.on_aftertreatment1.subscribe([&](const Aftertreatment1 &msg, Address src) {
        CHECK(msg.diesel_exhaust_fluid_tank_level == doctest::Approx(75.0).epsilon(0.5));
        CHECK(msg.intake_nox_ppm == doctest::Approx(500.0).epsilon(1.0));
        received = true;
    });

    Aftertreatment1 send;
    send.diesel_exhaust_fluid_tank_level = 75.0;
    send.intake_nox_ppm = 500.0;
    auto data = send.encode();

    Message msg;
    msg.pgn = PGN_AT1;
    msg.source = 0x57;
    msg.data = data;
    net.inject_message(msg);

    CHECK(received);
}

TEST_CASE("EngineInterface - AT2 message handling") {
    IsoNet net;
    Name name = Name::build().set_identity_number(204);
    auto cf = net.create_internal(name, 0, 0x58).value();

    EngineInterface ei(net, cf);
    ei.initialize();

    bool received = false;
    ei.on_aftertreatment2.subscribe([&](const Aftertreatment2 &msg, Address src) {
        CHECK(msg.dpf_soot_load_percent == doctest::Approx(60.0).epsilon(0.5));
        CHECK(msg.dpf_active_regeneration_status == 0x01);
        received = true;
    });

    Aftertreatment2 send;
    send.dpf_soot_load_percent = 60.0;
    send.dpf_active_regeneration_status = 0x01;
    auto data = send.encode();

    Message msg;
    msg.pgn = PGN_AT2;
    msg.source = 0x59;
    msg.data = data;
    net.inject_message(msg);

    CHECK(received);
}
