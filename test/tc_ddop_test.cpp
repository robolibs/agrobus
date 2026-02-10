#include <doctest/doctest.h>
#include <agrobus/isobus/tc/ddi_database.hpp>
#include <agrobus/isobus/tc/ddop.hpp>
#include <agrobus/isobus/tc/ddop_helpers.hpp>

using namespace agrobus::isobus;
using namespace agrobus::isobus::tc;
namespace ddi = agrobus::isobus::tc::ddi;

TEST_CASE("DDOP creation") {
    DDOP ddop;

    SUBCASE("empty pool") {
        CHECK(ddop.object_count() == 0);
        auto result = ddop.validate();
        CHECK(result.is_err()); // Must have at least one device
    }

    SUBCASE("add device") {
        DeviceObject dev;
        dev.designator = "TestSprayer";
        dev.software_version = "1.0";
        auto result = ddop.add_device(dev);
        CHECK(result.is_ok());
        CHECK(ddop.devices().size() == 1);
    }

    SUBCASE("add element") {
        DeviceElement elem;
        elem.type = DeviceElementType::Device;
        elem.designator = "Main";
        auto result = ddop.add_element(elem);
        CHECK(result.is_ok());
        CHECK(ddop.elements().size() == 1);
    }

    SUBCASE("add process data") {
        DeviceProcessData pd;
        pd.ddi = 0x0001; // Setpoint Volume Per Area
        pd.trigger_methods = static_cast<u8>(TriggerMethod::OnChange);
        auto result = ddop.add_process_data(pd);
        CHECK(result.is_ok());
        CHECK(ddop.process_data().size() == 1);
    }
}

TEST_CASE("DDOP validation") {
    DDOP ddop;

    SUBCASE("valid pool") {
        DeviceObject dev;
        dev.id = 1;
        dev.designator = "Device";
        ddop.add_device(dev);

        DeviceElement elem;
        elem.id = 2;
        elem.parent_id = 1;
        elem.type = DeviceElementType::Device;
        ddop.add_element(elem);

        auto result = ddop.validate();
        CHECK(result.is_ok());
    }

    SUBCASE("invalid parent reference") {
        DeviceObject dev;
        dev.id = 1;
        dev.designator = "Device";
        ddop.add_device(dev);

        DeviceElement elem;
        elem.id = 2;
        elem.parent_id = 999; // Non-existent
        ddop.add_element(elem);

        auto result = ddop.validate();
        CHECK(result.is_err());
    }
}

TEST_CASE("DDOP serialization") {
    DDOP ddop;

    DeviceObject dev;
    dev.id = 1;
    dev.designator = "Sprayer";
    dev.software_version = "2.0";
    ddop.add_device(dev);

    DeviceElement elem;
    elem.id = 2;
    elem.type = DeviceElementType::Device;
    elem.designator = "Root";
    elem.parent_id = 1;
    ddop.add_element(elem);

    auto result = ddop.serialize();
    CHECK(result.is_ok());
    CHECK(result.value().size() > 0);
}

TEST_CASE("DDOP clear") {
    DDOP ddop;
    DeviceObject dev;
    dev.designator = "Test";
    ddop.add_device(dev);
    CHECK(ddop.object_count() == 1);

    ddop.clear();
    CHECK(ddop.object_count() == 0);
}

TEST_CASE("DDOP deserialize round-trip") {
    DDOP original;

    DeviceObject dev;
    dev.id = 1;
    dev.designator = "Sprayer";
    dev.software_version = "3.1";
    dev.serial_number = "SN12345";
    original.add_device(dev);

    DeviceElement elem;
    elem.id = 2;
    elem.type = DeviceElementType::Device;
    elem.designator = "Root";
    elem.number = 0;
    elem.parent_id = 1;
    elem.child_objects = {3, 4};
    original.add_element(elem);

    DeviceProcessData pd;
    pd.id = 3;
    pd.ddi = 0x0001;
    pd.trigger_methods = 0x08;
    pd.designator = "Rate";
    pd.presentation_object_id = 5;
    original.add_process_data(pd);

    DeviceProperty prop;
    prop.id = 4;
    prop.ddi = 0x0086;
    prop.value = -500;
    prop.designator = "OffsetX";
    prop.presentation_object_id = 5;
    original.add_property(prop);

    DeviceValuePresentation vp;
    vp.id = 5;
    vp.offset = 0;
    vp.scale = 0.001f;
    vp.decimal_digits = 3;
    vp.unit_designator = "mm";
    original.add_value_presentation(vp);

    // Serialize
    auto ser_result = original.serialize();
    REQUIRE(ser_result.is_ok());

    // Deserialize
    auto deser_result = DDOP::deserialize(ser_result.value());
    REQUIRE(deser_result.is_ok());

    auto& restored = deser_result.value();
    CHECK(restored.object_count() == 5);
    CHECK(restored.devices().size() == 1);
    CHECK(restored.elements().size() == 1);
    CHECK(restored.process_data().size() == 1);
    CHECK(restored.properties().size() == 1);

    // Verify device
    CHECK(restored.devices()[0].id == 1);
    CHECK(restored.devices()[0].designator == "Sprayer");
    CHECK(restored.devices()[0].software_version == "3.1");
    CHECK(restored.devices()[0].serial_number == "SN12345");

    // Verify element
    CHECK(restored.elements()[0].id == 2);
    CHECK(restored.elements()[0].type == DeviceElementType::Device);
    CHECK(restored.elements()[0].designator == "Root");
    CHECK(restored.elements()[0].child_objects.size() == 2);

    // Verify process data
    CHECK(restored.process_data()[0].ddi == 0x0001);
    CHECK(restored.process_data()[0].trigger_methods == 0x08);
    CHECK(restored.process_data()[0].designator == "Rate");

    // Verify property
    CHECK(restored.properties()[0].ddi == 0x0086);
    CHECK(restored.properties()[0].value == -500);

    // Re-serialize and compare
    auto reser_result = restored.serialize();
    REQUIRE(reser_result.is_ok());
    CHECK(reser_result.value() == ser_result.value());
}

TEST_CASE("DDOP ISOXML generation") {
    DDOP ddop;

    DeviceObject dev;
    dev.id = 1;
    dev.designator = "TestDevice";
    dev.software_version = "1.0";
    ddop.add_device(dev);

    DeviceElement elem;
    elem.id = 2;
    elem.type = DeviceElementType::Function;
    elem.designator = "Boom";
    elem.number = 1;
    elem.parent_id = 1;
    elem.child_objects = {3};
    ddop.add_element(elem);

    DeviceProcessData pd;
    pd.id = 3;
    pd.ddi = 0x0001;
    pd.trigger_methods = 0x08;
    pd.designator = "SetpointRate";
    ddop.add_process_data(pd);

    dp::String xml = ddop.to_isoxml();
    CHECK(xml.find("ISO11783_TaskData") != dp::String::npos);
    CHECK(xml.find("DVC") != dp::String::npos);
    CHECK(xml.find("TestDevice") != dp::String::npos);
    CHECK(xml.find("DET") != dp::String::npos);
    CHECK(xml.find("DPD") != dp::String::npos);
    CHECK(xml.find("SetpointRate") != dp::String::npos);
}

TEST_CASE("DDI Database") {
    SUBCASE("lookup known DDI") {
        auto entry = DDIDatabase::lookup(1);
        REQUIRE(entry.has_value());
        CHECK(dp::String(entry->name) == "Setpoint Volume Per Area Application Rate");
        CHECK(dp::String(entry->unit) == "mm3/m2");
        CHECK(entry->resolution == doctest::Approx(0.01));
    }

    SUBCASE("lookup unknown DDI") {
        auto entry = DDIDatabase::lookup(9999);
        CHECK(!entry.has_value());
    }

    SUBCASE("lookup high DDI") {
        auto entry = DDIDatabase::lookup(65535);
        REQUIRE(entry.has_value());
        CHECK(dp::String(entry->name) == "Reserved");
    }

    SUBCASE("unit_for") {
        CHECK(dp::String(DDIDatabase::unit_for(116)) == "m2");
    }

    SUBCASE("is_geometry_ddi") {
        CHECK(DDIDatabase::is_geometry_ddi(134));
        CHECK(DDIDatabase::is_geometry_ddi(135));
        CHECK(!DDIDatabase::is_geometry_ddi(1));
    }

    SUBCASE("is_rate_ddi") {
        CHECK(DDIDatabase::is_rate_ddi(1));
        CHECK(DDIDatabase::is_rate_ddi(7));
        CHECK(!DDIDatabase::is_rate_ddi(116));
    }

    SUBCASE("engineering conversions") {
        // DDI 1 has resolution 0.01 per ISO 11783-11
        CHECK(DDIDatabase::to_engineering(1, 1000) == doctest::Approx(10.0));
        CHECK(DDIDatabase::from_engineering(1, 10.0) == 1000);
        // DDI 116 (Total Area) has resolution 1.0
        CHECK(DDIDatabase::to_engineering(116, 500) == doctest::Approx(500.0));
    }
}

TEST_CASE("DDOP Helpers") {
    DDOP ddop;

    DeviceObject dev;
    dev.id = 1;
    dev.designator = "Sprayer";
    ddop.add_device(dev);

    // Connector with offsets
    DeviceElement connector;
    connector.id = 10;
    connector.type = DeviceElementType::Connector;
    connector.designator = "Hitch";
    connector.parent_id = 1;
    connector.child_objects = {20};
    ddop.add_element(connector);

    DeviceProperty pivot_x;
    pivot_x.id = 20;
    pivot_x.ddi = ddi::CONNECTOR_PIVOT_X_OFFSET;
    pivot_x.value = -1500; // 1.5m behind reference
    ddop.add_property(pivot_x);

    // Function element (boom)
    DeviceElement boom;
    boom.id = 11;
    boom.type = DeviceElementType::Function;
    boom.designator = "Boom";
    boom.parent_id = 1;
    boom.child_objects = {21, 22};
    ddop.add_element(boom);

    DeviceProperty boom_x;
    boom_x.id = 21;
    boom_x.ddi = ddi::DEVICE_ELEMENT_OFFSET_X;
    boom_x.value = -3000;
    ddop.add_property(boom_x);

    DeviceProperty boom_y;
    boom_y.id = 22;
    boom_y.ddi = ddi::DEVICE_ELEMENT_OFFSET_Y;
    boom_y.value = 0;
    ddop.add_property(boom_y);

    // Section elements
    DeviceElement sec1;
    sec1.id = 12;
    sec1.type = DeviceElementType::Section;
    sec1.designator = "Section1";
    sec1.number = 1;
    sec1.parent_id = 11;
    sec1.child_objects = {30, 31};
    ddop.add_element(sec1);

    DeviceProperty sec1_y;
    sec1_y.id = 30;
    sec1_y.ddi = ddi::DEVICE_ELEMENT_OFFSET_Y;
    sec1_y.value = -1500;
    ddop.add_property(sec1_y);

    DeviceProperty sec1_width;
    sec1_width.id = 31;
    sec1_width.ddi = ddi::ACTUAL_WORKING_WIDTH;
    sec1_width.value = 3000;
    ddop.add_property(sec1_width);

    DeviceElement sec2;
    sec2.id = 13;
    sec2.type = DeviceElementType::Section;
    sec2.designator = "Section2";
    sec2.number = 2;
    sec2.parent_id = 11;
    sec2.child_objects = {32, 33};
    ddop.add_element(sec2);

    DeviceProperty sec2_y;
    sec2_y.id = 32;
    sec2_y.ddi = ddi::DEVICE_ELEMENT_OFFSET_Y;
    sec2_y.value = 1500;
    ddop.add_property(sec2_y);

    DeviceProperty sec2_width;
    sec2_width.id = 33;
    sec2_width.ddi = ddi::ACTUAL_WORKING_WIDTH;
    sec2_width.value = 3000;
    ddop.add_property(sec2_width);

    // Rate process data
    DeviceProcessData rate;
    rate.id = 40;
    rate.ddi = ddi::SETPOINT_VOLUME_PER_AREA;
    rate.trigger_methods = 0x08;
    rate.designator = "AppRate";
    ddop.add_process_data(rate);

    SUBCASE("extract geometry") {
        auto geo = DDOPHelpers::extract_geometry(ddop);
        CHECK(geo.connector_x_mm == -1500);
        CHECK(geo.sections.size() == 2);
        CHECK(geo.sections[0].offset_y_mm == -1500);
        CHECK(geo.sections[0].width_mm == 3000);
        CHECK(geo.sections[1].offset_y_mm == 1500);
        CHECK(geo.total_width_mm == 6000);
    }

    SUBCASE("extract rates") {
        auto rates = DDOPHelpers::extract_rates(ddop);
        CHECK(rates.size() == 1);
        CHECK(rates[0].ddi == ddi::SETPOINT_VOLUME_PER_AREA);
        CHECK(rates[0].designator == "AppRate");
    }

    SUBCASE("section count") {
        CHECK(DDOPHelpers::section_count(ddop) == 2);
    }
}
