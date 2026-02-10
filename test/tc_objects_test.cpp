#include <doctest/doctest.h>
#include <agrobus/isobus/tc/objects.hpp>
#include <cstring>

using namespace agrobus::isobus;
using namespace agrobus::isobus::tc;

TEST_CASE("DeviceObject serialization") {
    DeviceObject dev;
    dev.id = 1;
    dev.designator = "Sprayer";
    dev.software_version = "1.0";
    dev.serial_number = "SN001";

    auto bytes = dev.serialize();

    // Type byte
    CHECK(bytes[0] == static_cast<u8>(TCObjectType::Device));
    // Object ID little-endian
    CHECK(bytes[1] == 0x01);
    CHECK(bytes[2] == 0x00);
    // Designator length
    CHECK(bytes[3] == 7); // "Sprayer"
    // Designator string
    CHECK(bytes[4] == 'S');
    CHECK(bytes[10] == 'r');
}

TEST_CASE("DeviceElement serialization") {
    DeviceElement elem;
    elem.id = 10;
    elem.type = DeviceElementType::Section;
    elem.number = 1;
    elem.parent_id = 5;
    elem.designator = "Boom";
    elem.child_objects = {20, 21, 22};

    auto bytes = elem.serialize();

    CHECK(bytes[0] == static_cast<u8>(TCObjectType::DeviceElement));
    CHECK(bytes[1] == 10); // id low byte
    CHECK(bytes[2] == 0);  // id high byte
    CHECK(bytes[3] == static_cast<u8>(DeviceElementType::Section));
    CHECK(bytes[4] == 4); // designator length "Boom"
}

TEST_CASE("DeviceProcessData serialization") {
    DeviceProcessData pd;
    pd.id = 100;
    pd.ddi = 0x0001; // Setpoint Volume Per Area Application Rate
    pd.trigger_methods = static_cast<u8>(TriggerMethod::OnChange) |
                          static_cast<u8>(TriggerMethod::TimeInterval);
    pd.presentation_object_id = 200;
    pd.designator = "Rate";

    auto bytes = pd.serialize();

    CHECK(bytes[0] == static_cast<u8>(TCObjectType::DeviceProcessData));
    // Object ID
    CHECK(bytes[1] == 100);
    CHECK(bytes[2] == 0);
    // DDI little-endian
    CHECK(bytes[3] == 0x01);
    CHECK(bytes[4] == 0x00);
    // Trigger methods
    CHECK(bytes[5] == (0x08 | 0x01));
    // Presentation object ID
    CHECK(bytes[6] == 200);
    CHECK(bytes[7] == 0);
    // Designator length
    CHECK(bytes[8] == 4); // "Rate"
}

TEST_CASE("DeviceProperty serialization") {
    DeviceProperty prop;
    prop.id = 50;
    prop.ddi = 0x0086; // Actual Working Width
    prop.value = 12000; // 12m in mm
    prop.presentation_object_id = 201;
    prop.designator = "Width";

    auto bytes = prop.serialize();

    CHECK(bytes[0] == static_cast<u8>(TCObjectType::DeviceProperty));
    CHECK(bytes[1] == 50);
    CHECK(bytes[2] == 0);
    // DDI
    CHECK(bytes[3] == 0x86);
    CHECK(bytes[4] == 0x00);
    // Value (12000 = 0x00002EE0)
    CHECK(bytes[5] == 0xE0);
    CHECK(bytes[6] == 0x2E);
    CHECK(bytes[7] == 0x00);
    CHECK(bytes[8] == 0x00);
    // Presentation object ID
    CHECK(bytes[9] == 201);
    CHECK(bytes[10] == 0);
}

TEST_CASE("DeviceValuePresentation serialization") {
    DeviceValuePresentation vp;
    vp.id = 200;
    vp.offset = -100;
    vp.scale = 0.1f;
    vp.decimal_digits = 1;
    vp.unit_designator = "L/ha";

    auto bytes = vp.serialize();

    CHECK(bytes[0] == static_cast<u8>(TCObjectType::DeviceValuePresentation));
    CHECK(bytes[1] == 200);
    CHECK(bytes[2] == 0);
    // Offset (-100 as i32 LE)
    i32 offset_val = -100;
    CHECK(bytes[3] == static_cast<u8>(offset_val & 0xFF));
    // Scale (0.1f as IEEE 754)
    f32 scale_val = 0.1f;
    u32 scale_bits;
    std::memcpy(&scale_bits, &scale_val, sizeof(u32));
    CHECK(bytes[7] == static_cast<u8>(scale_bits & 0xFF));
    // Decimal digits
    CHECK(bytes[11] == 1);
    // Unit designator length
    CHECK(bytes[12] == 4); // "L/ha"
}

TEST_CASE("DDI type and ElementNumber") {
    DDI ddi = 0x0001;
    CHECK(ddi == 1);

    ElementNumber en = 0x000A;
    CHECK(en == 10);

    // Max DDI value (16 bits)
    DDI max_ddi = 0xFFFF;
    CHECK(max_ddi == 65535);
}

TEST_CASE("TCObjectType enum values") {
    CHECK(static_cast<u8>(TCObjectType::Device) == 0);
    CHECK(static_cast<u8>(TCObjectType::DeviceElement) == 1);
    CHECK(static_cast<u8>(TCObjectType::DeviceProcessData) == 2);
    CHECK(static_cast<u8>(TCObjectType::DeviceProperty) == 3);
    CHECK(static_cast<u8>(TCObjectType::DeviceValuePresentation) == 4);
}

TEST_CASE("DeviceElementType enum values") {
    CHECK(static_cast<u8>(DeviceElementType::Device) == 1);
    CHECK(static_cast<u8>(DeviceElementType::Function) == 2);
    CHECK(static_cast<u8>(DeviceElementType::Bin) == 3);
    CHECK(static_cast<u8>(DeviceElementType::Section) == 4);
    CHECK(static_cast<u8>(DeviceElementType::Unit) == 5);
    CHECK(static_cast<u8>(DeviceElementType::Connector) == 6);
    CHECK(static_cast<u8>(DeviceElementType::NavigationReference) == 7);
}

TEST_CASE("TriggerMethod bitmask combinations") {
    u8 methods = static_cast<u8>(TriggerMethod::TimeInterval) |
                 static_cast<u8>(TriggerMethod::DistanceInterval);
    CHECK(methods == 0x03);

    methods = static_cast<u8>(TriggerMethod::OnChange) |
              static_cast<u8>(TriggerMethod::Total);
    CHECK(methods == 0x18);

    // All methods combined
    u8 all = static_cast<u8>(TriggerMethod::TimeInterval) |
             static_cast<u8>(TriggerMethod::DistanceInterval) |
             static_cast<u8>(TriggerMethod::ThresholdLimits) |
             static_cast<u8>(TriggerMethod::OnChange) |
             static_cast<u8>(TriggerMethod::Total);
    CHECK(all == 0x1F);
}

TEST_CASE("DeviceElement with children") {
    DeviceElement elem;
    elem.id = 5;
    elem.type = DeviceElementType::Function;
    elem.number = 0;
    elem.parent_id = 1;
    elem.designator = "Fn";
    elem.child_objects = {10, 11, 12, 13, 14};

    auto bytes = elem.serialize();

    // Find the children count location (after type + id + type + designator + number + parent)
    // Type(1) + ID(2) + ElemType(1) + DesigLen(1) + Desig(2) + Number(2) + Parent(2) = 11
    usize offset = 11;
    u16 num_children = static_cast<u16>(bytes[offset]) | (static_cast<u16>(bytes[offset + 1]) << 8);
    CHECK(num_children == 5);

    // First child
    u16 child0 = static_cast<u16>(bytes[offset + 2]) | (static_cast<u16>(bytes[offset + 3]) << 8);
    CHECK(child0 == 10);

    // Last child
    u16 child4 = static_cast<u16>(bytes[offset + 10]) | (static_cast<u16>(bytes[offset + 11]) << 8);
    CHECK(child4 == 14);
}
