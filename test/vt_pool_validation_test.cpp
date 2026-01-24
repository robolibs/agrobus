#include <doctest/doctest.h>
#include <agrobus/isobus/vt/objects.hpp>
#include <agrobus/isobus/vt/server.hpp>
#include <agrobus/isobus/vt/client.hpp>

using namespace agrobus::isobus;
using namespace agrobus::isobus::vt;

TEST_CASE("ObjectPool validate") {
    SUBCASE("pool with no working set fails validation") {
        ObjectPool pool;
        pool.add(VTObject{}.set_type(ObjectType::DataMask).set_id(1));
        pool.add(VTObject{}.set_type(ObjectType::Button).set_id(2));

        auto result = pool.validate();
        CHECK(!result.is_ok());
        CHECK(result.error().message.find("must contain exactly one Working Set") != dp::String::npos);
    }

    SUBCASE("pool with 1 working set and a mask child passes") {
        ObjectPool pool;
        pool.add(VTObject{}.set_type(ObjectType::WorkingSet).set_id(0).set_children({1}));
        pool.add(VTObject{}.set_type(ObjectType::DataMask).set_id(1));

        auto result = pool.validate();
        CHECK(result.is_ok());
    }

    SUBCASE("pool with >1 working set fails") {
        ObjectPool pool;
        pool.add(VTObject{}.set_type(ObjectType::WorkingSet).set_id(0).set_children({2}));
        pool.add(VTObject{}.set_type(ObjectType::WorkingSet).set_id(1).set_children({2}));
        pool.add(VTObject{}.set_type(ObjectType::DataMask).set_id(2));

        auto result = pool.validate();
        CHECK(!result.is_ok());
        CHECK(result.error().message.find("must contain exactly one Working Set") != dp::String::npos);
    }

    SUBCASE("working set without mask child fails") {
        ObjectPool pool;
        pool.add(VTObject{}.set_type(ObjectType::WorkingSet).set_id(0).set_children({1}));
        pool.add(VTObject{}.set_type(ObjectType::Button).set_id(1));

        auto result = pool.validate();
        CHECK(!result.is_ok());
        CHECK(result.error().message.find("at least one Data Mask or Alarm Mask") != dp::String::npos);
    }

    SUBCASE("working set with alarm mask child passes") {
        ObjectPool pool;
        pool.add(VTObject{}.set_type(ObjectType::WorkingSet).set_id(0).set_children({1}));
        pool.add(VTObject{}.set_type(ObjectType::AlarmMask).set_id(1));

        auto result = pool.validate();
        CHECK(result.is_ok());
    }

    SUBCASE("orphan child reference fails") {
        ObjectPool pool;
        pool.add(VTObject{}.set_type(ObjectType::WorkingSet).set_id(0).set_children({1, 99}));
        pool.add(VTObject{}.set_type(ObjectType::DataMask).set_id(1));

        auto result = pool.validate();
        CHECK(!result.is_ok());
        CHECK(result.error().message.find("non-existent child") != dp::String::npos);
    }

    SUBCASE("empty pool fails validation") {
        ObjectPool pool;
        auto result = pool.validate();
        CHECK(!result.is_ok());
    }

    SUBCASE("working set with no children fails (no mask)") {
        ObjectPool pool;
        pool.add(VTObject{}.set_type(ObjectType::WorkingSet).set_id(0));

        auto result = pool.validate();
        CHECK(!result.is_ok());
        CHECK(result.error().message.find("at least one Data Mask or Alarm Mask") != dp::String::npos);
    }

    SUBCASE("valid pool with multiple objects") {
        ObjectPool pool;
        pool.add(VTObject{}.set_type(ObjectType::WorkingSet).set_id(0).set_children({1, 2}));
        pool.add(VTObject{}.set_type(ObjectType::DataMask).set_id(1).set_children({3}));
        pool.add(VTObject{}.set_type(ObjectType::AlarmMask).set_id(2));
        pool.add(VTObject{}.set_type(ObjectType::Button).set_id(3));

        auto result = pool.validate();
        CHECK(result.is_ok());
    }
}

TEST_CASE("VTServer active WS management") {
    IsoNet nm;
    Name name;
    auto cf_result = nm.create_internal(name, 0, 0x26);
    auto* cf = cf_result.value();

    VTServer server(nm, cf);

    SUBCASE("initial active WS is NULL_ADDRESS") {
        CHECK(server.active_working_set() == NULL_ADDRESS);
    }

    SUBCASE("set_active_working_set changes the address") {
        server.set_active_working_set(0x28);
        CHECK(server.active_working_set() == 0x28);
    }

    SUBCASE("set_active_working_set same address does not emit event") {
        server.set_active_working_set(0x28);

        bool event_fired = false;
        server.on_active_ws_changed += [&](Address, Address) {
            event_fired = true;
        };

        server.set_active_working_set(0x28); // same address
        CHECK(!event_fired);
    }

    SUBCASE("set_active_working_set emits event with old and new") {
        server.set_active_working_set(0x10);

        Address captured_old = 0;
        Address captured_new = 0;
        server.on_active_ws_changed += [&](Address old_addr, Address new_addr) {
            captured_old = old_addr;
            captured_new = new_addr;
        };

        server.set_active_working_set(0x20);
        CHECK(captured_old == 0x10);
        CHECK(captured_new == 0x20);
    }

    SUBCASE("set_active_working_set from NULL to address") {
        Address captured_old = 0xFF;
        Address captured_new = 0;
        server.on_active_ws_changed += [&](Address old_addr, Address new_addr) {
            captured_old = old_addr;
            captured_new = new_addr;
        };

        server.set_active_working_set(0x30);
        CHECK(captured_old == NULL_ADDRESS);
        CHECK(captured_new == 0x30);
        CHECK(server.active_working_set() == 0x30);
    }
}

TEST_CASE("VTClient active WS tracking") {
    IsoNet nm;
    Name name;
    auto cf_result = nm.create_internal(name, 0, 0x28);
    auto* cf = cf_result.value();
    cf->set_address(0x28);

    VTClient client(nm, cf);

    SUBCASE("initial is_active_ws is false") {
        CHECK(client.is_active_ws() == false);
    }
}
