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
    echo::info("=== TC Server Demo ===");

    auto link_result = wirebit::SocketCanLink::create({
        .interface_name = "vcan_tcsrv",
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

    Name tc_name = Name::build()
                       .set_identity_number(10)
                       .set_manufacturer_code(100)
                       .set_function_code(29)
                       .set_industry_group(2)
                       .set_self_configurable(true);
    auto* cf = nm.create_internal(tc_name, 0, 0x27).value();

    TaskControllerServer tc_server(nm, cf, TCServerConfig{}
        .number(0)
        .version(4)
        .booms(3)
        .sections(36)
        .channels(12)
        .options(static_cast<u8>(ServerOptions::SupportsDocumentation) |
                 static_cast<u8>(ServerOptions::SupportsImplementSectionControl)));

    tc_server.on_state_change.subscribe([](TCServerState state) {
        echo::info("TC Server state: ", static_cast<u8>(state));
    });

    tc_server.on_client_connected.subscribe([](Address addr) {
        echo::info("TC Client connected: 0x", addr);
    });

    tc_server.on_value_request([](ElementNumber elem, DDI ddi, TCClientInfo*) -> Result<i32> {
        echo::info("Value request: elem=", elem, " ddi=", ddi);
        return Result<i32>::ok(1000); // Default value
    });

    tc_server.on_value_received([](ElementNumber elem, DDI ddi, i32 value, TCClientInfo*)
                                     -> Result<ProcessDataAcknowledgeErrorCodes> {
        echo::info("Value received: elem=", elem, " ddi=", ddi, " value=", value);
        return Result<ProcessDataAcknowledgeErrorCodes>::ok(ProcessDataAcknowledgeErrorCodes::NoError);
    });

    tc_server.start();

    for (u32 i = 0; i < 30; ++i) {
        nm.update(100);
        tc_server.update(100);
    }

    echo::info("=== TC Server Demo Complete ===");
    return 0;
}
