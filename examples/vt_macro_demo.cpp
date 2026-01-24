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
    echo::info("=== VT Macro Demo ===");

    auto link_result = wirebit::SocketCanLink::create({
        .interface_name = "vcan_vtmac",
        .create_if_missing = true
    });
    if (!link_result.is_ok()) { echo::error("SocketCanLink failed"); return 1; }
    auto link = std::make_shared<wirebit::SocketCanLink>(std::move(link_result.value()));
    wirebit::CanEndpoint endpoint(std::static_pointer_cast<wirebit::Link>(link),
                                  wirebit::CanConfig{.bitrate = 250000}, 1);

    IsoNet nm;
    nm.set_endpoint(0, &endpoint);

    auto* cf = nm.create_internal(
        Name::build().set_identity_number(1).set_manufacturer_code(100).set_self_configurable(true),
        0, 0x28).value();

    VTClient vt(nm, cf);

    // Register macros
    VTClient::VTMacro show_gauge;
    show_gauge.macro_id = 1;
    show_gauge.commands.push_back({vt_cmd::HIDE_SHOW, 0x0A, 0x00, 0x01, 0xFF, 0xFF, 0xFF, 0xFF}); // Show obj 10
    show_gauge.commands.push_back({vt_cmd::CHANGE_NUMERIC_VALUE, 0x14, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00}); // Reset obj 20

    VTClient::VTMacro hide_gauge;
    hide_gauge.macro_id = 2;
    hide_gauge.commands.push_back({vt_cmd::HIDE_SHOW, 0x0A, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF}); // Hide obj 10

    VTClient::VTMacro change_mask;
    change_mask.macro_id = 3;
    change_mask.commands.push_back({vt_cmd::CHANGE_ACTIVE_MASK, 0x00, 0x00, 0x05, 0x00, 0xFF, 0xFF, 0xFF}); // Switch to mask 5

    vt.register_macro(std::move(show_gauge));
    vt.register_macro(std::move(hide_gauge));
    vt.register_macro(std::move(change_mask));

    echo::info("Registered ", vt.macros().size(), " macros");

    // List registered macros
    for (const auto& m : vt.macros()) {
        echo::info("  Macro ID=", m.macro_id, " commands=", m.commands.size());
    }

    // Subscribe to macro events
    vt.on_macro_executed.subscribe([](ObjectID id) {
        echo::info("Macro executed: ", id);
    });

    // Attempt to execute (will fail since not connected)
    auto result = vt.execute_macro(1);
    if (!result.is_ok()) {
        echo::info("Execute macro failed (not connected) - expected");
    }

    // Demonstrate VT version setting
    vt.set_vt_version_preference(VTVersion::Version4);
    echo::info("VT version preference: ", vt.get_vt_version());

    echo::info("=== VT Macro Demo Complete ===");
    return 0;
}
