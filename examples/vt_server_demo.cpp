#include <agrobus.hpp>
#include <wirebit/can/socketcan_link.hpp>
#include <wirebit/can/can_endpoint.hpp>
#include <agrobus/isobus/vt/server.hpp>
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
    echo::info("=== VT Server Demo ===");

    auto link_result = wirebit::SocketCanLink::create({
        .interface_name = "vcan_vtsrv",
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

    Name vt_name = Name::build()
                       .set_identity_number(1)
                       .set_manufacturer_code(100)
                       .set_function_code(25)
                       .set_industry_group(2)
                       .set_self_configurable(true);
    auto* cf = nm.create_internal(vt_name, 0, 0x26).value();

    VTServer vt_server(nm, cf, VTServerConfig{}.screen(480, 272).version(4));

    vt_server.on_state_change.subscribe([](VTServerState state) {
        echo::info("VT Server state: ", static_cast<u8>(state));
    });

    vt_server.on_client_connected.subscribe([](Address addr) {
        echo::info("Client connected: 0x", addr);
    });

    vt_server.on_numeric_value_change.subscribe([](ObjectID id, u32 value) {
        echo::info("Numeric value change: obj=", id, " value=", value);
    });

    vt_server.start();

    for (u32 i = 0; i < 50; ++i) {
        nm.update(100);
        vt_server.update(100);
    }

    echo::info("=== VT Server Demo Complete ===");
    return 0;
}
