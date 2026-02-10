#include <agrobus/isobus/vt/client.hpp>
#include <agrobus/isobus/vt/objects.hpp>
#include <agrobus/net/network_manager.hpp>
#include <echo/echo.hpp>

using namespace agrobus::net;
using namespace agrobus::isobus;
using namespace agrobus::isobus::vt;

int main() {
    echo::info("=== VT Gauge Demo ===");

    IsoNet nm;
    Name name = Name::build()
        .set_identity_number(5)
        .set_manufacturer_code(42)
        .set_function_code(50)
        .set_industry_group(2); // Agriculture

    auto cf_result = nm.create_internal(name, 0, 0x28);
    auto* cf = cf_result.value();

    // Create object pool with gauges
    ObjectPool pool;

    pool.add(VTObject{}.set_id(0).set_type(ObjectType::WorkingSet)
        .set_body({static_cast<u8>(480 & 0xFF), static_cast<u8>((480 >> 8) & 0xFF),
                   static_cast<u8>(480 & 0xFF), static_cast<u8>((480 >> 8) & 0xFF)}));

    pool.add(VTObject{}.set_id(100).set_type(ObjectType::DataMask)
        .set_body({static_cast<u8>(480 & 0xFF), static_cast<u8>((480 >> 8) & 0xFF),
                   static_cast<u8>(480 & 0xFF), static_cast<u8>((480 >> 8) & 0xFF)}));

    pool.add(VTObject{}.set_id(200).set_type(ObjectType::Meter)
        .set_body({static_cast<u8>(200 & 0xFF), static_cast<u8>((200 >> 8) & 0xFF),
                   static_cast<u8>(200 & 0xFF), static_cast<u8>((200 >> 8) & 0xFF)}));

    pool.add(VTObject{}.set_id(201).set_type(ObjectType::Meter)
        .set_body({static_cast<u8>(200 & 0xFF), static_cast<u8>((200 >> 8) & 0xFF),
                   static_cast<u8>(200 & 0xFF), static_cast<u8>((200 >> 8) & 0xFF)}));

    pool.add(VTObject{}.set_id(300).set_type(ObjectType::OutputNumber)
        .set_body({static_cast<u8>(100 & 0xFF), static_cast<u8>((100 >> 8) & 0xFF),
                   static_cast<u8>(30 & 0xFF), static_cast<u8>((30 >> 8) & 0xFF)}));

    pool.add(VTObject{}.set_id(301).set_type(ObjectType::OutputNumber)
        .set_body({static_cast<u8>(100 & 0xFF), static_cast<u8>((100 >> 8) & 0xFF),
                   static_cast<u8>(30 & 0xFF), static_cast<u8>((30 >> 8) & 0xFF)}));

    pool.add(VTObject{}.set_id(400).set_type(ObjectType::OutputString)
        .set_body({static_cast<u8>(200 & 0xFF), static_cast<u8>((200 >> 8) & 0xFF),
                   static_cast<u8>(20 & 0xFF), static_cast<u8>((20 >> 8) & 0xFF)}));

    echo::info("Object pool created with ", pool.size(), " objects");

    // Set up VT client
    VTClient vt(nm, cf);
    vt.set_object_pool(std::move(pool));

    // Handle button events
    vt.on_button.subscribe([](ObjectID id, ActivationCode code) {
        echo::info("Button pressed: id=", id, " code=", static_cast<u8>(code));
    });

    vt.on_numeric_value_change.subscribe([](ObjectID id, u32 value) {
        echo::info("Numeric value changed: id=", id, " value=", value);
    });

    vt.on_state_change.subscribe([&](VTState state) {
        if (state == VTState::Connected) {
            echo::info("VT connected! Updating gauges...");
        }
    });

    // Connect
    auto connect_result = vt.connect();
    if (connect_result.is_ok()) {
        echo::info("VT client connecting, waiting for VT Status...");
    } else {
        echo::error("Failed to start VT client");
        return 1;
    }

    // Simulate gauge updates
    echo::info("\nSimulating gauge updates:");
    for (u32 i = 0; i < 10; ++i) {
        u32 speed_kmh = 5 + i * 2; // 5-23 km/h
        u32 rpm = 1200 + i * 100;  // 1200-2100 RPM

        echo::info("  Speed: ", speed_kmh, " km/h, RPM: ", rpm);

        // These would succeed once connected
        vt.change_numeric_value(300, speed_kmh * 10); // 0.1 km/h resolution
        vt.change_numeric_value(301, rpm);

        vt.update(100); // 100ms tick
    }

    echo::info("\nDone.");
    return 0;
}
