#include <doctest/doctest.h>
#include <agrobus.hpp>

using namespace agrobus::j1939;
using namespace agrobus::net;

// ═════════════════════════════════════════════════════════════════════════════
// DM4 Driver's Information Message Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("DM4Message encode and decode") {
    SUBCASE("DM4 with active DTCs") {
        DM4Message dm4;
        dm4.mil_status = LampStatus::On;
        dm4.red_stop_lamp = LampStatus::On;
        dm4.dtcs.push_back({.spn = 110, .fmi = FMI::AboveNormal, .occurrence_count = 5});
        dm4.dtcs.push_back({.spn = 94, .fmi = FMI::BelowNormal, .occurrence_count = 2});

        auto encoded = dm4.encode();
        CHECK(encoded.size() >= 2 + 4 * 2); // Header + 2 DTCs

        auto decoded = DM4Message::decode(encoded);
        CHECK(decoded.mil_status == LampStatus::On);
        CHECK(decoded.red_stop_lamp == LampStatus::On);
        CHECK(decoded.dtcs.size() == 2);
        CHECK(decoded.dtcs[0].spn == 110);
        CHECK(decoded.dtcs[0].occurrence_count == 5);
    }

    SUBCASE("DM4 with no DTCs") {
        DM4Message dm4;
        dm4.mil_status = LampStatus::Off;
        dm4.red_stop_lamp = LampStatus::Off;

        auto encoded = dm4.encode();
        auto decoded = DM4Message::decode(encoded);

        CHECK(decoded.mil_status == LampStatus::Off);
        CHECK(decoded.dtcs.empty());
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// DM6 Pending DTCs Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("DM6Message encode and decode") {
    SUBCASE("DM6 with pending DTCs") {
        DM6Message dm6;
        dm6.lamps.malfunction = LampStatus::On;
        dm6.lamps.red_stop = LampStatus::Off;
        dm6.lamps.amber_warning = LampStatus::On;
        dm6.pending_dtcs.push_back({.spn = 412, .fmi = FMI::BadDevice, .occurrence_count = 1});
        dm6.pending_dtcs.push_back({.spn = 190, .fmi = FMI::AboveNormal, .occurrence_count = 3});

        auto encoded = dm6.encode();
        CHECK(encoded.size() >= 2 + 4 * 2);

        auto decoded = DM6Message::decode(encoded);
        CHECK(decoded.lamps.malfunction == LampStatus::On);
        CHECK(decoded.lamps.amber_warning == LampStatus::On);
        CHECK(decoded.pending_dtcs.size() == 2);
        CHECK(decoded.pending_dtcs[0].spn == 412);
        CHECK(decoded.pending_dtcs[1].spn == 190);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// DM7/DM8 Test Command and Results Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("DM7Command encode and decode") {
    DM7Command cmd;
    cmd.test_id = 247;
    cmd.spn = 110;

    auto encoded = cmd.encode();
    CHECK(encoded.size() >= 4);

    auto decoded = DM7Command::decode(encoded);
    CHECK(decoded.test_id == 247);
    CHECK(decoded.spn == 110);
}

TEST_CASE("DM8TestResult encode and decode") {
    DM8TestResult result;
    result.test_id = 247;
    result.spn = 110;
    result.test_result = 0; // Passed
    result.test_value = 100;

    auto encoded = result.encode();
    CHECK(encoded.size() >= 4);

    auto decoded = DM8TestResult::decode(encoded);
    CHECK(decoded.test_id == 247);
    CHECK(decoded.spn == 110);
    CHECK(decoded.test_result == 0);
}

// ═════════════════════════════════════════════════════════════════════════════
// DM12 Emissions DTCs Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("DM12Message encode and decode") {
    DM12Message dm12;
    dm12.lamps.malfunction = LampStatus::On;
    dm12.lamps.amber_warning = LampStatus::On;
    dm12.emissions_dtcs.push_back({.spn = 3055, .fmi = FMI::AboveNormal, .occurrence_count = 7});
    dm12.emissions_dtcs.push_back({.spn = 3058, .fmi = FMI::BelowNormal, .occurrence_count = 4});

    auto encoded = dm12.encode();
    CHECK(encoded.size() >= 2 + 4 * 2);

    auto decoded = DM12Message::decode(encoded);
    CHECK(decoded.lamps.malfunction == LampStatus::On);
    CHECK(decoded.emissions_dtcs.size() == 2);
    CHECK(decoded.emissions_dtcs[0].spn == 3055);
    CHECK(decoded.emissions_dtcs[1].spn == 3058);
}

// ═════════════════════════════════════════════════════════════════════════════
// DM21 Diagnostic Readiness 2 Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("DM21Readiness encode and decode") {
    DM21Readiness dm21;
    dm21.distance_with_mil_on_km = 250;
    dm21.distance_since_codes_cleared_km = 1500;
    dm21.minutes_with_mil_on = 120;
    dm21.comprehensive_component = 1;
    dm21.fuel_system = 1;
    dm21.misfire = 1;

    auto encoded = dm21.encode();
    CHECK(encoded.size() >= 6);

    auto decoded = DM21Readiness::decode(encoded);
    CHECK(decoded.distance_with_mil_on_km == 250);
    CHECK(decoded.distance_since_codes_cleared_km == 1500);
    CHECK(decoded.minutes_with_mil_on == 120);
    CHECK(decoded.comprehensive_component == 1);
    CHECK(decoded.fuel_system == 1);
}

// ═════════════════════════════════════════════════════════════════════════════
// DM23 Previously MIL-OFF DTCs Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("DM23Message encode and decode") {
    DM23Message dm23;
    dm23.lamps.malfunction = LampStatus::Off;
    dm23.previously_mil_off_dtcs.push_back({.spn = 110, .fmi = FMI::AboveNormal, .occurrence_count = 10});
    dm23.previously_mil_off_dtcs.push_back({.spn = 94, .fmi = FMI::BelowNormal, .occurrence_count = 5});

    auto encoded = dm23.encode();
    CHECK(encoded.size() >= 2 + 4 * 2);

    auto decoded = DM23Message::decode(encoded);
    CHECK(decoded.lamps.malfunction == LampStatus::Off);
    CHECK(decoded.previously_mil_off_dtcs.size() == 2);
    CHECK(decoded.previously_mil_off_dtcs[0].spn == 110);
    CHECK(decoded.previously_mil_off_dtcs[0].occurrence_count == 10);
}
