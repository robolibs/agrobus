#include <doctest/doctest.h>
#include <agrobus.hpp>

using namespace agrobus::net;
using namespace agrobus::j1939;
using namespace agrobus::isobus;
using namespace agrobus::nmea;
using namespace agrobus::isobus::vt;
using namespace agrobus::isobus::tc;
using namespace agrobus::isobus::sc;
using namespace agrobus::isobus::implement;
using namespace agrobus::isobus::fs;

TEST_CASE("AuxOInterface - function management") {
    IsoNet nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x10).value();

    AuxOInterface aux(nm, cf);

    SUBCASE("Add and retrieve function") {
        aux.add_function(1, AuxFunctionType::Type0);
        auto func = aux.get_function(1);
        REQUIRE(func.has_value());
        CHECK(func->function_number == 1);
        CHECK(func->type == AuxFunctionType::Type0);
        CHECK(func->state == AuxFunctionState::Off);
        CHECK(func->setpoint == 0);
    }

    SUBCASE("Set function updates state") {
        aux.add_function(2, AuxFunctionType::Type1);
        aux.set_function(2, AuxFunctionType::Type1, 5000);
        auto func = aux.get_function(2);
        REQUIRE(func.has_value());
        CHECK(func->setpoint == 5000);
        CHECK(func->state == AuxFunctionState::Variable);
    }

    SUBCASE("Non-existent function returns nullopt") {
        CHECK_FALSE(aux.get_function(99).has_value());
    }

    SUBCASE("Event fires on change") {
        bool changed = false;
        aux.on_function_changed.subscribe([&](const AuxOFunction& f) {
            changed = true;
            CHECK(f.function_number == 3);
        });
        aux.set_function(3, AuxFunctionType::Type0, 1);
        CHECK(changed);
    }
}

TEST_CASE("AuxNInterface - function management") {
    IsoNet nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x10).value();

    AuxNInterface aux(nm, cf);

    SUBCASE("Add and retrieve function") {
        aux.add_function(1, AuxFunctionType::Type2);
        auto func = aux.get_function(1);
        REQUIRE(func.has_value());
        CHECK(func->function_number == 1);
        CHECK(func->type == AuxFunctionType::Type2);
    }

    SUBCASE("Type0 function state is On/Off") {
        aux.set_function(1, AuxFunctionType::Type0, 100);
        auto func = aux.get_function(1);
        REQUIRE(func.has_value());
        CHECK(func->state == AuxFunctionState::On);

        aux.set_function(1, AuxFunctionType::Type0, 0);
        func = aux.get_function(1);
        CHECK(func->state == AuxFunctionState::Off);
    }

    SUBCASE("Type1/2 function state is Variable") {
        aux.set_function(5, AuxFunctionType::Type1, 32000);
        auto func = aux.get_function(5);
        REQUIRE(func.has_value());
        CHECK(func->state == AuxFunctionState::Variable);
    }
}
