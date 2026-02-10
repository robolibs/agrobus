#include <doctest/doctest.h>
#include <agrobus/isobus/tc/client.hpp>

using namespace agrobus::isobus;
using namespace agrobus::isobus::tc;

TEST_CASE("TaskControllerClient state machine") {
    IsoNet nm;
    Name name;
    name.set_function_code(100);
    auto cf_result = nm.create_internal(name, 0, 0x28);
    auto* cf = cf_result.value();

    TaskControllerClient tc(nm, cf);

    SUBCASE("initial state") {
        CHECK(tc.state() == TCState::Disconnected);
    }

    SUBCASE("connect without DDOP fails") {
        auto result = tc.connect();
        CHECK(result.is_err());
    }

    SUBCASE("connect with valid DDOP") {
        DDOP ddop;
        DeviceObject dev;
        dev.id = 1;
        dev.designator = "TestDevice";
        ddop.add_device(dev);

        DeviceElement elem;
        elem.id = 2;
        elem.parent_id = 1;
        elem.type = DeviceElementType::Device;
        ddop.add_element(elem);

        tc.set_ddop(std::move(ddop));
        auto result = tc.connect();
        CHECK(result.is_ok());
        CHECK(tc.state() == TCState::WaitForServerStatus);
    }

    SUBCASE("timeout without server") {
        DDOP ddop;
        DeviceObject dev;
        dev.id = 1;
        dev.designator = "Dev";
        ddop.add_device(dev);
        DeviceElement elem;
        elem.id = 2;
        elem.parent_id = 1;
        elem.type = DeviceElementType::Device;
        ddop.add_element(elem);

        tc.set_ddop(std::move(ddop));
        tc.connect();
        tc.update(6001); // Timeout
        CHECK(tc.state() == TCState::Disconnected);
    }
}

TEST_CASE("TaskControllerClient callbacks") {
    IsoNet nm;
    Name name;
    auto cf_result = nm.create_internal(name, 0, 0x28);
    auto* cf = cf_result.value();

    TaskControllerClient tc(nm, cf);

    SUBCASE("value callback") {
        bool called = false;
        tc.on_value_request([&](ElementNumber, DDI) -> Result<i32> {
            called = true;
            return Result<i32>::ok(42);
        });
        // Callback is registered
        CHECK(!called);
    }

    SUBCASE("command callback") {
        bool called = false;
        tc.on_value_command([&](ElementNumber, DDI, i32) -> Result<void> {
            called = true;
            return {};
        });
        CHECK(!called);
    }
}
