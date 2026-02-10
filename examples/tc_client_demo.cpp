#include <agrobus/net/network_manager.hpp>
#include <agrobus/isobus/tc/client.hpp>
#include <agrobus/isobus/tc/ddop.hpp>
#include <echo/echo.hpp>

using namespace agrobus::net;
using namespace agrobus::isobus;
using namespace agrobus::isobus::tc;

int main() {
    echo::info("=== Task Controller Client Demo ===");

    IsoNet nm;
    Name name = Name::build()
        .set_identity_number(1)
        .set_manufacturer_code(42)
        .set_function_code(25)       // Sprayer
        .set_device_class(7)         // Crop care
        .set_industry_group(2);      // Agriculture

    auto cf_result = nm.create_internal(name, 0, 0x28);
    auto* cf = cf_result.value();

    // Build DDOP for a simple sprayer
    DDOP ddop;

    auto device = DeviceObject{}
        .set_id(ddop.next_id())
        .set_designator("MySprayer")
        .set_software_version("1.0.0")
        .set_serial_number("SN-001");
    ddop.add_device(device);

    auto root = DeviceElement{}
        .set_id(ddop.next_id())
        .set_type(DeviceElementType::Device)
        .set_designator("Sprayer")
        .set_parent(device.id);
    ddop.add_element(root);

    auto section1 = DeviceElement{}
        .set_id(ddop.next_id())
        .set_type(DeviceElementType::Section)
        .set_number(1)
        .set_designator("Section1")
        .set_parent(root.id);
    ddop.add_element(section1);

    auto section2 = DeviceElement{}
        .set_id(ddop.next_id())
        .set_type(DeviceElementType::Section)
        .set_number(2)
        .set_designator("Section2")
        .set_parent(root.id);
    ddop.add_element(section2);

    auto rate = DeviceProcessData{}
        .set_id(ddop.next_id())
        .set_ddi(0x0001) // Setpoint Volume Per Area
        .set_triggers(static_cast<u8>(TriggerMethod::OnChange) |
                      static_cast<u8>(TriggerMethod::TimeInterval))
        .set_designator("AppRate");
    ddop.add_process_data(rate);

    auto width_prop = DeviceProperty{}
        .set_id(ddop.next_id())
        .set_ddi(0x0043) // Device Element Offset X
        .set_value(5000)  // 5m in mm
        .set_designator("Width");
    ddop.add_property(width_prop);

    echo::info("DDOP created: ", ddop.object_count(), " objects");

    // Validate
    auto validation = ddop.validate();
    echo::info("DDOP valid: ", validation.is_ok());

    // Serialize
    auto serialized = ddop.serialize();
    if (serialized) {
        echo::info("DDOP size: ", serialized.value().size(), " bytes");
    }

    // Create TC client
    TaskControllerClient tc(nm, cf);
    tc.set_ddop(std::move(ddop));

    // Set value callback
    tc.on_value_request([](ElementNumber elem, DDI ddi) -> Result<i32> {
        echo::info("TC requested value: elem=", elem, " ddi=", ddi);
        return Result<i32>::ok(1000); // Return 1000 as example value
    });

    // Set command callback
    tc.on_value_command([](ElementNumber elem, DDI ddi, i32 value) -> Result<void> {
        echo::info("TC command: elem=", elem, " ddi=", ddi, " value=", value);
        return {};
    });

    tc.on_state_change.subscribe([](TCState state) {
        echo::info("TC state changed: ", static_cast<int>(state));
    });

    // Connect (would need actual TC on the bus)
    auto connect_result = tc.connect();
    echo::info("TC connect initiated: ", connect_result.is_ok());
    echo::info("TC state: ", static_cast<int>(tc.state()));

    return 0;
}
