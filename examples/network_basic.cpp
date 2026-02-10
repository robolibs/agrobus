#include <agrobus/net/network_manager.hpp>
#include <echo/echo.hpp>

using namespace agrobus::net;

int main() {
    echo::info("=== Network Basic Demo ===");

    // Create network with custom config
    IsoNet nm(NetworkConfig{}.ports(1).bus_load(true));

    // Create our ECU
    Name our_name = Name::build()
        .set_identity_number(42)
        .set_manufacturer_code(100)
        .set_function_code(25)
        .set_industry_group(2)
        .set_self_configurable(true);

    auto our_cf = nm.create_internal(our_name, 0, 0x28);
    echo::info("Created internal CF: address=0x", our_cf.value()->preferred_address());

    // Create a partner to look for (e.g., a TC)
    dp::Vector<NameFilter> tc_filters = {
        {NameFilterField::FunctionCode, 0x89}, // Task Controller
        {NameFilterField::IndustryGroup, 2}    // Agriculture
    };
    auto tc_partner = nm.create_partner(0, std::move(tc_filters));
    echo::info("Created partner CF for TC");

    // Register message callback
    nm.on_message.subscribe([](const Message& msg) {
        echo::debug("Message received: PGN=0x", msg.pgn, " from=0x", msg.source);
    });

    // Register specific PGN callback
    nm.register_pgn_callback(PGN_VEHICLE_SPEED, [](const Message& msg) {
        if (msg.data.size() >= 2) {
            u16 raw_speed = msg.get_u16_le(0);
            f64 speed_mps = raw_speed * 0.001;
            echo::info("Vehicle speed: ", speed_mps, " m/s");
        }
    });

    // In a real application, you would:
    // 1. Create wirebit link + endpoint and register: nm.set_endpoint(0, &can_endpoint);
    // 2. Start address claiming: nm.start_address_claiming();
    // 3. Call nm.update(elapsed_ms) in a loop

    echo::info("Network ready. Bus load: ", nm.bus_load(0), "%");
    echo::info("Control functions: ", nm.control_functions().size());

    return 0;
}
