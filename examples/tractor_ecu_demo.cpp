#include <agrobus.hpp>
#include <wirebit/can/socketcan_link.hpp>
#include <wirebit/can/can_endpoint.hpp>
#include <agrobus/isobus/tc/server.hpp>
#include <echo/echo.hpp>

using namespace agrobus::net;
using namespace agrobus::j1939;
using namespace agrobus::isobus;
using namespace agrobus::nmea;
using namespace agrobus::isobus::tc;
using namespace agrobus::isobus::vt;
using namespace agrobus::isobus::sc;
using namespace agrobus::isobus::implement;
using namespace agrobus::isobus::fs;

int main() {
    echo::info("=== Tractor ECU Demo (TIM + TC Server) ===");

    auto link_result = wirebit::SocketCanLink::create({
        .interface_name = "vcan_tecu",
        .create_if_missing = true
    });
    if (!link_result.is_ok()) { echo::error("SocketCanLink failed"); return 1; }
    auto link = std::make_shared<wirebit::SocketCanLink>(std::move(link_result.value()));
    wirebit::CanEndpoint endpoint(std::static_pointer_cast<wirebit::Link>(link),
                                  wirebit::CanConfig{.bitrate = 250000}, 1);

    IsoNet nm;
    nm.set_endpoint(0, &endpoint);

    Name tractor_name = Name::build()
                            .set_identity_number(1)
                            .set_manufacturer_code(100)
                            .set_function_code(25)
                            .set_industry_group(2)
                            .set_self_configurable(true);
    auto* cf = nm.create_internal(tractor_name, 0, 0x28).value();

    // --- TIM Server ---
    TimServer tim(nm, cf);
    tim.initialize();

    // Configure initial state
    tim.set_front_pto(false, true, 0);
    tim.set_rear_pto(false, true, 0);
    tim.set_front_hitch(false, 0);
    tim.set_rear_hitch(false, 0);

    // Configure aux valves
    for (u8 i = 0; i < 4; ++i) {
        tim.set_aux_valve_capabilities(i, true, true);
    }

    // --- TC Server ---
    TaskControllerServer tc(nm, cf, TCServerConfig{}
        .number(0)
        .version(4)
        .booms(1)
        .sections(12)
        .channels(4));

    tc.on_client_connected.subscribe([](Address addr) {
        echo::info("TC Client connected: 0x", addr);
    });

    tc.on_value_received([](ElementNumber elem, DDI ddi, i32 value, TCClientInfo*)
                             -> Result<ProcessDataAcknowledgeErrorCodes> {
        echo::info("Process data: elem=", elem, " ddi=", ddi, " val=", value);
        return Result<ProcessDataAcknowledgeErrorCodes>::ok(ProcessDataAcknowledgeErrorCodes::NoError);
    });

    // --- Functionalities ---
    ControlFunctionFunctionalities func(nm, cf);
    func.initialize();
    func.set_functionality_is_supported(Functionality::BasicTractorECUServer, 1, true);
    func.set_functionality_is_supported(Functionality::TractorImplementManagementServer, 1, true);
    func.set_functionality_is_supported(Functionality::TaskControllerBasicServer, 1, true);
    func.set_basic_tractor_ECU_server_option_state(BasicTractorECUOptions::Class1NoOptions, true);
    func.set_basic_tractor_ECU_server_option_state(BasicTractorECUOptions::GuidanceOption, true);
    func.set_minimum_control_function_option_state(MinimumControlFunctionOptions::SupportOfHeartbeatProducer, true);

    // --- Heartbeat ---
    HeartbeatProtocol heartbeat(nm, cf, HeartbeatConfig{}.interval(100));
    heartbeat.initialize();
    heartbeat.enable();

    // --- Speed ---
    SpeedDistanceInterface speed(nm, cf);
    speed.initialize();

    tc.start();
    echo::info("Tractor ECU started");
    echo::info("  TIM: PTO, Hitch, 4 aux valves");
    echo::info("  TC: 1 boom, 12 sections, 4 channels");
    echo::info("  Functionalities: TECU Server, TIM Server, TC Basic Server");

    // Simulate operation
    tim.set_rear_pto(true, true, 540);
    tim.set_rear_hitch(true, 7500);
    echo::info("Rear PTO engaged at 540 RPM, hitch at 75%");

    for (u32 i = 0; i < 30; ++i) {
        nm.update(100);
        tim.update(100);
        tc.update(100);
        heartbeat.update(100);
        func.update(100);

        if (i % 10 == 0) {
            speed.send_wheel_speed(3.0 + i * 0.1, 0.0);
        }
    }

    echo::info("=== Tractor ECU Demo Complete ===");
    return 0;
}
