#include <doctest/doctest.h>
#include <agrobus/isobus/implement/lighting.hpp>
#include <agrobus/isobus/implement/tractor_facilities.hpp>
#include <agrobus/isobus/implement/machine_speed_cmd.hpp>
#include <agrobus/isobus/implement/aux_valve_status.hpp>

using namespace agrobus::isobus;
using namespace agrobus::isobus::implement;

// ─── Lighting encode/decode ────────────────────────────────────────────────────

TEST_CASE("LightingState encode/decode round-trip") {
    LightingState ls;
    ls.left_turn = LightState::On;
    ls.right_turn = LightState::Off;
    ls.low_beam = LightState::On;
    ls.high_beam = LightState::Off;
    ls.front_fog = LightState::NotAvailable;
    ls.rear_fog = LightState::Error;
    ls.beacon = LightState::On;
    ls.running = LightState::Off;
    ls.rear_work = LightState::On;
    ls.front_work = LightState::Off;
    ls.side_work = LightState::NotAvailable;
    ls.hazard = LightState::On;
    ls.backup = LightState::Off;
    ls.center_stop = LightState::On;
    ls.left_stop = LightState::Error;
    ls.right_stop = LightState::NotAvailable;

    auto data = ls.encode();
    REQUIRE(data.size() == 8);

    auto decoded = LightingState::decode(data);
    CHECK(decoded.left_turn == LightState::On);
    CHECK(decoded.right_turn == LightState::Off);
    CHECK(decoded.low_beam == LightState::On);
    CHECK(decoded.high_beam == LightState::Off);
    CHECK(decoded.front_fog == LightState::NotAvailable);
    CHECK(decoded.rear_fog == LightState::Error);
    CHECK(decoded.beacon == LightState::On);
    CHECK(decoded.running == LightState::Off);
    CHECK(decoded.rear_work == LightState::On);
    CHECK(decoded.front_work == LightState::Off);
    CHECK(decoded.side_work == LightState::NotAvailable);
    CHECK(decoded.hazard == LightState::On);
    CHECK(decoded.backup == LightState::Off);
    CHECK(decoded.center_stop == LightState::On);
    CHECK(decoded.left_stop == LightState::Error);
    CHECK(decoded.right_stop == LightState::NotAvailable);
}

TEST_CASE("LightingState all off") {
    LightingState ls;
    ls.left_turn = LightState::Off;
    ls.right_turn = LightState::Off;
    ls.low_beam = LightState::Off;
    ls.high_beam = LightState::Off;
    ls.front_fog = LightState::Off;
    ls.rear_fog = LightState::Off;
    ls.beacon = LightState::Off;
    ls.running = LightState::Off;
    ls.rear_work = LightState::Off;
    ls.front_work = LightState::Off;
    ls.side_work = LightState::Off;
    ls.hazard = LightState::Off;
    ls.backup = LightState::Off;
    ls.center_stop = LightState::Off;
    ls.left_stop = LightState::Off;
    ls.right_stop = LightState::Off;

    auto data = ls.encode();
    CHECK(data[0] == 0x00);
    CHECK(data[1] == 0x00);
    CHECK(data[2] == 0x00);
    CHECK(data[3] == 0x00);
}

TEST_CASE("LightingState all not available") {
    LightingState ls; // default is NotAvailable
    auto data = ls.encode();
    CHECK(data[0] == 0xFF);
    CHECK(data[1] == 0xFF);
    CHECK(data[2] == 0xFF);
    CHECK(data[3] == 0xFF);
}

// ─── Tractor Facilities encode/decode ──────────────────────────────────────────

TEST_CASE("TractorFacilities encode/decode round-trip") {
    TractorFacilities tf;
    tf.rear_hitch_position = true;
    tf.rear_pto_speed = true;
    tf.wheel_based_speed = true;
    tf.ground_based_speed = true;
    tf.lighting = true;
    tf.navigation = true;
    tf.guidance = true;

    auto data = tf.encode();
    REQUIRE(data.size() == 8);

    auto decoded = TractorFacilities::decode(data);
    CHECK(decoded.rear_hitch_position == true);
    CHECK(decoded.rear_hitch_in_work == false);
    CHECK(decoded.rear_pto_speed == true);
    CHECK(decoded.rear_pto_engagement == false);
    CHECK(decoded.wheel_based_speed == true);
    CHECK(decoded.ground_based_speed == true);
    CHECK(decoded.lighting == true);
    CHECK(decoded.navigation == true);
    CHECK(decoded.guidance == true);
    CHECK(decoded.rear_hitch_command == false);
    CHECK(decoded.machine_selected_speed == false);
}

TEST_CASE("TractorFacilities set_class1_all") {
    TractorFacilities tf;
    tf.set_class1_all();
    CHECK(tf.rear_hitch_position == true);
    CHECK(tf.rear_hitch_in_work == true);
    CHECK(tf.rear_pto_speed == true);
    CHECK(tf.rear_pto_engagement == true);
    CHECK(tf.wheel_based_speed == true);
    CHECK(tf.ground_based_speed == true);
    // Class 2 should still be false
    CHECK(tf.ground_based_distance == false);
    CHECK(tf.lighting == false);
}

TEST_CASE("TractorFacilities set_class2_all sets class2 fields") {
    TractorFacilities tf;
    tf.set_class1_all().set_class2_all();
    // Class 1 should be set (via set_class1_all)
    CHECK(tf.rear_hitch_position == true);
    CHECK(tf.wheel_based_speed == true);
    // Class 2 should be set
    CHECK(tf.ground_based_distance == true);
    CHECK(tf.lighting == true);
    CHECK(tf.aux_valve_flow == true);
    // Class 3 should still be false
    CHECK(tf.rear_hitch_command == false);
}

TEST_CASE("TractorFacilities v2 Class 3 bits encode/decode") {
    TractorFacilities tf;
    tf.set_class3_v2_all();
    CHECK(tf.rear_hitch_limit_status == true);
    CHECK(tf.rear_hitch_exit_code == true);
    CHECK(tf.rear_pto_engagement_request == true);
    CHECK(tf.rear_pto_speed_limit_status == true);
    CHECK(tf.rear_pto_exit_code == true);
    CHECK(tf.aux_valve_limit_status == true);
    CHECK(tf.aux_valve_exit_code == true);

    auto data = tf.encode();
    // Byte 3 bits 2-7 should all be set
    CHECK((data[3] & 0xFC) == 0xFC);
    // Byte 4 bit 0 (aux_valve_exit_code) should be set
    CHECK((data[4] & 0x01) == 0x01);

    auto decoded = TractorFacilities::decode(data);
    CHECK(decoded.rear_hitch_limit_status == true);
    CHECK(decoded.rear_hitch_exit_code == true);
    CHECK(decoded.rear_pto_engagement_request == true);
    CHECK(decoded.rear_pto_speed_limit_status == true);
    CHECK(decoded.rear_pto_exit_code == true);
    CHECK(decoded.aux_valve_limit_status == true);
    CHECK(decoded.aux_valve_exit_code == true);
}

TEST_CASE("TractorFacilities v2 Front F addendum bits encode/decode") {
    TractorFacilities tf;
    tf.set_front_v2_all();
    CHECK(tf.front_hitch_limit_status == true);
    CHECK(tf.front_hitch_exit_code == true);
    CHECK(tf.front_pto_engagement_request == true);
    CHECK(tf.front_pto_speed_limit_status == true);
    CHECK(tf.front_pto_exit_code == true);

    auto data = tf.encode();
    // Byte 4 bits 1-5 should all be set
    CHECK((data[4] & 0x3E) == 0x3E);

    auto decoded = TractorFacilities::decode(data);
    CHECK(decoded.front_hitch_limit_status == true);
    CHECK(decoded.front_hitch_exit_code == true);
    CHECK(decoded.front_pto_engagement_request == true);
    CHECK(decoded.front_pto_speed_limit_status == true);
    CHECK(decoded.front_pto_exit_code == true);
}

TEST_CASE("TractorFacilities v2 all combined with v1 class3") {
    TractorFacilities tf;
    tf.set_class1_all().set_class2_all().set_class3_all().set_class3_v2_all().set_front_v2_all();
    tf.machine_selected_speed = true;
    tf.machine_selected_speed_command = true;

    auto data = tf.encode();
    auto decoded = TractorFacilities::decode(data);

    // v1 Class 3 should be preserved
    CHECK(decoded.rear_hitch_command == true);
    CHECK(decoded.rear_pto_command == true);
    CHECK(decoded.aux_valve_command == true);

    // Powertrain (P addendum) should be preserved
    CHECK(decoded.machine_selected_speed == true);
    CHECK(decoded.machine_selected_speed_command == true);

    // v2 should be set
    CHECK(decoded.rear_hitch_limit_status == true);
    CHECK(decoded.aux_valve_exit_code == true);
    CHECK(decoded.front_pto_exit_code == true);
}

// ─── Machine Selected Speed encode/decode ──────────────────────────────────────

TEST_CASE("MachineSelectedSpeedMsg encode/decode") {
    MachineSelectedSpeedMsg msg;
    msg.speed_raw = 5000; // 5.0 m/s
    msg.direction = MachineDirection::Forward;
    msg.source = SpeedSource::GroundBased;
    msg.limit_status = SpeedExitCode::NotLimited;

    CHECK(msg.speed_mps() == doctest::Approx(5.0));

    auto data = msg.encode();
    REQUIRE(data.size() == 8);

    auto decoded = MachineSelectedSpeedMsg::decode(data);
    CHECK(decoded.speed_raw == 5000);
    CHECK(decoded.direction == MachineDirection::Forward);
    CHECK(decoded.source == SpeedSource::GroundBased);
    CHECK(decoded.limit_status == SpeedExitCode::NotLimited);
    CHECK(decoded.speed_mps() == doctest::Approx(5.0));
}

TEST_CASE("MachineSpeedCommandMsg encode/decode") {
    MachineSpeedCommandMsg cmd;
    cmd.set_speed_mps(3.5).set_direction(MachineDirection::Forward);

    CHECK(cmd.target_speed_mps() == doctest::Approx(3.5).epsilon(0.001));

    auto data = cmd.encode();
    REQUIRE(data.size() == 8);

    auto decoded = MachineSpeedCommandMsg::decode(data);
    CHECK(decoded.target_speed_raw == cmd.target_speed_raw);
    CHECK(decoded.direction_cmd == MachineDirection::Forward);
}

TEST_CASE("MachineSelectedSpeedMsg not available") {
    MachineSelectedSpeedMsg msg; // defaults
    CHECK(msg.speed_raw == 0xFFFF);
    CHECK(msg.speed_mps() == 0.0);
}

// ─── Auxiliary Valve Flow encode/decode ────────────────────────────────────────

TEST_CASE("AuxValveFlowMsg encode/decode") {
    AuxValveFlowMsg msg;
    msg.valve_index = 3;
    msg.extend_flow_percent = 125; // 50%
    msg.retract_flow_percent = 0;  // 0%
    msg.state = ValveState::Extending;
    msg.limit_status = ValveLimitStatus::NotLimited;
    msg.fail_safe = ValveFailSafe::Block;

    CHECK(msg.extend_flow() == doctest::Approx(50.0));
    CHECK(msg.retract_flow() == doctest::Approx(0.0));

    auto data = msg.encode();
    REQUIRE(data.size() == 8);

    auto decoded = AuxValveFlowMsg::decode(data, 3);
    CHECK(decoded.valve_index == 3);
    CHECK(decoded.extend_flow_percent == 125);
    CHECK(decoded.retract_flow_percent == 0);
    CHECK(decoded.state == ValveState::Extending);
    CHECK(decoded.limit_status == ValveLimitStatus::NotLimited);
    CHECK(decoded.fail_safe == ValveFailSafe::Block);
}

TEST_CASE("AuxValveFlowMsg not available defaults") {
    AuxValveFlowMsg msg; // defaults
    CHECK(msg.extend_flow_percent == 0xFF);
    CHECK(msg.extend_flow() == 0.0);
}

TEST_CASE("AuxValveFlowMsg all valve states") {
    for (u8 s = 0; s < 4; ++s) {
        AuxValveFlowMsg msg;
        msg.state = static_cast<ValveState>(s);
        msg.extend_flow_percent = 100;
        msg.retract_flow_percent = 50;
        auto data = msg.encode();
        auto decoded = AuxValveFlowMsg::decode(data, 0);
        CHECK(decoded.state == static_cast<ValveState>(s));
    }
}

// ─── PGN constants ─────────────────────────────────────────────────────────────

TEST_CASE("Lighting PGN values") {
    CHECK(PGN_LIGHTING_DATA == 0xFE40);
    CHECK(PGN_LIGHTING_COMMAND == 0xFE41);
}

TEST_CASE("Tractor Facilities PGN values") {
    CHECK(PGN_TRACTOR_FACILITIES_RESPONSE == 0xFE09);
    CHECK(PGN_REQUIRED_TRACTOR_FACILITIES == 0xFE0A);
}

TEST_CASE("Machine Speed PGN values") {
    CHECK(PGN_MACHINE_SELECTED_SPEED_CMD == 0xFD43);
    CHECK(PGN_MACHINE_SPEED == 0xF022); // from constants.hpp
}

TEST_CASE("Aux Valve Flow PGN values") {
    CHECK(PGN_AUX_VALVE_ESTIMATED_FLOW_BASE == 0xFE10);
    CHECK(PGN_AUX_VALVE_MEASURED_FLOW_BASE == 0xFE20);
    // Valve 5 estimated should be 0xFE15
    CHECK((PGN_AUX_VALVE_ESTIMATED_FLOW_BASE + 5) == 0xFE15);
    // Valve 15 measured should be 0xFE2F
    CHECK((PGN_AUX_VALVE_MEASURED_FLOW_BASE + 15) == 0xFE2F);
}
