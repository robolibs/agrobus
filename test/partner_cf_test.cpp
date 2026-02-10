#include <doctest/doctest.h>
#include <agrobus/net/partner_cf.hpp>

using namespace agrobus::net;

// ═════════════════════════════════════════════════════════════════════════════
// NameFilter Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("NameFilter matching") {
    Name test_name = Name::build()
                         .set_identity_number(12345)
                         .set_manufacturer_code(1234)
                         .set_ecu_instance(2)
                         .set_function_instance(3)
                         .set_function_code(25)
                         .set_device_class(7)
                         .set_device_class_instance(1)
                         .set_industry_group(2);

    SUBCASE("identity number filter") {
        NameFilter filter{NameFilterField::IdentityNumber, 12345};
        CHECK(filter.matches(test_name));

        NameFilter wrong{NameFilterField::IdentityNumber, 99999};
        CHECK_FALSE(wrong.matches(test_name));
    }

    SUBCASE("manufacturer code filter") {
        NameFilter filter{NameFilterField::ManufacturerCode, 1234};
        CHECK(filter.matches(test_name));

        NameFilter wrong{NameFilterField::ManufacturerCode, 5678};
        CHECK_FALSE(wrong.matches(test_name));
    }

    SUBCASE("ECU instance filter") {
        NameFilter filter{NameFilterField::ECUInstance, 2};
        CHECK(filter.matches(test_name));

        NameFilter wrong{NameFilterField::ECUInstance, 5};
        CHECK_FALSE(wrong.matches(test_name));
    }

    SUBCASE("function instance filter") {
        NameFilter filter{NameFilterField::FunctionInstance, 3};
        CHECK(filter.matches(test_name));

        NameFilter wrong{NameFilterField::FunctionInstance, 7};
        CHECK_FALSE(wrong.matches(test_name));
    }

    SUBCASE("function code filter") {
        NameFilter filter{NameFilterField::FunctionCode, 25};
        CHECK(filter.matches(test_name));

        NameFilter wrong{NameFilterField::FunctionCode, 50};
        CHECK_FALSE(wrong.matches(test_name));
    }

    SUBCASE("device class filter") {
        NameFilter filter{NameFilterField::DeviceClass, 7};
        CHECK(filter.matches(test_name));

        NameFilter wrong{NameFilterField::DeviceClass, 9};
        CHECK_FALSE(wrong.matches(test_name));
    }

    SUBCASE("device class instance filter") {
        NameFilter filter{NameFilterField::DeviceClassInstance, 1};
        CHECK(filter.matches(test_name));

        NameFilter wrong{NameFilterField::DeviceClassInstance, 4};
        CHECK_FALSE(wrong.matches(test_name));
    }

    SUBCASE("industry group filter") {
        NameFilter filter{NameFilterField::IndustryGroup, 2};
        CHECK(filter.matches(test_name));

        NameFilter wrong{NameFilterField::IndustryGroup, 5};
        CHECK_FALSE(wrong.matches(test_name));
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// PartnerCF Basic Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("PartnerCF construction") {
    SUBCASE("empty filters") {
        dp::Vector<NameFilter> filters;
        PartnerCF partner(0, filters);

        CHECK(partner.port() == 0);
        CHECK(partner.filters().size() == 0);
        CHECK(partner.cf().type == CFType::Partnered);
    }

    SUBCASE("single filter") {
        dp::Vector<NameFilter> filters;
        filters.push_back({NameFilterField::ManufacturerCode, 1234});

        PartnerCF partner(1, filters);

        CHECK(partner.port() == 1);
        CHECK(partner.filters().size() == 1);
    }

    SUBCASE("multiple filters") {
        dp::Vector<NameFilter> filters;
        filters.push_back({NameFilterField::ManufacturerCode, 1234});
        filters.push_back({NameFilterField::FunctionCode, 25});
        filters.push_back({NameFilterField::IndustryGroup, 2});

        PartnerCF partner(0, filters);

        CHECK(partner.filters().size() == 3);
    }
}

TEST_CASE("PartnerCF getters and setters") {
    dp::Vector<NameFilter> filters;
    PartnerCF partner(0, filters);

    SUBCASE("address") {
        partner.set_address(0x42);
        CHECK(partner.address() == 0x42);
    }

    SUBCASE("name") {
        Name name = Name::build().set_identity_number(999).set_manufacturer_code(111);
        partner.set_name(name);

        CHECK(partner.name().identity_number() == 999);
        CHECK(partner.name().manufacturer_code() == 111);
    }

    SUBCASE("state") {
        partner.set_state(CFState::Online);
        CHECK(partner.cf().state == CFState::Online);

        partner.set_state(CFState::Offline);
        CHECK(partner.cf().state == CFState::Offline);
    }

    SUBCASE("port is immutable") {
        // Port is set in constructor and cannot be changed
        CHECK(partner.port() == 0);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// PartnerCF Name Matching Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("PartnerCF name matching") {
    SUBCASE("empty filters match all") {
        dp::Vector<NameFilter> filters;
        PartnerCF partner(0, filters);

        Name any_name = Name::build().set_identity_number(123);
        CHECK(partner.matches_name(any_name));
    }

    SUBCASE("single filter matching") {
        dp::Vector<NameFilter> filters;
        filters.push_back({NameFilterField::ManufacturerCode, 1234});
        PartnerCF partner(0, filters);

        Name matching = Name::build().set_manufacturer_code(1234);
        CHECK(partner.matches_name(matching));

        Name non_matching = Name::build().set_manufacturer_code(5678);
        CHECK_FALSE(partner.matches_name(non_matching));
    }

    SUBCASE("all filters must match (AND logic)") {
        dp::Vector<NameFilter> filters;
        filters.push_back({NameFilterField::ManufacturerCode, 1234});
        filters.push_back({NameFilterField::FunctionCode, 25});
        PartnerCF partner(0, filters);

        // Both match
        Name matching = Name::build().set_manufacturer_code(1234).set_function_code(25);
        CHECK(partner.matches_name(matching));

        // Only first matches
        Name partial1 = Name::build().set_manufacturer_code(1234).set_function_code(99);
        CHECK_FALSE(partner.matches_name(partial1));

        // Only second matches
        Name partial2 = Name::build().set_manufacturer_code(9999).set_function_code(25);
        CHECK_FALSE(partner.matches_name(partial2));

        // Neither matches
        Name none = Name::build().set_manufacturer_code(9999).set_function_code(99);
        CHECK_FALSE(partner.matches_name(none));
    }

    SUBCASE("complex multi-field matching") {
        dp::Vector<NameFilter> filters;
        filters.push_back({NameFilterField::ManufacturerCode, 1234});
        filters.push_back({NameFilterField::FunctionCode, 25});
        filters.push_back({NameFilterField::IndustryGroup, 2});
        PartnerCF partner(0, filters);

        Name exact_match = Name::build().set_manufacturer_code(1234).set_function_code(25).set_industry_group(2);
        CHECK(partner.matches_name(exact_match));

        // Extra fields are okay
        Name extra = Name::build()
                         .set_manufacturer_code(1234)
                         .set_function_code(25)
                         .set_industry_group(2)
                         .set_ecu_instance(5);
        CHECK(partner.matches_name(extra));
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// PartnerCF Event Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("PartnerCF events") {
    dp::Vector<NameFilter> filters;
    PartnerCF partner(0, filters);

    SUBCASE("on_partner_found event") {
        bool found = false;
        Address found_addr = NULL_ADDRESS;

        partner.on_partner_found.subscribe([&](Address addr) {
            found = true;
            found_addr = addr;
        });

        partner.on_partner_found.emit(0x42);

        CHECK(found);
        CHECK(found_addr == 0x42);
    }

    SUBCASE("on_partner_lost event") {
        bool lost = false;

        partner.on_partner_lost.subscribe([&]() { lost = true; });

        partner.on_partner_lost.emit();

        CHECK(lost);
    }

    SUBCASE("multiple event subscribers") {
        int count1 = 0;
        int count2 = 0;

        partner.on_partner_found.subscribe([&](Address) { count1++; });
        partner.on_partner_found.subscribe([&](Address) { count2++; });

        partner.on_partner_found.emit(0x42);

        CHECK(count1 == 1);
        CHECK(count2 == 1);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Practical Usage Scenarios
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("PartnerCF practical scenarios") {
    SUBCASE("tracking specific implement by manufacturer") {
        // Looking for a specific manufacturer's implement
        dp::Vector<NameFilter> filters;
        filters.push_back({NameFilterField::ManufacturerCode, 1234});
        filters.push_back({NameFilterField::FunctionCode, 40}); // Implement controller

        PartnerCF partner(0, filters);

        // Matching implement
        Name implement = Name::build()
                             .set_manufacturer_code(1234)
                             .set_function_code(40)
                             .set_identity_number(555);
        CHECK(partner.matches_name(implement));

        // Different manufacturer
        Name other_mfr = Name::build().set_manufacturer_code(5678).set_function_code(40).set_identity_number(555);
        CHECK_FALSE(partner.matches_name(other_mfr));

        // Same manufacturer, different function
        Name other_func = Name::build().set_manufacturer_code(1234).set_function_code(50).set_identity_number(555);
        CHECK_FALSE(partner.matches_name(other_func));
    }

    SUBCASE("tracking VT terminal") {
        // Looking for any VT terminal
        dp::Vector<NameFilter> filters;
        filters.push_back({NameFilterField::FunctionCode, 26}); // VT function

        PartnerCF vt_partner(0, filters);

        Name vt = Name::build().set_function_code(26).set_manufacturer_code(9999);
        CHECK(vt_partner.matches_name(vt));

        Name non_vt = Name::build().set_function_code(25).set_manufacturer_code(9999);
        CHECK_FALSE(vt_partner.matches_name(non_vt));
    }

    SUBCASE("tracking specific ECU instance") {
        // Looking for a specific ECU instance
        dp::Vector<NameFilter> filters;
        filters.push_back({NameFilterField::FunctionCode, 0});    // Engine
        filters.push_back({NameFilterField::ECUInstance, 1});     // Second instance

        PartnerCF ecu_partner(0, filters);

        Name ecu1 = Name::build().set_function_code(0).set_ecu_instance(1);
        CHECK(ecu_partner.matches_name(ecu1));

        Name ecu0 = Name::build().set_function_code(0).set_ecu_instance(0);
        CHECK_FALSE(ecu_partner.matches_name(ecu0));
    }

    SUBCASE("partner lifecycle tracking") {
        dp::Vector<NameFilter> filters;
        filters.push_back({NameFilterField::ManufacturerCode, 1234});

        PartnerCF partner(0, filters);

        bool found = false;
        bool lost = false;
        Address found_addr = NULL_ADDRESS;

        partner.on_partner_found.subscribe([&](Address addr) {
            found = true;
            found_addr = addr;
        });
        partner.on_partner_lost.subscribe([&]() { lost = true; });

        // Simulate partner discovery
        partner.set_address(0x20);
        partner.set_state(CFState::Online);
        partner.on_partner_found.emit(0x20);

        CHECK(found);
        CHECK(found_addr == 0x20);
        CHECK(partner.address() == 0x20);

        // Simulate partner loss
        partner.set_state(CFState::Offline);
        partner.on_partner_lost.emit();

        CHECK(lost);
    }
}
