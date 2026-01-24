#include <doctest/doctest.h>
#include <agrobus/nmea/n2k_management.hpp>

using namespace agrobus::nmea;

TEST_CASE("N2KProductInfo encode/decode round-trip") {
    N2KProductInfo info;
    info.nmea2000_version = 0x0901;
    info.product_code = 1234;
    info.model_id = "TestModel";
    info.software_version = "1.0.0";
    info.model_version = "RevA";
    info.serial_code = "SN123456";
    info.certification_level = 2;
    info.load_equivalency = 3;

    auto encoded = info.encode();

    // Expected size: 2+2+32+40+24+32+1+1 = 134 bytes
    CHECK(encoded.size() == 134);

    auto result = N2KProductInfo::decode(encoded);
    REQUIRE(result.is_ok());

    auto decoded = result.value();
    CHECK(decoded.nmea2000_version == 0x0901);
    CHECK(decoded.product_code == 1234);
    CHECK(decoded.model_id == "TestModel");
    CHECK(decoded.software_version == "1.0.0");
    CHECK(decoded.model_version == "RevA");
    CHECK(decoded.serial_code == "SN123456");
    CHECK(decoded.certification_level == 2);
    CHECK(decoded.load_equivalency == 3);
}

TEST_CASE("N2KProductInfo decode rejects short data") {
    dp::Vector<u8> short_data(50, 0xFF);
    auto result = N2KProductInfo::decode(short_data);
    CHECK_FALSE(result.is_ok());
}

TEST_CASE("N2KProductInfo with max-length strings") {
    N2KProductInfo info;
    info.model_id = "MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM";         // 32 chars
    info.software_version = "SSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSS"; // 40 chars
    info.model_version = "VVVVVVVVVVVVVVVVVVVVVVVV";    // 24 chars
    info.serial_code = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";      // 32 chars

    auto encoded = info.encode();
    auto result = N2KProductInfo::decode(encoded);
    REQUIRE(result.is_ok());

    auto decoded = result.value();
    CHECK(decoded.model_id.size() == 32);
    CHECK(decoded.software_version.size() == 40);
    CHECK(decoded.model_version.size() == 24);
    CHECK(decoded.serial_code.size() == 32);
}

TEST_CASE("N2KProductInfo with empty strings") {
    N2KProductInfo info;
    // All strings left empty

    auto encoded = info.encode();
    CHECK(encoded.size() == 134);

    auto result = N2KProductInfo::decode(encoded);
    REQUIRE(result.is_ok());

    auto decoded = result.value();
    CHECK(decoded.model_id.empty());
    CHECK(decoded.software_version.empty());
    CHECK(decoded.model_version.empty());
    CHECK(decoded.serial_code.empty());
}

TEST_CASE("N2KConfigInfo encode/decode round-trip") {
    N2KConfigInfo info;
    info.installation_desc1 = "Port side installation";
    info.installation_desc2 = "Starboard connection";
    info.manufacturer_info = "ACME Marine Electronics";

    auto encoded = info.encode();

    auto result = N2KConfigInfo::decode(encoded);
    REQUIRE(result.is_ok());

    auto decoded = result.value();
    CHECK(decoded.installation_desc1 == "Port side installation");
    CHECK(decoded.installation_desc2 == "Starboard connection");
    CHECK(decoded.manufacturer_info == "ACME Marine Electronics");
}

TEST_CASE("N2KConfigInfo with empty fields") {
    N2KConfigInfo info;
    // All fields empty

    auto encoded = info.encode();
    // Should be 6 bytes (3 x 2-byte length prefixes, all zero)
    CHECK(encoded.size() == 6);

    auto result = N2KConfigInfo::decode(encoded);
    REQUIRE(result.is_ok());

    auto decoded = result.value();
    CHECK(decoded.installation_desc1.empty());
    CHECK(decoded.installation_desc2.empty());
    CHECK(decoded.manufacturer_info.empty());
}

TEST_CASE("N2KConfigInfo decode rejects truncated data") {
    SUBCASE("too short for headers") {
        dp::Vector<u8> data = {0x05, 0x00};  // claims 5 bytes but not enough
        auto result = N2KConfigInfo::decode(data);
        CHECK_FALSE(result.is_ok());
    }

    SUBCASE("desc1 length exceeds data") {
        dp::Vector<u8> data = {0x10, 0x00, 0x41, 0x42};  // claims 16 bytes, has 2
        auto result = N2KConfigInfo::decode(data);
        CHECK_FALSE(result.is_ok());
    }
}

TEST_CASE("N2KHeartbeat encode/decode") {
    SUBCASE("default values") {
        N2KHeartbeat hb;
        hb.update_interval_ms = 60000;
        hb.sequence_counter = 5;
        hb.controller_class1 = 0xAB;
        hb.controller_class2 = 0xCD;

        auto encoded = hb.encode();
        CHECK(encoded.size() == 8);

        auto decoded = N2KHeartbeat::decode(encoded);
        CHECK(decoded.update_interval_ms == 60000);
        CHECK(decoded.sequence_counter == 5);
        CHECK(decoded.controller_class1 == 0xAB);
        CHECK(decoded.controller_class2 == 0xCD);
    }

    SUBCASE("interval encoding resolution") {
        N2KHeartbeat hb;
        hb.update_interval_ms = 5000; // 5 seconds -> 100 * 50ms

        auto encoded = hb.encode();
        auto decoded = N2KHeartbeat::decode(encoded);
        CHECK(decoded.update_interval_ms == 5000);
    }

    SUBCASE("sequence counter wraps at 4 bits") {
        N2KHeartbeat hb;
        hb.sequence_counter = 0x0F; // max 4-bit value

        auto encoded = hb.encode();
        auto decoded = N2KHeartbeat::decode(encoded);
        CHECK(decoded.sequence_counter == 0x0F);
    }

    SUBCASE("reserved bytes are 0xFF") {
        N2KHeartbeat hb;
        auto encoded = hb.encode();
        CHECK(encoded[5] == 0xFF);
        CHECK(encoded[6] == 0xFF);
        CHECK(encoded[7] == 0xFF);
    }

    SUBCASE("decode from short data uses defaults") {
        dp::Vector<u8> empty;
        auto decoded = N2KHeartbeat::decode(empty);
        CHECK(decoded.update_interval_ms == 60000); // struct default
        CHECK(decoded.sequence_counter == 0);
    }
}

TEST_CASE("N2KManagement heartbeat timing") {
    IsoNet nm;
    Name name;
    auto* cf = nm.create_internal(name, 0, 0x28).value();

    N2KManagementConfig config;
    config.heartbeat_interval_ms = 1000; // 1 second for testing

    N2KManagement mgmt(nm, cf, config);
    mgmt.initialize();

    SUBCASE("no heartbeat before interval") {
        bool hb_received = false;
        mgmt.on_heartbeat_received.subscribe([&](N2KHeartbeat, Address) {
            hb_received = true;
        });

        mgmt.update(500); // Only 500ms elapsed
        // Heartbeat not triggered yet via event (would need loopback)
        CHECK(mgmt.heartbeat_sequence() == 0);
    }

    SUBCASE("heartbeat sent after interval") {
        mgmt.update(1000); // Exactly 1000ms
        CHECK(mgmt.heartbeat_sequence() == 1);
    }

    SUBCASE("multiple heartbeats increment sequence") {
        mgmt.update(1000);
        CHECK(mgmt.heartbeat_sequence() == 1);

        mgmt.update(1000);
        CHECK(mgmt.heartbeat_sequence() == 2);

        mgmt.update(1000);
        CHECK(mgmt.heartbeat_sequence() == 3);
    }

    SUBCASE("sequence wraps at 16") {
        for (int i = 0; i < 16; ++i) {
            mgmt.update(1000);
        }
        // After 16 increments, should wrap (counter & 0x0F)
        CHECK(mgmt.heartbeat_sequence() == 0);
    }
}

TEST_CASE("N2KManagement product info request/response") {
    IsoNet nm;
    Name name;
    auto* cf = nm.create_internal(name, 0, 0x28).value();

    N2KProductInfo pinfo;
    pinfo.product_code = 42;
    pinfo.model_id = "AgriBot";
    pinfo.software_version = "2.0.0";

    N2KManagementConfig config;
    config.product_info = pinfo;

    N2KManagement mgmt(nm, cf, config);
    mgmt.initialize();

    SUBCASE("product info accessible") {
        CHECK(mgmt.product_info().product_code == 42);
        CHECK(mgmt.product_info().model_id == "AgriBot");
        CHECK(mgmt.product_info().software_version == "2.0.0");
    }

    SUBCASE("set_product_info updates stored info") {
        N2KProductInfo new_info;
        new_info.product_code = 99;
        new_info.model_id = "NewBot";

        mgmt.set_product_info(new_info);
        CHECK(mgmt.product_info().product_code == 99);
        CHECK(mgmt.product_info().model_id == "NewBot");
    }

    SUBCASE("send_product_info emits requested event") {
        bool requested = false;
        mgmt.on_product_info_requested.subscribe([&](Address addr) {
            requested = true;
            CHECK(addr == 0x28);
        });

        mgmt.send_product_info();
        CHECK(requested);
    }

    SUBCASE("send_config_info emits requested event") {
        N2KConfigInfo cinfo;
        cinfo.installation_desc1 = "Front mount";
        mgmt.set_config_info(cinfo);

        bool requested = false;
        Address req_addr = 0;
        mgmt.on_config_info_requested.subscribe([&](Address addr) {
            requested = true;
            req_addr = addr;
        });

        mgmt.send_config_info(0x30);
        CHECK(requested);
        CHECK(req_addr == 0x30);
    }
}

TEST_CASE("N2KManagement config info") {
    IsoNet nm;
    Name name;
    auto* cf = nm.create_internal(name, 0, 0x28).value();

    N2KConfigInfo cinfo;
    cinfo.installation_desc1 = "Primary GPS";
    cinfo.installation_desc2 = "Rear mounted";
    cinfo.manufacturer_info = "AgriTech Inc.";

    N2KManagementConfig config;
    config.config_info = cinfo;

    N2KManagement mgmt(nm, cf, config);
    mgmt.initialize();

    CHECK(mgmt.config_info().installation_desc1 == "Primary GPS");
    CHECK(mgmt.config_info().installation_desc2 == "Rear mounted");
    CHECK(mgmt.config_info().manufacturer_info == "AgriTech Inc.");
}

TEST_CASE("N2KManagementConfig fluent API") {
    N2KProductInfo pinfo;
    pinfo.model_id = "Test";

    N2KConfigInfo cinfo;
    cinfo.installation_desc1 = "Desc";

    auto config = N2KManagementConfig{}
        .product(pinfo)
        .config(cinfo)
        .heartbeat_interval(5000);

    CHECK(config.product_info.model_id == "Test");
    CHECK(config.config_info.installation_desc1 == "Desc");
    CHECK(config.heartbeat_interval_ms == 5000);
}
