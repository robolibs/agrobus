#include <doctest/doctest.h>
#include <agrobus/net/control_function.hpp>
#include <agrobus/net/partner_cf.hpp>
#include <agrobus/net/constants.hpp>

using namespace agrobus::net;

TEST_CASE("ControlFunction") {
    SUBCASE("default state") {
        ControlFunction cf;
        CHECK(cf.address == NULL_ADDRESS);
        CHECK(cf.state == CFState::Offline);
        CHECK(!cf.address_valid());
        CHECK(!cf.is_online());
    }

    SUBCASE("valid address") {
        ControlFunction cf;
        cf.address = 0x28;
        CHECK(cf.address_valid());
    }

    SUBCASE("NULL_ADDRESS is invalid") {
        ControlFunction cf;
        cf.address = NULL_ADDRESS;
        CHECK(!cf.address_valid());
    }
}

TEST_CASE("NameFilter matching") {
    Name name;
    name.set_function_code(100);
    name.set_industry_group(2);
    name.set_manufacturer_code(555);

    SUBCASE("single filter match") {
        NameFilter f{NameFilterField::FunctionCode, 100};
        CHECK(f.matches(name));
    }

    SUBCASE("single filter no match") {
        NameFilter f{NameFilterField::FunctionCode, 99};
        CHECK(!f.matches(name));
    }

    SUBCASE("manufacturer code filter") {
        NameFilter f{NameFilterField::ManufacturerCode, 555};
        CHECK(f.matches(name));
    }

    SUBCASE("industry group filter") {
        NameFilter f{NameFilterField::IndustryGroup, 2};
        CHECK(f.matches(name));
    }
}

TEST_CASE("PartnerCF matching") {
    dp::Vector<NameFilter> filters = {
        {NameFilterField::FunctionCode, 100},
        {NameFilterField::IndustryGroup, 2}
    };
    PartnerCF partner(0, std::move(filters));

    SUBCASE("matches all filters") {
        Name name;
        name.set_function_code(100);
        name.set_industry_group(2);
        CHECK(partner.matches_name(name));
    }

    SUBCASE("fails if any filter doesn't match") {
        Name name;
        name.set_function_code(100);
        name.set_industry_group(3);
        CHECK(!partner.matches_name(name));
    }
}
