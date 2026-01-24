#include <agrobus.hpp>
#include <wirebit/can/socketcan_link.hpp>
#include <wirebit/can/can_endpoint.hpp>
#include <agrobus/isobus/tc/peer_control.hpp>
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
    echo::info("=== Peer Control Demo ===");

    auto link_result = wirebit::SocketCanLink::create({
        .interface_name = "vcan_peer",
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

    PeerControlInterface pc(nm, cf);
    pc.initialize();

    // Subscribe to events
    pc.on_assignment_added.subscribe([](const PeerControlAssignment& a) {
        echo::info("Assignment added: src_elem=", a.source_element,
                   " src_ddi=", a.source_ddi, " -> dst_elem=", a.destination_element);
    });

    pc.on_assignment_state_changed.subscribe([](const PeerControlAssignment& a) {
        echo::info("Assignment ", a.source_element, " active=", a.active);
    });

    // Create assignments: seeder rate controlled by GPS speed
    PeerControlAssignment speed_to_rate;
    speed_to_rate.source_element = 1;   // GPS element
    speed_to_rate.source_ddi = 0x0086;  // Actual speed
    speed_to_rate.destination_element = 10; // Seeder element
    speed_to_rate.destination_ddi = 0x0001; // Setpoint rate
    speed_to_rate.source_address = 0x28;
    speed_to_rate.destination_address = 0x30;
    pc.add_assignment(speed_to_rate);

    // Section control from TC to boom sections
    PeerControlAssignment section_ctrl;
    section_ctrl.source_element = 5;    // TC control
    section_ctrl.source_ddi = 0x0087;   // Section on/off
    section_ctrl.destination_element = 20; // Boom section
    section_ctrl.destination_ddi = 0x0087;
    pc.add_assignment(section_ctrl);

    echo::info("Total assignments: ", pc.assignments().size());

    // Activate first assignment
    pc.activate_assignment(1, 0x0086, true);
    echo::info("Speed->Rate assignment activated");

    // Deactivate
    pc.activate_assignment(1, 0x0086, false);
    echo::info("Speed->Rate assignment deactivated");

    // Remove an assignment
    pc.remove_assignment(5, 0x0087);
    echo::info("Section control assignment removed");
    echo::info("Remaining assignments: ", pc.assignments().size());

    echo::info("=== Peer Control Demo Complete ===");
    return 0;
}
