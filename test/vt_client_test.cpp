#include <doctest/doctest.h>
#include <agrobus/isobus/vt/client.hpp>

using namespace agrobus::isobus;
using namespace agrobus::isobus::vt;

TEST_CASE("VTClient state machine") {
    IsoNet nm;
    Name name;
    auto cf_result = nm.create_internal(name, 0, 0x28);
    auto* cf = cf_result.value();

    VTClient vt(nm, cf);

    SUBCASE("initial state") {
        CHECK(vt.state() == VTState::Disconnected);
    }

    SUBCASE("connect without pool fails") {
        auto result = vt.connect();
        CHECK(result.is_err());
    }

    SUBCASE("connect with pool") {
        ObjectPool pool;
        VTObject obj;
        obj.id = 1;
        obj.type = ObjectType::WorkingSet;
        pool.add(std::move(obj));

        vt.set_object_pool(std::move(pool));
        auto result = vt.connect();
        CHECK(result.is_ok());
        CHECK(vt.state() == VTState::WaitForVTStatus);
    }

    SUBCASE("timeout without VT") {
        ObjectPool pool;
        VTObject obj;
        obj.id = 1;
        pool.add(std::move(obj));

        vt.set_object_pool(std::move(pool));
        vt.connect();
        vt.update(6001);
        CHECK(vt.state() == VTState::Disconnected);
    }
}

TEST_CASE("VTClient commands require connected state") {
    IsoNet nm;
    Name name;
    auto cf_result = nm.create_internal(name, 0, 0x28);
    auto* cf = cf_result.value();

    VTClient vt(nm, cf);

    SUBCASE("hide_show fails when disconnected") {
        auto result = vt.hide_show(1, true);
        CHECK(result.is_err());
    }

    SUBCASE("change_numeric_value fails when disconnected") {
        auto result = vt.change_numeric_value(1, 42);
        CHECK(result.is_err());
    }

    SUBCASE("change_string_value fails when disconnected") {
        auto result = vt.change_string_value(1, "test");
        CHECK(result.is_err());
    }

    SUBCASE("change_active_mask fails when disconnected") {
        auto result = vt.change_active_mask(1, 2);
        CHECK(result.is_err());
    }
}
