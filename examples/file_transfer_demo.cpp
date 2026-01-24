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
    echo::info("=== File Transfer Demo ===");

    auto link_result = wirebit::SocketCanLink::create({
        .interface_name = "vcan_fxfer",
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

    // Setup file server
    FileServer server(nm, cf, FileServerConfig{}.path("/isobus/data"));
    server.initialize();
    server.add_file("task_data.xml");
    server.add_file("prescription_01.bin");
    server.add_file("device_log.csv");

    server.on_file_read_request.subscribe([](dp::String filename, Address addr) {
        echo::info("File read request: '", filename, "' from 0x", addr);
    });

    server.on_file_delete_request.subscribe([](dp::String filename, Address) {
        echo::info("File delete request: '", filename, "'");
    });

    // List files
    const auto& files = server.files();
    echo::info("Files available: ", files.size());
    for (const auto& f : files) {
        echo::info("  - ", f.name);
    }

    // Setup file client
    FileClient client(nm, cf);
    client.initialize();

    client.on_file_opened.subscribe([](u8 handle) {
        echo::info("File opened with handle: ", handle);
    });

    client.on_file_count.subscribe([](u8 count) {
        echo::info("Server reports ", count, " files");
    });

    client.on_error.subscribe([](FileTransferError err, dp::String ctx) {
        echo::warn("File error: ", static_cast<u8>(err), " context: ", ctx);
    });

    echo::info("=== File Transfer Demo Complete ===");
    return 0;
}
