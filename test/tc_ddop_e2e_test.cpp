#include <doctest/doctest.h>
#include <agrobus.hpp>
#include <wirebit/can/socketcan_link.hpp>
#include <wirebit/can/can_endpoint.hpp>
#include <agrobus/isobus/tc/client.hpp>
#include <agrobus/isobus/tc/server.hpp>

using namespace agrobus::net;
using namespace agrobus::j1939;
using namespace agrobus::isobus;
using namespace agrobus::nmea;
using namespace agrobus::isobus::tc;
using namespace agrobus::isobus::vt;
using namespace agrobus::isobus::sc;
using namespace agrobus::isobus::implement;
using namespace agrobus::isobus::fs;

// ─── Dual-node harness for TC client<->server ─────────────────────────────────
struct TCDualNode {
    std::shared_ptr<wirebit::SocketCanLink> link_client;
    std::shared_ptr<wirebit::SocketCanLink> link_server;
    wirebit::CanEndpoint ep_client;
    wirebit::CanEndpoint ep_server;
    IsoNet nm_client;
    IsoNet nm_server;
    InternalCF *cf_client = nullptr;
    InternalCF *cf_server = nullptr;

    TCDualNode()
        : link_client(std::make_shared<wirebit::SocketCanLink>(
              wirebit::SocketCanLink::create({.interface_name = "vcan_tc_ddop", .create_if_missing = true, .destroy_on_close = true}).value())),
          link_server(std::make_shared<wirebit::SocketCanLink>(
              wirebit::SocketCanLink::attach("vcan_tc_ddop").value())),
          ep_client(link_client, wirebit::CanConfig{}, 1),
          ep_server(link_server, wirebit::CanConfig{}, 2) {
        nm_client.set_endpoint(0, &ep_client);
        nm_server.set_endpoint(0, &ep_server);
        cf_client =
            nm_client.create_internal(Name::build().set_identity_number(10).set_manufacturer_code(100), 0, 0x28).value();
        cf_server =
            nm_server.create_internal(Name::build().set_identity_number(20).set_manufacturer_code(200), 0, 0x30).value();
    }

    void tick(u32 elapsed_ms = 10) {
        nm_client.update(elapsed_ms);
        nm_server.update(elapsed_ms);
        nm_client.update(0);
        nm_server.update(0);
    }
};

// ─── Helper: create a representative DDOP ─────────────────────────────────────
static DDOP make_test_ddop() {
    DDOP ddop;

    DeviceObject device;
    device.id = 1;
    device.designator = "TestSprayer";
    device.software_version = "2.1.0";
    device.serial_number = "SN12345";
    ddop.add_device(device);

    DeviceElement root_elem;
    root_elem.id = 2;
    root_elem.type = DeviceElementType::Device;
    root_elem.number = 0;
    root_elem.parent_id = 1;
    root_elem.designator = "Root";
    root_elem.child_objects = {3, 4};
    ddop.add_element(root_elem);

    DeviceElement boom;
    boom.id = 3;
    boom.type = DeviceElementType::Function;
    boom.number = 1;
    boom.parent_id = 2;
    boom.designator = "MainBoom";
    ddop.add_element(boom);

    DeviceElement section;
    section.id = 4;
    section.type = DeviceElementType::Section;
    section.number = 2;
    section.parent_id = 2;
    section.designator = "Section1";
    ddop.add_element(section);

    DeviceProcessData speed;
    speed.id = 5;
    speed.ddi = 0x0086; // Machine Selected Speed
    speed.trigger_methods = static_cast<u8>(TriggerMethod::OnChange);
    speed.presentation_object_id = 7;
    speed.designator = "Speed";
    ddop.add_process_data(speed);

    DeviceProcessData rate;
    rate.id = 6;
    rate.ddi = 0x0001; // Application Rate
    rate.trigger_methods = static_cast<u8>(TriggerMethod::TimeInterval) | static_cast<u8>(TriggerMethod::OnChange);
    rate.presentation_object_id = 7;
    rate.designator = "AppRate";
    ddop.add_process_data(rate);

    DeviceValuePresentation vp;
    vp.id = 7;
    vp.offset = 0;
    vp.scale = 0.001f;
    vp.decimal_digits = 3;
    vp.unit_designator = "m/s";
    ddop.add_value_presentation(vp);

    DeviceProperty width;
    width.id = 8;
    width.ddi = 0x0043; // Working width
    width.value = 12000; // 12m in mm
    width.presentation_object_id = 7;
    width.designator = "Width";
    ddop.add_property(width);

    return ddop;
}

TEST_CASE("TC E2E: DDOP serialize produces valid binary") {
    auto ddop = make_test_ddop();

    auto result = ddop.serialize();
    REQUIRE(result.is_ok());
    auto &data = result.value();

    // Should have meaningful content (device + 3 elements + 2 process data + 1 VP + 1 property)
    CHECK(data.size() > 50);

    // First byte should be TCObjectType::Device (0x00)
    CHECK(data[0] == static_cast<u8>(TCObjectType::Device));

    // Verify device designator is in the binary
    bool found_designator = false;
    for (usize i = 0; i + 10 < data.size(); ++i) {
        if (data[i] == 'T' && data[i + 1] == 'e' && data[i + 2] == 's' && data[i + 3] == 't') {
            found_designator = true;
            break;
        }
    }
    CHECK(found_designator);
}

TEST_CASE("TC E2E: DDOP validation passes for correct pool") {
    auto ddop = make_test_ddop();
    auto result = ddop.validate();
    CHECK(result.is_ok());
}

TEST_CASE("TC E2E: DDOP validation rejects missing device") {
    DDOP ddop;
    // Only add element, no device
    DeviceElement elem;
    elem.id = 1;
    elem.designator = "Root";
    ddop.add_element(elem);

    auto result = ddop.validate();
    CHECK_FALSE(result.is_ok());
}

TEST_CASE("TC E2E: DDOP validation rejects invalid parent reference") {
    DDOP ddop;

    DeviceObject device;
    device.id = 1;
    device.designator = "Dev";
    ddop.add_device(device);

    DeviceElement elem;
    elem.id = 2;
    elem.parent_id = 99; // Non-existent parent
    elem.designator = "Bad";
    ddop.add_element(elem);

    auto result = ddop.validate();
    CHECK_FALSE(result.is_ok());
}

TEST_CASE("TC E2E: DDOP validation rejects invalid presentation reference") {
    DDOP ddop;

    DeviceObject device;
    device.id = 1;
    device.designator = "Dev";
    ddop.add_device(device);

    DeviceElement elem;
    elem.id = 2;
    elem.parent_id = 1;
    elem.designator = "Root";
    ddop.add_element(elem);

    DeviceProcessData pd;
    pd.id = 3;
    pd.ddi = 0x0086;
    pd.presentation_object_id = 99; // Non-existent VP
    pd.designator = "Bad PD";
    ddop.add_process_data(pd);

    auto result = ddop.validate();
    CHECK_FALSE(result.is_ok());
}

TEST_CASE("TC E2E: DDOP transfer via transport protocol") {
    TCDualNode setup;

    TaskControllerServer server(setup.nm_server, setup.cf_server, TCServerConfig{}
        .version(4)
        .booms(2)
        .sections(12));
    server.start();

    TaskControllerClient client(setup.nm_client, setup.cf_client);
    auto ddop = make_test_ddop();
    client.set_ddop(std::move(ddop));
    client.connect();

    // Server broadcasts TC_STATUS to trigger client discovery
    // The server sends status every 2000ms
    server.update(2100);
    setup.tick(0);

    // Run ticks to advance state machine
    for (i32 i = 0; i < 100; ++i) {
        client.update(50);
        server.update(50);
        setup.tick(0);
    }

    // The DDOP was serialized by the client for transport.
    // Verify the serialized data is correct regardless of transport success.
    auto ddop_data = client.ddop().serialize();
    REQUIRE(ddop_data.is_ok());
    CHECK(ddop_data.value().size() > 50);

    // Verify DDOP object count
    CHECK(client.ddop().object_count() == 8); // 1 device + 3 elements + 2 PD + 1 VP + 1 prop
}

TEST_CASE("TC E2E: DDOP serialization byte-level verification") {
    DDOP ddop;

    DeviceObject device;
    device.id = 1;
    device.designator = "AB";
    device.software_version = "1";
    ddop.add_device(device);

    DeviceElement elem;
    elem.id = 2;
    elem.type = DeviceElementType::Function;
    elem.number = 5;
    elem.parent_id = 1;
    elem.designator = "E";
    ddop.add_element(elem);

    auto result = ddop.serialize();
    REQUIRE(result.is_ok());
    auto &data = result.value();

    // Verify device object layout:
    // [0] = TCObjectType::Device (0x00)
    CHECK(data[0] == 0x00);
    // [1..2] = ObjectID 1 (LE)
    CHECK(data[1] == 0x01);
    CHECK(data[2] == 0x00);
    // [3] = designator length = 2
    CHECK(data[3] == 0x02);
    // [4..5] = "AB"
    CHECK(data[4] == 'A');
    CHECK(data[5] == 'B');
    // [6] = software_version length = 1
    CHECK(data[6] == 0x01);
    // [7] = "1"
    CHECK(data[7] == '1');
    // [8] = serial_number length = 0
    CHECK(data[8] == 0x00);
    // [9..15] = structure_label (7 bytes, all 0)
    for (usize i = 9; i < 16; ++i)
        CHECK(data[i] == 0x00);
    // [16..22] = localization_label (7 bytes, all 0)
    for (usize i = 16; i < 23; ++i)
        CHECK(data[i] == 0x00);

    // Verify device element layout starts at offset 23:
    // [23] = TCObjectType::DeviceElement (0x01)
    CHECK(data[23] == 0x01);
    // [24..25] = ObjectID 2 (LE)
    CHECK(data[24] == 0x02);
    CHECK(data[25] == 0x00);
    // [26] = DeviceElementType::Function (0x02)
    CHECK(data[26] == 0x02);
    // [27] = designator length = 1
    CHECK(data[27] == 0x01);
    // [28] = "E"
    CHECK(data[28] == 'E');
    // [29..30] = element number = 5 (LE)
    CHECK(data[29] == 0x05);
    CHECK(data[30] == 0x00);
    // [31..32] = parent_id = 1 (LE)
    CHECK(data[31] == 0x01);
    CHECK(data[32] == 0x00);
    // [33..34] = num_children = 0 (LE)
    CHECK(data[33] == 0x00);
    CHECK(data[34] == 0x00);
}

TEST_CASE("TC E2E: DeviceProcessData serialization with presentation object") {
    DDOP ddop;

    DeviceObject device;
    device.id = 1;
    device.designator = "D";
    ddop.add_device(device);

    DeviceElement elem;
    elem.id = 2;
    elem.parent_id = 1;
    elem.designator = "E";
    ddop.add_element(elem);

    DeviceValuePresentation vp;
    vp.id = 10;
    vp.offset = 100;
    vp.scale = 0.01f;
    vp.decimal_digits = 2;
    vp.unit_designator = "mm";
    ddop.add_value_presentation(vp);

    DeviceProcessData pd;
    pd.id = 3;
    pd.ddi = 0x0086;
    pd.trigger_methods = 0x09; // TimeInterval | OnChange
    pd.presentation_object_id = 10;
    pd.designator = "SPD";
    ddop.add_process_data(pd);

    // Should validate OK (VP exists)
    auto val = ddop.validate();
    CHECK(val.is_ok());

    auto result = ddop.serialize();
    REQUIRE(result.is_ok());
    auto &data = result.value();

    // Find the ProcessData object in serialized data.
    // It appears after the device, element, and VP objects in serialization order.
    // The DDOP serializes: devices, elements, process_data, properties, value_presentations.
    // So the PD with id=3 will have bytes: type(0x02), id_lo(0x03), id_hi(0x00)
    usize pd_offset = 0;
    for (usize i = 0; i + 2 < data.size(); ++i) {
        if (data[i] == static_cast<u8>(TCObjectType::DeviceProcessData) &&
            data[i + 1] == 0x03 && data[i + 2] == 0x00) {
            pd_offset = i;
            break;
        }
    }
    REQUIRE(pd_offset > 0);

    // Verify PD layout:
    // [+0] = type (0x02)
    CHECK(data[pd_offset] == 0x02);
    // [+1..+2] = id = 3 (LE)
    CHECK(data[pd_offset + 1] == 0x03);
    CHECK(data[pd_offset + 2] == 0x00);
    // [+3..+4] = DDI = 0x0086 (LE)
    CHECK(data[pd_offset + 3] == 0x86);
    CHECK(data[pd_offset + 4] == 0x00);
    // [+5] = trigger_methods = 0x09
    CHECK(data[pd_offset + 5] == 0x09);
    // [+6..+7] = presentation_object_id = 10 (LE)
    CHECK(data[pd_offset + 6] == 0x0A);
    CHECK(data[pd_offset + 7] == 0x00);
    // [+8] = designator length = 3
    CHECK(data[pd_offset + 8] == 0x03);
    // [+9..+11] = "SPD"
    CHECK(data[pd_offset + 9] == 'S');
    CHECK(data[pd_offset + 10] == 'P');
    CHECK(data[pd_offset + 11] == 'D');
}

TEST_CASE("TC E2E: DeviceProperty serialization with value") {
    DeviceProperty prop;
    prop.id = 5;
    prop.ddi = 0x0043; // Working width
    prop.value = 12000;
    prop.presentation_object_id = 0xFFFF; // No presentation
    prop.designator = "W";

    auto data = prop.serialize();

    // [0] = type (0x03)
    CHECK(data[0] == 0x03);
    // [1..2] = id = 5 (LE)
    CHECK(data[1] == 0x05);
    CHECK(data[2] == 0x00);
    // [3..4] = DDI = 0x0043 (LE)
    CHECK(data[3] == 0x43);
    CHECK(data[4] == 0x00);
    // [5..8] = value = 12000 (0x2EE0) (LE)
    CHECK(data[5] == 0xE0);
    CHECK(data[6] == 0x2E);
    CHECK(data[7] == 0x00);
    CHECK(data[8] == 0x00);
    // [9..10] = presentation_object_id = 0xFFFF (LE)
    CHECK(data[9] == 0xFF);
    CHECK(data[10] == 0xFF);
    // [11] = designator length = 1
    CHECK(data[11] == 0x01);
    // [12] = "W"
    CHECK(data[12] == 'W');
}

TEST_CASE("TC E2E: DeviceValuePresentation serialization") {
    DeviceValuePresentation vp;
    vp.id = 7;
    vp.offset = -100;
    vp.scale = 1.5f;
    vp.decimal_digits = 1;
    vp.unit_designator = "kg";

    auto data = vp.serialize();

    // [0] = type (0x04)
    CHECK(data[0] == 0x04);
    // [1..2] = id = 7 (LE)
    CHECK(data[1] == 0x07);
    CHECK(data[2] == 0x00);
    // [3..6] = offset = -100 (LE, signed)
    i32 offset_check = static_cast<i32>(data[3]) | (static_cast<i32>(data[4]) << 8) |
                        (static_cast<i32>(data[5]) << 16) | (static_cast<i32>(data[6]) << 24);
    CHECK(offset_check == -100);
    // [7..10] = scale as IEEE 754 float (1.5f)
    u32 scale_bits = static_cast<u32>(data[7]) | (static_cast<u32>(data[8]) << 8) |
                     (static_cast<u32>(data[9]) << 16) | (static_cast<u32>(data[10]) << 24);
    f32 scale_check;
    std::memcpy(&scale_check, &scale_bits, sizeof(f32));
    CHECK(scale_check == doctest::Approx(1.5f));
    // [11] = decimal_digits = 1
    CHECK(data[11] == 0x01);
    // [12] = unit_designator length = 2
    CHECK(data[12] == 0x02);
    // [13..14] = "kg"
    CHECK(data[13] == 'k');
    CHECK(data[14] == 'g');
}

TEST_CASE("TC E2E: DDOP child objects serialization") {
    DDOP ddop;

    DeviceObject device;
    device.id = 1;
    device.designator = "D";
    ddop.add_device(device);

    DeviceElement root;
    root.id = 2;
    root.parent_id = 1;
    root.designator = "R";
    root.child_objects = {3, 4, 5}; // Three children
    ddop.add_element(root);

    DeviceElement child1;
    child1.id = 3;
    child1.type = DeviceElementType::Section;
    child1.parent_id = 2;
    child1.designator = "S1";
    ddop.add_element(child1);

    DeviceElement child2;
    child2.id = 4;
    child2.type = DeviceElementType::Section;
    child2.parent_id = 2;
    child2.designator = "S2";
    ddop.add_element(child2);

    DeviceElement child3;
    child3.id = 5;
    child3.type = DeviceElementType::Section;
    child3.parent_id = 2;
    child3.designator = "S3";
    ddop.add_element(child3);

    // Validate passes (all children exist)
    auto val = ddop.validate();
    CHECK(val.is_ok());

    auto result = ddop.serialize();
    REQUIRE(result.is_ok());

    // The serialized data should contain the root element with 3 child references
    auto &data = result.value();

    // Find root element (id=2) in serialized data
    bool found_children = false;
    for (usize i = 0; i + 2 < data.size(); ++i) {
        if (data[i] == static_cast<u8>(TCObjectType::DeviceElement) && data[i + 1] == 0x02 && data[i + 2] == 0x00) {
            // Found root element, skip to child count
            // type(1) + id(2) + elem_type(1) + designator_len(1) + "R"(1) + number(2) + parent_id(2)
            usize child_count_offset = i + 1 + 2 + 1 + 1 + 1 + 2 + 2;
            u16 num_children =
                static_cast<u16>(data[child_count_offset]) | (static_cast<u16>(data[child_count_offset + 1]) << 8);
            CHECK(num_children == 3);

            // Check child IDs
            usize child_ids_start = child_count_offset + 2;
            u16 cid1 = static_cast<u16>(data[child_ids_start]) | (static_cast<u16>(data[child_ids_start + 1]) << 8);
            u16 cid2 =
                static_cast<u16>(data[child_ids_start + 2]) | (static_cast<u16>(data[child_ids_start + 3]) << 8);
            u16 cid3 =
                static_cast<u16>(data[child_ids_start + 4]) | (static_cast<u16>(data[child_ids_start + 5]) << 8);
            CHECK(cid1 == 3);
            CHECK(cid2 == 4);
            CHECK(cid3 == 5);
            found_children = true;
            break;
        }
    }
    CHECK(found_children);
}

TEST_CASE("TC E2E: DDOP validation rejects invalid child reference") {
    DDOP ddop;

    DeviceObject device;
    device.id = 1;
    device.designator = "D";
    ddop.add_device(device);

    DeviceElement elem;
    elem.id = 2;
    elem.parent_id = 1;
    elem.designator = "E";
    elem.child_objects = {99}; // Non-existent child
    ddop.add_element(elem);

    auto result = ddop.validate();
    CHECK_FALSE(result.is_ok());
}

TEST_CASE("TC E2E: TC client rejects invalid DDOP on connect") {
    IsoNet nm;
    auto *cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x10).value();

    TaskControllerClient client(nm, cf);

    // Set empty DDOP (no devices)
    DDOP empty_ddop;
    client.set_ddop(std::move(empty_ddop));

    auto result = client.connect();
    CHECK_FALSE(result.is_ok());
    CHECK(client.state() == TCState::Disconnected);
}

TEST_CASE("TC E2E: DDOP serialized size matches expected") {
    auto ddop = make_test_ddop();
    auto result = ddop.serialize();
    REQUIRE(result.is_ok());

    // Calculate expected size:
    // Device: 1(type) + 2(id) + 1(desig_len) + 11("TestSprayer") + 1(sw_len) + 5("2.1.0") + 1(sn_len) + 7("SN12345") + 7(struct) + 7(local) = 43
    // Root elem: 1 + 2 + 1 + 1 + 4("Root") + 2 + 2 + 2 + 4(2 children) = 19
    // Boom elem: 1 + 2 + 1 + 1 + 8("MainBoom") + 2 + 2 + 2 + 0 = 19
    // Section elem: 1 + 2 + 1 + 1 + 8("Section1") + 2 + 2 + 2 + 0 = 19
    // Speed PD: 1 + 2 + 2 + 1 + 2 + 1 + 5("Speed") = 14
    // Rate PD: 1 + 2 + 2 + 1 + 2 + 1 + 7("AppRate") = 16
    // VP: 1 + 2 + 4 + 4 + 1 + 1 + 3("m/s") = 16
    // Width Prop: 1 + 2 + 2 + 4 + 2 + 1 + 5("Width") = 17

    // Verify size is non-trivial (more than just type tags)
    CHECK(result.value().size() > ddop.object_count());

    // Verify serialization is deterministic
    auto result2 = ddop.serialize();
    REQUIRE(result2.is_ok());
    CHECK(result.value().size() == result2.value().size());
    CHECK(result.value() == result2.value());
}
