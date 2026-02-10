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
    echo::info("=== VT Version 3 Demo ===");

    auto link_result = wirebit::SocketCanLink::create({
        .interface_name = "vcan_vtv3",
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

    VTClient vt(nm, cf);

    // Set VT version preference
    vt.set_vt_version_preference(VTVersion::Version3);
    echo::info("VT Version preference: ", vt.get_vt_version());

    // Create object pool with VT v3 compatible objects
    ObjectPool pool;

    pool.add(VTObject{}.set_id(0).set_type(ObjectType::WorkingSet)
        .set_body({static_cast<u8>(200 & 0xFF), static_cast<u8>((200 >> 8) & 0xFF),
                   static_cast<u8>(200 & 0xFF), static_cast<u8>((200 >> 8) & 0xFF)}));

    pool.add(VTObject{}.set_id(1).set_type(ObjectType::DataMask)
        .set_body({static_cast<u8>(200 & 0xFF), static_cast<u8>((200 >> 8) & 0xFF),
                   static_cast<u8>(200 & 0xFF), static_cast<u8>((200 >> 8) & 0xFF)}));

    pool.add(VTObject{}.set_id(100).set_type(ObjectType::OutputString)
        .set_body({static_cast<u8>(100 & 0xFF), static_cast<u8>((100 >> 8) & 0xFF),
                   static_cast<u8>(20 & 0xFF), static_cast<u8>((20 >> 8) & 0xFF)}));

    pool.add(VTObject{}.set_id(101).set_type(ObjectType::OutputNumber)
        .set_body({static_cast<u8>(60 & 0xFF), static_cast<u8>((60 >> 8) & 0xFF),
                   static_cast<u8>(20 & 0xFF), static_cast<u8>((20 >> 8) & 0xFF)}));

    pool.add(VTObject{}.set_id(200).set_type(ObjectType::NumberVariable));

    pool.add(VTObject{}.set_id(201).set_type(ObjectType::StringVariable));

    pool.add(VTObject{}.set_id(300).set_type(ObjectType::Button)
        .set_body({static_cast<u8>(40 & 0xFF), static_cast<u8>((40 >> 8) & 0xFF),
                   static_cast<u8>(40 & 0xFF), static_cast<u8>((40 >> 8) & 0xFF)}));

    pool.add(VTObject{}.set_id(400).set_type(ObjectType::SoftKeyMask)
        .set_body({static_cast<u8>(60 & 0xFF), static_cast<u8>((60 >> 8) & 0xFF),
                   static_cast<u8>(200 & 0xFF), static_cast<u8>((200 >> 8) & 0xFF)}));

    echo::info("Object pool: ", pool.size(), " objects");

    vt.set_object_pool(std::move(pool));

    // VT v3 event handlers
    vt.on_button.subscribe([](ObjectID id, ActivationCode code) {
        echo::info("Button ", id, " activation: ", static_cast<u8>(code));
    });

    vt.on_soft_key.subscribe([](ObjectID id, ActivationCode code) {
        echo::info("Soft key ", id, " activation: ", static_cast<u8>(code));
    });

    vt.on_numeric_value_change.subscribe([](ObjectID id, u32 value) {
        echo::info("Numeric ", id, " = ", value);
    });

    echo::info("VT client configured for Version 3 compatibility");
    echo::info("=== VT Version 3 Demo Complete ===");
    return 0;
}
