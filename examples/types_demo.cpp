#include <agrobus/net/types.hpp>
#include <agrobus/net/name.hpp>
#include <agrobus/net/identifier.hpp>
#include <agrobus/net/frame.hpp>
#include <agrobus/net/constants.hpp>
#include <echo/echo.hpp>

using namespace agrobus::net;

int main() {
    echo::info("=== ISOBUS Types Demo ===");

    // Create a NAME for our device
    Name name = Name::build()
        .set_identity_number(12345)
        .set_manufacturer_code(555)
        .set_function_code(25)       // Sprayer
        .set_device_class(7)         // Crop care
        .set_industry_group(2)       // Agriculture
        .set_self_configurable(true);

    echo::info("NAME raw: ", name.raw);
    echo::info("  Identity: ", name.identity_number());
    echo::info("  Manufacturer: ", name.manufacturer_code());
    echo::info("  Function: ", name.function_code());
    echo::info("  Device Class: ", name.device_class());
    echo::info("  Industry Group: ", name.industry_group());
    echo::info("  Self-configurable: ", name.self_configurable());

    // Serialize/deserialize
    auto bytes = name.to_bytes();
    Name restored = Name::from_bytes(bytes.data());
    echo::info("  Roundtrip OK: ", (restored == name));

    // Create a CAN identifier
    auto id = Identifier::encode(Priority::Default, PGN_VEHICLE_SPEED, 0x28, BROADCAST_ADDRESS);
    echo::info("\nIdentifier: 0x", id.raw);
    echo::info("  Priority: ", static_cast<int>(id.priority()));
    echo::info("  PGN: 0x", id.pgn());
    echo::info("  Source: 0x", id.source());
    echo::info("  Broadcast: ", id.is_broadcast());

    // Create a frame
    u8 payload[] = {0x88, 0x13, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF};
    Frame f = Frame::from_message(Priority::Default, PGN_VEHICLE_SPEED, 0x28, BROADCAST_ADDRESS, payload, 8);
    echo::info("\nFrame PGN: 0x", f.pgn());
    echo::info("  Data[0:1] = speed: ", f.data[0] | (f.data[1] << 8), " (raw)");

    return 0;
}
