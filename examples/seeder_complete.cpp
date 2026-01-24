#include <agrobus.hpp>
#include <wirebit/can/socketcan_link.hpp>
#include <wirebit/can/can_endpoint.hpp>
#include <agrobus/isobus/tc/geo.hpp>
#include <agrobus/isobus/tc/peer_control.hpp>
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
    echo::info("=== Complete Seeder Implementation ===");

    // Create CAN bus via SocketCAN virtual interface
    auto link_result = wirebit::SocketCanLink::create({
        .interface_name = "vcan_seeder",
        .create_if_missing = true
    });
    if (!link_result.is_ok()) { echo::error("SocketCanLink failed"); return 1; }
    auto link = std::make_shared<wirebit::SocketCanLink>(std::move(link_result.value()));
    wirebit::CanEndpoint endpoint(std::static_pointer_cast<wirebit::Link>(link),
                                  wirebit::CanConfig{.bitrate = 250000}, 1);

    IsoNet nm;
    nm.set_endpoint(0, &endpoint);

    Name seeder_name = Name::build()
                           .set_identity_number(42)
                           .set_manufacturer_code(200)
                           .set_function_code(30)
                           .set_industry_group(2)
                           .set_self_configurable(true);
    auto* cf = nm.create_internal(seeder_name, 0, 0x30).value();

    // --- Functionalities ---
    ControlFunctionFunctionalities func(nm, cf);
    func.initialize();
    func.set_functionality_is_supported(Functionality::UniversalTerminalWorkingSet, 3, true);
    func.set_functionality_is_supported(Functionality::TaskControllerBasicClient, 2, true);
    func.set_functionality_is_supported(Functionality::TaskControllerGeoClient, 1, true);
    func.set_functionality_is_supported(Functionality::TractorImplementManagementClient, 1, true);
    func.set_minimum_control_function_option_state(MinimumControlFunctionOptions::SupportOfHeartbeatProducer, true);
    func.set_task_controller_geo_client_option(12); // 12 sections
    func.set_tractor_implement_management_client_option_state(
        TractorImplementManagementOptions::VehicleSpeedInForwardDirectionIsSupported, true);

    // --- VT Client with macros ---
    VTClient vt(nm, cf);

    // Create a simple object pool
    ObjectPool pool;

    pool.add(VTObject{}.set_id(0).set_type(ObjectType::WorkingSet)
        .set_body({static_cast<u8>(200 & 0xFF), static_cast<u8>((200 >> 8) & 0xFF),
                   static_cast<u8>(200 & 0xFF), static_cast<u8>((200 >> 8) & 0xFF)}));

    pool.add(VTObject{}.set_id(1).set_type(ObjectType::DataMask)
        .set_body({static_cast<u8>(200 & 0xFF), static_cast<u8>((200 >> 8) & 0xFF),
                   static_cast<u8>(200 & 0xFF), static_cast<u8>((200 >> 8) & 0xFF)}));

    pool.add(VTObject{}.set_id(10).set_type(ObjectType::NumberVariable));

    vt.set_object_pool(std::move(pool));

    // Register macro for quick rate display update
    VTClient::VTMacro update_rate;
    update_rate.macro_id = 1;
    update_rate.commands.push_back({vt_cmd::CHANGE_NUMERIC_VALUE, 0x0A, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00});
    vt.register_macro(std::move(update_rate));

    // --- TC Client (already in isobus.hpp) ---
    TaskControllerClient tc(nm, cf);
    DDOP ddop;

    ddop.add_device(DeviceObject{}
        .set_designator("Seeder Pro 3000")
        .set_software_version("1.0.0"));

    ddop.add_element(DeviceElement{}
        .set_type(DeviceElementType::Function)
        .set_designator("Main Boom"));

    tc.set_ddop(std::move(ddop));

    // --- TC-GEO ---
    TCGEOInterface geo(nm, cf);
    geo.initialize();
    geo.on_application_rate_changed.subscribe([](i32 rate) {
        echo::info("Seeder: Prescription rate = ", rate, " seeds/ha");
    });

    // --- TIM Client ---
    TimClient tim(nm, cf);
    tim.initialize();
    tim.on_rear_hitch_updated.subscribe([](const HitchState& h) {
        echo::info("Seeder: Hitch position = ", h.position / 100, "%");
    });

    // --- Heartbeat ---
    HeartbeatProtocol heartbeat(nm, cf, HeartbeatConfig{}.interval(100));
    heartbeat.initialize();
    heartbeat.enable();

    // --- Peer Control ---
    PeerControlInterface peer(nm, cf);
    peer.initialize();

    // Assign: speed -> seeding rate
    PeerControlAssignment speed_rate;
    speed_rate.source_element = 1;
    speed_rate.source_ddi = 0x0086; // Ground speed
    speed_rate.destination_element = 10;
    speed_rate.destination_ddi = 0x0001; // Application rate
    peer.add_assignment(speed_rate);

    echo::info("Seeder initialized:");
    echo::info("  Functionalities: VT WS, TC Basic, TC-GEO, TIM Client");
    echo::info("  Object pool: ", vt.macros().size(), " macros");
    echo::info("  TC-GEO: Ready for prescription maps");
    echo::info("  Peer control: speed->rate assignment");

    // Simulate operation
    for (u32 i = 0; i < 50; ++i) {
        nm.update(100);
        heartbeat.update(100);
        func.update(100);
        tim.update(100);
        geo.update(100);

        // Simulate GNSS position updates
        if (i % 10 == 0) {
            GeoPoint pt;
            pt.position = concord::earth::WGS(48.05 + i * 0.001, 11.05, 400);
            pt.timestamp_us = i * 100000;
            geo.set_position(pt);
        }
    }

    echo::info("=== Seeder Complete Demo Done ===");
    return 0;
}
