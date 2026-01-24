#include <agrobus/net/network_manager.hpp>
#include <agrobus/isobus/vt/client.hpp>
#include <echo/echo.hpp>

using namespace agrobus::net;
using namespace agrobus::isobus;
using namespace agrobus::isobus::vt;

int main() {
    echo::info("=== VT Client Demo ===");

    IsoNet nm;
    Name name = Name::build()
        .set_identity_number(1)
        .set_manufacturer_code(42)
        .set_function_code(25);

    auto cf_result = nm.create_internal(name, 0, 0x28);
    auto* cf = cf_result.value();

    // Build object pool
    ObjectPool pool;

    pool.add(VTObject{}.set_id(0).set_type(ObjectType::WorkingSet)
        .set_body({static_cast<u8>(240 & 0xFF), static_cast<u8>((240 >> 8) & 0xFF),
                   static_cast<u8>(240 & 0xFF), static_cast<u8>((240 >> 8) & 0xFF)}));

    pool.add(VTObject{}.set_id(1).set_type(ObjectType::DataMask)
        .set_body({static_cast<u8>(240 & 0xFF), static_cast<u8>((240 >> 8) & 0xFF),
                   static_cast<u8>(240 & 0xFF), static_cast<u8>((240 >> 8) & 0xFF)}));

    pool.add(VTObject{}.set_id(2).set_type(ObjectType::DataMask)
        .set_body({static_cast<u8>(240 & 0xFF), static_cast<u8>((240 >> 8) & 0xFF),
                   static_cast<u8>(240 & 0xFF), static_cast<u8>((240 >> 8) & 0xFF)}));

    pool.add(VTObject{}.set_id(100).set_type(ObjectType::Button)
        .set_body({static_cast<u8>(80 & 0xFF), static_cast<u8>((80 >> 8) & 0xFF),
                   static_cast<u8>(40 & 0xFF), static_cast<u8>((40 >> 8) & 0xFF)}));

    pool.add(VTObject{}.set_id(200).set_type(ObjectType::OutputNumber)
        .set_body({static_cast<u8>(100 & 0xFF), static_cast<u8>((100 >> 8) & 0xFF),
                   static_cast<u8>(30 & 0xFF), static_cast<u8>((30 >> 8) & 0xFF)}));

    echo::info("Object pool: ", pool.size(), " objects");

    // Create VT client
    VTClient vt(nm, cf);
    vt.set_object_pool(std::move(pool));

    // Set up working set
    WorkingSet working_set;
    working_set.add_data_mask(1);
    working_set.add_data_mask(2);
    working_set.set_active_mask(1);
    vt.set_working_set(std::move(working_set));

    // Event handlers
    vt.on_button.subscribe([](ObjectID id, ActivationCode code) {
        echo::info("Button ", id, " activation: ", static_cast<int>(code));
    });

    vt.on_soft_key.subscribe([](ObjectID id, ActivationCode code) {
        echo::info("Soft key ", id, " activation: ", static_cast<int>(code));
    });

    vt.on_numeric_value_change.subscribe([](ObjectID id, u32 value) {
        echo::info("Numeric change: id=", id, " value=", value);
    });

    vt.on_state_change.subscribe([](VTState state) {
        echo::info("VT state: ", static_cast<int>(state));
    });

    // Connect
    auto result = vt.connect();
    echo::info("VT connect: ", result.is_ok());
    echo::info("VT state: ", static_cast<int>(vt.state()));

    return 0;
}
