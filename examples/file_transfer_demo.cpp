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
    fs::FileServerEnhanced server(nm, cf, fs::FileServerConfig{});
    server.initialize();

    // Add some test files
    dp::Vector<u8> xml_data;
    xml_data.push_back(0xAA);
    xml_data.push_back(0xBB);
    server.add_file("task_data.xml", xml_data, fs::FileAttributes::ReadOnly);

    dp::Vector<u8> bin_data;
    for (int i = 0; i < 100; ++i) {
        bin_data.push_back(static_cast<u8>(i));
    }
    server.add_file("prescription_01.bin", bin_data);

    dp::Vector<u8> csv_data;
    server.add_file("device_log.csv", csv_data);

    echo::info("Files available on server");

    // Setup file client
    fs::FileClient client(nm, cf);
    client.initialize();

    client.on_file_opened.subscribe([](u8 handle, dp::String filename) {
        echo::info("File opened with handle: ", static_cast<int>(handle), " filename: ", filename);
    });

    client.on_error.subscribe([](fs::FSError err) {
        echo::warn("File error: ", static_cast<u8>(err));
    });

    echo::info("=== File Transfer Demo Complete ===");
    return 0;
}
