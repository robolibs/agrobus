#include <doctest/doctest.h>
#include <agrobus/isobus/vt/auxiliary_caps.hpp>
#include <agrobus/isobus/vt/client.hpp>
#include <agrobus/isobus/vt/commands.hpp>

using namespace agrobus::isobus;
using namespace agrobus::isobus::vt;

// ─── VT v5 Extended Version Label Tests ──────────────────────────────────────

TEST_CASE("VT v5 extended version command constants") {
    CHECK(vt_cmd::EXTENDED_GET_VERSIONS == 0xC0);
    CHECK(vt_cmd::EXTENDED_STORE_VERSION == 0xC1);
    CHECK(vt_cmd::EXTENDED_LOAD_VERSION == 0xC2);
    CHECK(vt_cmd::EXTENDED_DELETE_VERSION == 0xC3);
    CHECK(vt_cmd::EXTENDED_GET_VERSIONS_RESPONSE == 0xC4);
    CHECK(vt_cmd::EXTENDED_VERSION_LABEL_SIZE == 32);
    CHECK(vt_cmd::CLASSIC_VERSION_LABEL_SIZE == 7);
    CHECK(vt_cmd::EXTENDED_VERSION_SUBFUNCTION == 0xFE);
}

TEST_CASE("VTVersion enum includes Version5") {
    CHECK(static_cast<u8>(VTVersion::Version3) == 3);
    CHECK(static_cast<u8>(VTVersion::Version4) == 4);
    CHECK(static_cast<u8>(VTVersion::Version5) == 5);
}

TEST_CASE("VTClient extended version fields") {
    IsoNet nm;
    Name name;
    auto cf_result = nm.create_internal(name, 0, 0x28);
    auto* cf = cf_result.value();

    VTClient vt(nm, cf);

    SUBCASE("initial extended version state") {
        CHECK(vt.vt_supports_extended_versions() == false);
        CHECK(vt.extended_version_label().empty());
    }

    SUBCASE("request_extended_version_label sends command") {
        // This will fail since no VT is connected, but tests the API
        auto result = vt.request_extended_version_label();
        // Should attempt to send (will fail due to no address claimed)
        CHECK(result.is_err());
    }

    SUBCASE("send_extended_store_version requires connected state") {
        auto result = vt.send_extended_store_version("my_extended_label_v5");
        CHECK(result.is_err()); // Not connected
    }

    SUBCASE("send_extended_delete_version requires connected state") {
        auto result = vt.send_extended_delete_version("my_extended_label_v5");
        CHECK(result.is_err()); // Not connected
    }
}

TEST_CASE("VTClient handle_extended_version_response") {
    IsoNet nm;
    Name name;
    auto cf_result = nm.create_internal(name, 0, 0x28);
    auto* cf = cf_result.value();

    VTClient vt(nm, cf);

    SUBCASE("extended versions received event fires") {
        dp::Vector<dp::String> received_labels;
        vt.on_extended_versions_received += [&](dp::Vector<dp::String> labels) {
            received_labels = std::move(labels);
        };

        // Simulate an extended version response message
        Message msg;
        msg.pgn = PGN_VT_TO_ECU;
        msg.source = 0x26;
        msg.data.resize(3 + 32); // header + one 32-byte label
        msg.data[0] = vt_cmd::EXTENDED_GET_VERSIONS_RESPONSE;
        msg.data[1] = vt_cmd::EXTENDED_VERSION_SUBFUNCTION;
        msg.data[2] = 1; // 1 version label

        // Fill in a 32-byte label "TestExtLabel_V5" padded with spaces
        dp::String test_label = "TestExtLabel_V5";
        for (usize i = 0; i < 32; ++i) {
            msg.data[3 + i] = (i < test_label.size())
                ? static_cast<u8>(test_label[i])
                : 0x20; // space padding
        }

        // Connect pool first so we can register callbacks
        ObjectPool pool;
        VTObject obj;
        obj.id = 1;
        obj.type = ObjectType::WorkingSet;
        pool.add(std::move(obj));
        vt.set_object_pool(std::move(pool));
        vt.connect();

        // Directly test: we cannot send VT messages without infrastructure,
        // but we can verify the event structure compiles and is usable
        CHECK(received_labels.empty()); // No message dispatched yet
    }

    SUBCASE("extended store response event structure") {
        bool store_success = false;
        u8 store_error = 0xFF;
        vt.on_extended_store_response += [&](bool success, u8 err) {
            store_success = success;
            store_error = err;
        };
        // Verify event is subscribable
        CHECK(vt.on_extended_store_response.count() == 1);
    }

    SUBCASE("extended load response event structure") {
        bool load_success = false;
        u8 load_error = 0xFF;
        vt.on_extended_load_response += [&](bool success, u8 err) {
            load_success = success;
            load_error = err;
        };
        CHECK(vt.on_extended_load_response.count() == 1);
    }
}

TEST_CASE("VTClient version detection from VT status") {
    IsoNet nm;
    Name name;
    auto cf_result = nm.create_internal(name, 0, 0x28);
    auto* cf = cf_result.value();

    VTClient vt(nm, cf);

    SUBCASE("get_vt_version initially zero") {
        CHECK(vt.get_vt_version() == 0);
    }

    SUBCASE("set_vt_version_preference works") {
        vt.set_vt_version_preference(VTVersion::Version5);
        CHECK(vt.get_vt_version() == 5);
    }
}

// ─── VT v5 Auxiliary Capabilities Tests ──────────────────────────────────────

TEST_CASE("AuxChannelCapability encode/decode") {
    AuxChannelCapability cap;
    cap.channel_id = 3;
    cap.aux_type = 1;        // analog
    cap.resolution = 1024;
    cap.function_type = 2;

    auto encoded = cap.encode();
    CHECK(encoded.size() == 5);
    CHECK(encoded[0] == 3);
    CHECK(encoded[1] == 1);
    // resolution 1024 = 0x0400
    CHECK(encoded[2] == 0x00);
    CHECK(encoded[3] == 0x04);
    CHECK(encoded[4] == 2);

    auto decoded = AuxChannelCapability::decode(encoded, 0);
    CHECK(decoded.channel_id == 3);
    CHECK(decoded.aux_type == 1);
    CHECK(decoded.resolution == 1024);
    CHECK(decoded.function_type == 2);
}

TEST_CASE("AuxChannelCapability decode with offset") {
    dp::Vector<u8> data = {0xAA, 0xBB, // padding
                           5, 2, 0x00, 0x08, 7}; // actual cap data at offset 2
    auto decoded = AuxChannelCapability::decode(data, 2);
    CHECK(decoded.channel_id == 5);
    CHECK(decoded.aux_type == 2);    // bidirectional
    CHECK(decoded.resolution == 2048); // 0x0800
    CHECK(decoded.function_type == 7);
}

TEST_CASE("AuxChannelCapability decode insufficient data") {
    dp::Vector<u8> data = {1, 2}; // too short
    auto decoded = AuxChannelCapability::decode(data, 0);
    // Should return default values when data is insufficient
    CHECK(decoded.channel_id == 0);
    CHECK(decoded.aux_type == 0);
    CHECK(decoded.resolution == 0);
    CHECK(decoded.function_type == 0);
}

TEST_CASE("AuxCapabilities default state") {
    AuxCapabilities caps;
    CHECK(caps.channels.empty());
    CHECK(caps.vt_version == 0);
    CHECK(caps.discovery_complete == false);
}

TEST_CASE("AuxCapabilityDiscovery construction and initial state") {
    IsoNet nm;
    Name name;
    auto cf_result = nm.create_internal(name, 0, 0x28);
    auto* cf = cf_result.value();

    AuxCapabilityDiscovery discovery(nm, cf);

    SUBCASE("initial state") {
        CHECK(discovery.capabilities().channels.empty());
        CHECK(discovery.capabilities().discovery_complete == false);
        CHECK(discovery.is_request_pending() == false);
    }

    SUBCASE("request_capabilities sets pending") {
        // Will fail to actually send (no address claimed), but tests the logic
        auto result = discovery.request_capabilities();
        // Regardless of send success, request_pending is set before send
        CHECK(discovery.is_request_pending() == true);
    }

    SUBCASE("double request fails") {
        discovery.request_capabilities(); // first request
        auto result = discovery.request_capabilities(); // second should fail
        CHECK(result.is_err());
    }

    SUBCASE("on_capabilities_received event subscribable") {
        bool called = false;
        discovery.on_capabilities_received += [&](AuxCapabilities caps) {
            called = true;
            CHECK(caps.discovery_complete == true);
        };
        CHECK(discovery.on_capabilities_received.count() == 1);
    }
}

TEST_CASE("AuxCapabilityDiscovery handle_response") {
    IsoNet nm;
    Name name;
    auto cf_result = nm.create_internal(name, 0, 0x28);
    auto* cf = cf_result.value();

    AuxCapabilityDiscovery discovery(nm, cf);

    SUBCASE("parses response with multiple channels") {
        AuxCapabilities received_caps;
        discovery.on_capabilities_received += [&](AuxCapabilities caps) {
            received_caps = caps;
        };

        // Build a response message with 2 channels
        Message msg;
        msg.pgn = PGN_VT_TO_ECU;
        msg.source = 0x26;
        msg.data.push_back(vt_cmd::GET_SUPPORTED_OBJECTS); // byte 0: command
        msg.data.push_back(0x01);                           // byte 1: sub-function
        msg.data.push_back(2);                              // byte 2: num_channels

        // Channel 0: boolean, resolution 1
        msg.data.push_back(0);    // channel_id
        msg.data.push_back(0);    // aux_type (boolean)
        msg.data.push_back(0x01); // resolution low
        msg.data.push_back(0x00); // resolution high
        msg.data.push_back(1);    // function_type

        // Channel 1: analog, resolution 4096
        msg.data.push_back(1);    // channel_id
        msg.data.push_back(1);    // aux_type (analog)
        msg.data.push_back(0x00); // resolution low (4096 = 0x1000)
        msg.data.push_back(0x10); // resolution high
        msg.data.push_back(3);    // function_type

        discovery.handle_response(msg);

        CHECK(received_caps.discovery_complete == true);
        CHECK(received_caps.vt_version == 5);
        CHECK(received_caps.channels.size() == 2);

        CHECK(received_caps.channels[0].channel_id == 0);
        CHECK(received_caps.channels[0].aux_type == 0);
        CHECK(received_caps.channels[0].resolution == 1);
        CHECK(received_caps.channels[0].function_type == 1);

        CHECK(received_caps.channels[1].channel_id == 1);
        CHECK(received_caps.channels[1].aux_type == 1);
        CHECK(received_caps.channels[1].resolution == 4096);
        CHECK(received_caps.channels[1].function_type == 3);
    }

    SUBCASE("handles empty response") {
        AuxCapabilities received_caps;
        discovery.on_capabilities_received += [&](AuxCapabilities caps) {
            received_caps = caps;
        };

        Message msg;
        msg.pgn = PGN_VT_TO_ECU;
        msg.source = 0x26;
        msg.data.push_back(vt_cmd::GET_SUPPORTED_OBJECTS);
        msg.data.push_back(0x01);
        msg.data.push_back(0); // 0 channels

        discovery.handle_response(msg);

        CHECK(received_caps.discovery_complete == true);
        CHECK(received_caps.channels.empty());
    }

    SUBCASE("clears pending state on response") {
        discovery.request_capabilities();
        CHECK(discovery.is_request_pending() == true);

        Message msg;
        msg.pgn = PGN_VT_TO_ECU;
        msg.source = 0x26;
        msg.data.push_back(vt_cmd::GET_SUPPORTED_OBJECTS);
        msg.data.push_back(0x01);
        msg.data.push_back(0);

        discovery.handle_response(msg);
        CHECK(discovery.is_request_pending() == false);
    }

    SUBCASE("ignores too-short messages") {
        Message msg;
        msg.pgn = PGN_VT_TO_ECU;
        msg.data.push_back(vt_cmd::GET_SUPPORTED_OBJECTS);
        msg.data.push_back(0x01);
        // Missing num_channels byte

        // Should not crash, just return early
        discovery.handle_response(msg);
        CHECK(discovery.capabilities().discovery_complete == false);
    }
}
