#include <doctest/doctest.h>
#include <agrobus.hpp>

using namespace agrobus::net;
using namespace agrobus::j1939;
using namespace agrobus::isobus;
using namespace agrobus::nmea;
using namespace agrobus::isobus::vt;
using namespace agrobus::isobus::tc;
using namespace agrobus::isobus::sc;
using namespace agrobus::isobus::implement;
using namespace agrobus::isobus::fs;

TEST_CASE("ControlFunctionFunctionalities - basic functionality management") {
    IsoNet nm;
    auto cf_result = nm.create_internal(Name::build().set_identity_number(1).set_manufacturer_code(100), 0, 0x28);
    auto* cf = cf_result.value();

    ControlFunctionFunctionalities func(nm, cf);

    SUBCASE("MinimumControlFunction is always supported") {
        CHECK(func.get_functionality_is_supported(Functionality::MinimumControlFunction));
        CHECK(func.get_functionality_generation(Functionality::MinimumControlFunction) == 1);
    }

    SUBCASE("Add and remove functionalities") {
        func.set_functionality_is_supported(Functionality::UniversalTerminalWorkingSet, 2, true);
        CHECK(func.get_functionality_is_supported(Functionality::UniversalTerminalWorkingSet));
        CHECK(func.get_functionality_generation(Functionality::UniversalTerminalWorkingSet) == 2);

        func.set_functionality_is_supported(Functionality::UniversalTerminalWorkingSet, 0, false);
        CHECK_FALSE(func.get_functionality_is_supported(Functionality::UniversalTerminalWorkingSet));
    }

    SUBCASE("Update generation of existing functionality") {
        func.set_functionality_is_supported(Functionality::TaskControllerBasicClient, 1, true);
        CHECK(func.get_functionality_generation(Functionality::TaskControllerBasicClient) == 1);

        func.set_functionality_is_supported(Functionality::TaskControllerBasicClient, 3, true);
        CHECK(func.get_functionality_generation(Functionality::TaskControllerBasicClient) == 3);
    }

    SUBCASE("Generation returns 0 for unsupported functionality") {
        CHECK(func.get_functionality_generation(Functionality::FileServer) == 0);
    }
}

TEST_CASE("ControlFunctionFunctionalities - Minimum CF options") {
    IsoNet nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x28).value();
    ControlFunctionFunctionalities func(nm, cf);

    func.set_minimum_control_function_option_state(MinimumControlFunctionOptions::SupportOfHeartbeatProducer, true);
    CHECK(func.get_minimum_control_function_option_state(MinimumControlFunctionOptions::SupportOfHeartbeatProducer));
    CHECK_FALSE(
        func.get_minimum_control_function_option_state(MinimumControlFunctionOptions::SupportOfHeartbeatConsumer));

    func.set_minimum_control_function_option_state(MinimumControlFunctionOptions::SupportOfHeartbeatConsumer, true);
    CHECK(func.get_minimum_control_function_option_state(MinimumControlFunctionOptions::SupportOfHeartbeatConsumer));

    func.set_minimum_control_function_option_state(MinimumControlFunctionOptions::SupportOfHeartbeatProducer, false);
    CHECK_FALSE(
        func.get_minimum_control_function_option_state(MinimumControlFunctionOptions::SupportOfHeartbeatProducer));
    CHECK(func.get_minimum_control_function_option_state(MinimumControlFunctionOptions::SupportOfHeartbeatConsumer));
}

TEST_CASE("ControlFunctionFunctionalities - AUX-O options") {
    IsoNet nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x28).value();
    ControlFunctionFunctionalities func(nm, cf);

    func.set_aux_O_inputs_option_state(AuxOOptions::SupportsType0Function, true);
    func.set_aux_O_inputs_option_state(AuxOOptions::SupportsType2Function, true);
    CHECK(func.get_aux_O_inputs_option_state(AuxOOptions::SupportsType0Function));
    CHECK_FALSE(func.get_aux_O_inputs_option_state(AuxOOptions::SupportsType1Function));
    CHECK(func.get_aux_O_inputs_option_state(AuxOOptions::SupportsType2Function));

    func.set_aux_O_functions_option_state(AuxOOptions::SupportsType1Function, true);
    CHECK(func.get_aux_O_functions_option_state(AuxOOptions::SupportsType1Function));
    CHECK_FALSE(func.get_aux_O_functions_option_state(AuxOOptions::SupportsType0Function));
}

TEST_CASE("ControlFunctionFunctionalities - AUX-N options") {
    IsoNet nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x28).value();
    ControlFunctionFunctionalities func(nm, cf);

    func.set_aux_N_inputs_option_state(AuxNOptions::SupportsType0Function, true);
    func.set_aux_N_inputs_option_state(AuxNOptions::SupportsType14Function, true);
    CHECK(func.get_aux_N_inputs_option_state(AuxNOptions::SupportsType0Function));
    CHECK(func.get_aux_N_inputs_option_state(AuxNOptions::SupportsType14Function));
    CHECK_FALSE(func.get_aux_N_inputs_option_state(AuxNOptions::SupportsType7Function));
}

TEST_CASE("ControlFunctionFunctionalities - TC GEO options") {
    IsoNet nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x28).value();
    ControlFunctionFunctionalities func(nm, cf);

    func.set_task_controller_geo_server_option_state(
        TaskControllerGeoServerOptions::PolygonBasedPrescriptionMapsAreSupported, true);
    CHECK(func.get_task_controller_geo_server_option_state(
        TaskControllerGeoServerOptions::PolygonBasedPrescriptionMapsAreSupported));

    func.set_task_controller_geo_client_option(4);
    CHECK(func.get_task_controller_geo_client_option() == 4);
}

TEST_CASE("ControlFunctionFunctionalities - TC Section Control options") {
    IsoNet nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x28).value();
    ControlFunctionFunctionalities func(nm, cf);

    func.set_task_controller_section_control_server_option_state(3, 16);
    CHECK(func.get_task_controller_section_control_server_number_supported_booms() == 3);
    CHECK(func.get_task_controller_section_control_server_number_supported_sections() == 16);

    func.set_task_controller_section_control_client_option_state(2, 8);
    CHECK(func.get_task_controller_section_control_client_number_supported_booms() == 2);
    CHECK(func.get_task_controller_section_control_client_number_supported_sections() == 8);
}

TEST_CASE("ControlFunctionFunctionalities - Basic Tractor ECU options") {
    IsoNet nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x28).value();
    ControlFunctionFunctionalities func(nm, cf);

    func.set_basic_tractor_ECU_server_option_state(BasicTractorECUOptions::Class1NoOptions, true);
    func.set_basic_tractor_ECU_server_option_state(BasicTractorECUOptions::GuidanceOption, true);
    CHECK(func.get_basic_tractor_ECU_server_option_state(BasicTractorECUOptions::Class1NoOptions));
    CHECK(func.get_basic_tractor_ECU_server_option_state(BasicTractorECUOptions::GuidanceOption));
    CHECK_FALSE(func.get_basic_tractor_ECU_server_option_state(BasicTractorECUOptions::NavigationOption));
}

TEST_CASE("ControlFunctionFunctionalities - TIM options") {
    IsoNet nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x28).value();
    ControlFunctionFunctionalities func(nm, cf);

    func.set_tractor_implement_management_server_option_state(
        TractorImplementManagementOptions::FrontPTODisengagementIsSupported, true);
    func.set_tractor_implement_management_server_option_state(
        TractorImplementManagementOptions::RearHitchPositionIsSupported, true);
    CHECK(func.get_tractor_implement_management_server_option_state(
        TractorImplementManagementOptions::FrontPTODisengagementIsSupported));
    CHECK(func.get_tractor_implement_management_server_option_state(
        TractorImplementManagementOptions::RearHitchPositionIsSupported));
    CHECK_FALSE(func.get_tractor_implement_management_server_option_state(
        TractorImplementManagementOptions::GuidanceCurvatureIsSupported));
}

TEST_CASE("ControlFunctionFunctionalities - TIM aux valve options") {
    IsoNet nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x28).value();
    ControlFunctionFunctionalities func(nm, cf);

    func.set_tractor_implement_management_server_aux_valve_option(0, true, false);
    func.set_tractor_implement_management_server_aux_valve_option(5, true, true);
    func.set_tractor_implement_management_server_aux_valve_option(31, false, true);

    CHECK(func.get_tractor_implement_management_server_aux_valve_state_supported(0));
    CHECK_FALSE(func.get_tractor_implement_management_server_aux_valve_flow_supported(0));
    CHECK(func.get_tractor_implement_management_server_aux_valve_state_supported(5));
    CHECK(func.get_tractor_implement_management_server_aux_valve_flow_supported(5));
    CHECK_FALSE(func.get_tractor_implement_management_server_aux_valve_state_supported(31));
    CHECK(func.get_tractor_implement_management_server_aux_valve_flow_supported(31));

    // Out of range
    CHECK_FALSE(func.get_tractor_implement_management_server_aux_valve_state_supported(32));
}

TEST_CASE("ControlFunctionFunctionalities - serialization") {
    IsoNet nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x28).value();
    ControlFunctionFunctionalities func(nm, cf);

    func.set_functionality_is_supported(Functionality::UniversalTerminalWorkingSet, 2, true);
    func.set_minimum_control_function_option_state(MinimumControlFunctionOptions::SupportOfHeartbeatProducer, true);

    auto data = func.serialize();

    // Byte 0: count (2 functionalities)
    CHECK(data[0] == 2);

    // First: MinimumControlFunction (0), gen=1, option=0x04 (heartbeat producer)
    CHECK(data[1] == 0);  // functionality
    CHECK(data[2] == 1);  // generation
    CHECK(data[3] == 0x04);  // option: heartbeat producer

    // Second: UniversalTerminalWorkingSet (2), gen=2, option=0x00
    CHECK(data[4] == 2);  // functionality
    CHECK(data[5] == 2);  // generation
    CHECK(data[6] == 0x00);  // no options
}
