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
    echo::info("=== Tractor Implement Management Demo ===");

    // Create two ECUs over the same vcan interface
    auto link1_result = wirebit::SocketCanLink::create({
        .interface_name = "vcan_tim",
        .create_if_missing = true
    });
    if (!link1_result.is_ok()) { echo::error("Link1 create failed"); return 1; }
    auto link1 = std::make_shared<wirebit::SocketCanLink>(std::move(link1_result.value()));
    wirebit::CanEndpoint endpoint1(std::static_pointer_cast<wirebit::Link>(link1),
                                   wirebit::CanConfig{.bitrate = 250000}, 1);

    auto link2_result = wirebit::SocketCanLink::attach("vcan_tim");
    if (!link2_result.is_ok()) { echo::error("Link2 attach failed"); return 1; }
    auto link2 = std::make_shared<wirebit::SocketCanLink>(std::move(link2_result.value()));
    wirebit::CanEndpoint endpoint2(std::static_pointer_cast<wirebit::Link>(link2),
                                   wirebit::CanConfig{.bitrate = 250000}, 2);

    // Tractor ECU
    IsoNet nm_tractor;
    nm_tractor.set_endpoint(0, &endpoint1);
    auto* tractor_cf = nm_tractor.create_internal(
        Name::build().set_identity_number(1).set_manufacturer_code(100).set_function_code(25).set_self_configurable(true),
        0, 0x28).value();

    TimServer tim_server(nm_tractor, tractor_cf);
    tim_server.initialize();
    tim_server.set_aux_valve_capabilities(0, true, true);
    tim_server.set_aux_valve_capabilities(1, true, false);

    // Implement ECU
    IsoNet nm_impl;
    nm_impl.set_endpoint(0, &endpoint2);
    auto* impl_cf = nm_impl.create_internal(
        Name::build().set_identity_number(2).set_manufacturer_code(200).set_function_code(30).set_self_configurable(true),
        0, 0x30).value();

    TimClient tim_client(nm_impl, impl_cf);
    tim_client.initialize();

    tim_client.on_front_pto_updated.subscribe([](const PTOState& s) {
        echo::info("Client: Front PTO updated - engaged=", s.engaged, " speed=", s.speed, " RPM");
    });

    tim_client.on_rear_hitch_updated.subscribe([](const HitchState& h) {
        echo::info("Client: Rear hitch updated - position=", h.position);
    });

    // Simulate tractor operations
    tim_server.set_front_pto(true, true, 540);
    tim_server.set_rear_pto(true, true, 1000);
    tim_server.set_front_hitch(true, 5000);
    tim_server.set_rear_hitch(true, 7500);
    tim_server.set_aux_valve(0, true, 500);

    echo::info("Tractor: Front PTO = ", tim_server.get_front_pto().speed, " RPM");
    echo::info("Tractor: Rear hitch = ", tim_server.get_rear_hitch().position, " (0.01%)");
    echo::info("Tractor: Aux valve 0 flow = ", tim_server.get_aux_valve(0).flow);

    // Run update loop
    for (u32 i = 0; i < 20; ++i) {
        nm_tractor.update(50);
        nm_impl.update(50);
        tim_server.update(50);
        tim_client.update(50);
    }

    echo::info("=== TIM Demo Complete ===");
    return 0;
}
