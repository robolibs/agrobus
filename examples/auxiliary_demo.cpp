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
    echo::info("=== Auxiliary Functions Demo ===");

    auto link_result = wirebit::SocketCanLink::create({
        .interface_name = "vcan_aux",
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

    auto* cf = nm.create_internal(
        Name::build().set_identity_number(5).set_manufacturer_code(100).set_self_configurable(true),
        0, 0x30).value();

    // AUX-O interface
    AuxOInterface aux_o(nm, cf);
    aux_o.initialize();

    aux_o.add_function(0, AuxFunctionType::Type0); // On/Off valve
    aux_o.add_function(1, AuxFunctionType::Type1); // Variable speed
    aux_o.add_function(2, AuxFunctionType::Type2); // Variable position

    aux_o.on_function_changed.subscribe([](const AuxOFunction& f) {
        echo::info("AUX-O function ", f.function_number, " changed: setpoint=", f.setpoint);
    });

    // Simulate setting functions
    aux_o.set_function(0, AuxFunctionType::Type0, 1);      // Turn on
    aux_o.set_function(1, AuxFunctionType::Type1, 7500);   // 75% speed
    aux_o.set_function(2, AuxFunctionType::Type2, 3000);   // 30% position

    // AUX-N interface
    AuxNInterface aux_n(nm, cf);
    aux_n.initialize();

    aux_n.add_function(0, AuxFunctionType::Type0);
    aux_n.add_function(1, AuxFunctionType::Type2);

    aux_n.on_function_changed.subscribe([](const AuxNFunction& f) {
        echo::info("AUX-N function ", f.function_number, " changed: setpoint=", f.setpoint);
    });

    aux_n.set_function(0, AuxFunctionType::Type0, 1);
    aux_n.set_function(1, AuxFunctionType::Type2, 45000);

    echo::info("AUX-O functions: ", aux_o.functions().size());
    echo::info("AUX-N functions: ", aux_n.functions().size());
    echo::info("=== Auxiliary Demo Complete ===");
    return 0;
}
