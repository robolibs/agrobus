#include <agrobus.hpp>
#include <echo/echo.hpp>
#include <wirebit/can/socketcan_link.hpp>
#include <wirebit/can/can_endpoint.hpp>
#include <csignal>
#include <atomic>

using namespace agrobus::net;
using namespace agrobus::j1939;
using namespace agrobus::isobus;
using namespace agrobus::nmea;
using namespace agrobus::isobus::vt;
using namespace agrobus::isobus::tc;
using namespace agrobus::isobus::sc;
using namespace agrobus::isobus::implement;
using namespace agrobus::isobus::fs;

static std::atomic<bool> running{true};
void signal_handler(int) { running = false; }

int main(int argc, char* argv[]) {
    echo::info("=== ISOBUS Full Stack Demo ===");

    dp::String interface = "vcan0";
    if (argc > 1) interface = argv[1];

    // --- Hardware (wirebit) ---
    auto link_result = wirebit::SocketCanLink::create({
        .interface_name = interface,
        .create_if_missing = true
    });
    if (!link_result.is_ok()) {
        echo::error("Failed to open ", interface, ": ", link_result.error().message);
        echo::info("Run: sudo modprobe vcan && sudo ip link add dev ", interface, " type vcan && sudo ip link set ", interface, " up");
        return 1;
    }
    auto link = std::make_shared<wirebit::SocketCanLink>(std::move(link_result.value()));
    wirebit::CanEndpoint can(link, wirebit::CanConfig{.bitrate = 250000}, 1);

    // --- Network ---
    IsoNet nm(NetworkConfig{}.bus_load(true));
    nm.set_endpoint(0, &can);

    Name our_name = Name::build()
        .set_identity_number(42)
        .set_manufacturer_code(100)
        .set_function_code(25)
        .set_device_class(7)
        .set_industry_group(2)
        .set_self_configurable(true);

    auto cf = nm.create_internal(our_name, 0, 0x28).value();

    // --- Protocols ---
    HeartbeatProtocol heartbeat(nm, cf, HeartbeatConfig{}.interval(100));
    heartbeat.initialize();
    heartbeat.enable();

    SpeedDistanceInterface speed(nm, cf);
    speed.initialize();

    DiagnosticProtocol diag(nm, cf);
    diag.initialize();

    GuidanceInterface guidance(nm, cf);
    guidance.initialize();

    NMEAInterface gnss(nm, cf);
    gnss.initialize();

    // --- TC Client ---
    tc::DDOP ddop;

    auto device_id = ddop.next_id();
    ddop.add_device(tc::DeviceObject{}
        .set_id(device_id)
        .set_designator("DemoDevice")
        .set_software_version("1.0"));

    ddop.add_element(tc::DeviceElement{}
        .set_id(ddop.next_id())
        .set_type(tc::DeviceElementType::Device)
        .set_designator("Root")
        .set_parent(device_id));

    tc::TaskControllerClient tc_client(nm, cf);
    tc_client.set_ddop(std::move(ddop));
    tc_client.on_value_request([](tc::ElementNumber, tc::DDI) -> Result<i32> {
        return Result<i32>::ok(0);
    });

    // --- VT Client ---
    vt::ObjectPool vt_pool;
    vt_pool.add(vt::VTObject{}.set_id(0).set_type(vt::ObjectType::WorkingSet));

    vt::VTClient vt_client(nm, cf);
    vt_client.set_object_pool(std::move(vt_pool));

    // --- Event handlers ---
    speed.on_speed_update.subscribe([](const SpeedData& sd) {
        if (sd.wheel_speed_mps) {
            echo::info("Speed: ", *sd.wheel_speed_mps * 3.6, " km/h");
        }
    });

    gnss.on_position.subscribe([](const GNSSPosition& pos) {
        echo::info("GNSS: ", pos.wgs.latitude, ", ", pos.wgs.longitude,
                   " fix=", static_cast<int>(pos.fix_type));
    });

    guidance.on_guidance_system.subscribe([](const GuidanceData& gd) {
        if (gd.curvature) {
            echo::info("Guidance curvature: ", *gd.curvature, " 1/m");
        }
    });

    // --- Start ---
    nm.start_address_claiming();
    tc_client.connect();
    vt_client.connect();

    signal(SIGINT, signal_handler);
    echo::info("Full stack running on ", interface, "... (Ctrl+C to stop)");

    while (running) {
        nm.update(10);
        heartbeat.update(10);
        diag.update(10);
        tc_client.update(10);
        vt_client.update(10);

        usleep(10000);
    }

    echo::info("\nBus load: ", nm.bus_load(0), "%");
    echo::info("Active DTCs: ", diag.active_dtcs().size());

    return 0;
}
