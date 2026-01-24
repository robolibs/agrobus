#include <agrobus.hpp>
#include <wirebit/can/socketcan_link.hpp>
#include <wirebit/can/can_endpoint.hpp>
#include <echo/echo.hpp>

using namespace agrobus::net;
using namespace agrobus::j1939;
using namespace agrobus::isobus;
using namespace agrobus::nmea;
using namespace agrobus::isobus::vt;
using namespace agrobus::isobus::tc;
using namespace agrobus::isobus::sc;
using namespace agrobus::isobus::implement;
using namespace agrobus::isobus::fs;

int main() {
    echo::info("=== Control Function Functionalities Demo ===");

    // Create SocketCAN link on virtual interface
    auto link_result = wirebit::SocketCanLink::create({
        .interface_name = "vcan_func",
        .create_if_missing = true
    });
    if (!link_result.is_ok()) {
        echo::error("Failed to create SocketCanLink");
        return 1;
    }
    auto link = std::make_shared<wirebit::SocketCanLink>(std::move(link_result.value()));
    wirebit::CanEndpoint endpoint(std::static_pointer_cast<wirebit::Link>(link),
                                  wirebit::CanConfig{.bitrate = 250000}, 1);

    IsoNet nm;
    nm.set_endpoint(0, &endpoint);

    Name ecu_name = Name::build()
                        .set_identity_number(42)
                        .set_manufacturer_code(100)
                        .set_function_code(25)
                        .set_industry_group(2)
                        .set_self_configurable(true);
    auto* cf = nm.create_internal(ecu_name, 0, 0x28).value();

    // Create functionalities interface
    ControlFunctionFunctionalities func(nm, cf);
    func.initialize();

    // Configure supported functionalities
    func.set_functionality_is_supported(Functionality::UniversalTerminalWorkingSet, 3, true);
    func.set_functionality_is_supported(Functionality::TaskControllerBasicClient, 2, true);
    func.set_functionality_is_supported(Functionality::TaskControllerGeoClient, 1, true);
    func.set_functionality_is_supported(Functionality::BasicTractorECUImplementClient, 1, true);
    func.set_functionality_is_supported(Functionality::TractorImplementManagementClient, 1, true);

    // Set minimum CF options
    func.set_minimum_control_function_option_state(MinimumControlFunctionOptions::SupportOfHeartbeatProducer, true);
    func.set_minimum_control_function_option_state(MinimumControlFunctionOptions::SupportOfHeartbeatConsumer, true);

    // Set TC GEO client option
    func.set_task_controller_geo_client_option(4);

    // Set Basic TECU options
    func.set_basic_tractor_ECU_implement_client_option_state(BasicTractorECUOptions::Class1NoOptions, true);
    func.set_basic_tractor_ECU_implement_client_option_state(BasicTractorECUOptions::GuidanceOption, true);

    // Set TIM client options
    func.set_tractor_implement_management_client_option_state(
        TractorImplementManagementOptions::RearHitchPositionIsSupported, true);
    func.set_tractor_implement_management_client_option_state(
        TractorImplementManagementOptions::VehicleSpeedInForwardDirectionIsSupported, true);

    // Subscribe to request events
    func.on_functionalities_request.subscribe([](const dp::Vector<u8>& data) {
        echo::info("Functionalities response sent, ", data.size(), " bytes");
    });

    // Serialize and display
    auto data = func.serialize();
    echo::info("Serialized functionalities: ", data.size(), " bytes");
    echo::info("Number of functionalities: ", data[0]);

    // List supported
    for (const auto& f : func.supported_functionalities()) {
        echo::info("  Functionality ", static_cast<u8>(f.functionality), " gen=", f.generation);
    }

    echo::info("=== Demo Complete ===");
    return 0;
}
