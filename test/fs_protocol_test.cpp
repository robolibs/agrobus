#include <doctest/doctest.h>
#include <agrobus/isobus/fs/connection.hpp>
#include <agrobus/isobus/fs/properties.hpp>

using namespace agrobus::isobus;
using namespace agrobus::isobus::fs;

// ═══════════════════════════════════════════════════════════════════════════════
// FileServerProperties tests
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("FileServerProperties encode/decode") {
    SUBCASE("default values round-trip") {
        FileServerProperties props;
        props.version_number = 2;
        props.max_open_files = 16;
        props.supports_volumes = true;
        props.supports_long_filenames = true;
        props.max_simultaneous_clients = 4;

        auto encoded = props.encode();
        CHECK(encoded.size() == 8);
        CHECK(encoded[0] == FS_CMD_GET_PROPERTIES);

        auto decoded = FileServerProperties::decode(encoded);
        CHECK(decoded.version_number == 2);
        CHECK(decoded.max_open_files == 16);
        CHECK(decoded.supports_volumes == true);
        CHECK(decoded.supports_long_filenames == true);
        CHECK(decoded.max_simultaneous_clients == 4);
    }

    SUBCASE("capabilities bitfield") {
        FileServerProperties props;
        props.supports_volumes = false;
        props.supports_long_filenames = false;

        auto encoded = props.encode();
        auto decoded = FileServerProperties::decode(encoded);
        CHECK(decoded.supports_volumes == false);
        CHECK(decoded.supports_long_filenames == false);
    }

    SUBCASE("volumes only") {
        FileServerProperties props;
        props.supports_volumes = true;
        props.supports_long_filenames = false;

        auto encoded = props.encode();
        auto decoded = FileServerProperties::decode(encoded);
        CHECK(decoded.supports_volumes == true);
        CHECK(decoded.supports_long_filenames == false);
    }

    SUBCASE("long filenames only") {
        FileServerProperties props;
        props.supports_volumes = false;
        props.supports_long_filenames = true;

        auto encoded = props.encode();
        auto decoded = FileServerProperties::decode(encoded);
        CHECK(decoded.supports_volumes == false);
        CHECK(decoded.supports_long_filenames == true);
    }

    SUBCASE("custom values") {
        FileServerProperties props;
        props.version_number = 3;
        props.max_open_files = 32;
        props.max_simultaneous_clients = 8;

        auto encoded = props.encode();
        auto decoded = FileServerProperties::decode(encoded);
        CHECK(decoded.version_number == 3);
        CHECK(decoded.max_open_files == 32);
        CHECK(decoded.max_simultaneous_clients == 8);
    }

    SUBCASE("reserved bytes are 0xFF") {
        FileServerProperties props;
        auto encoded = props.encode();
        CHECK(encoded[5] == 0xFF);
        CHECK(encoded[6] == 0xFF);
        CHECK(encoded[7] == 0xFF);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// VolumeStatus tests
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("VolumeStatus encode/decode") {
    SUBCASE("mounted volume round-trip") {
        VolumeStatus vol;
        vol.name = "ISOBUS";
        vol.state = VolumeState::Mounted;
        vol.total_bytes = 1048576;  // 1 MB
        vol.free_bytes = 524288;    // 512 KB
        vol.removable = false;

        auto encoded = vol.encode();
        CHECK(encoded[0] == FS_CMD_GET_VOLUME_STATUS);

        auto decoded = VolumeStatus::decode(encoded);
        CHECK(decoded.name == "ISOBUS");
        CHECK(decoded.state == VolumeState::Mounted);
        CHECK(decoded.total_bytes == 1048576);
        CHECK(decoded.free_bytes == 524288);
        CHECK(decoded.removable == false);
    }

    SUBCASE("removable volume") {
        VolumeStatus vol;
        vol.name = "USB";
        vol.state = VolumeState::Mounted;
        vol.removable = true;
        vol.total_bytes = 4096;
        vol.free_bytes = 2048;

        auto encoded = vol.encode();
        auto decoded = VolumeStatus::decode(encoded);
        CHECK(decoded.removable == true);
        CHECK(decoded.name == "USB");
    }

    SUBCASE("not mounted state") {
        VolumeStatus vol;
        vol.name = "SD";
        vol.state = VolumeState::NotMounted;

        auto encoded = vol.encode();
        auto decoded = VolumeStatus::decode(encoded);
        CHECK(decoded.state == VolumeState::NotMounted);
    }

    SUBCASE("prepare for removal state") {
        VolumeStatus vol;
        vol.state = VolumeState::PrepareForRemoval;
        vol.name = "CF";

        auto encoded = vol.encode();
        auto decoded = VolumeStatus::decode(encoded);
        CHECK(decoded.state == VolumeState::PrepareForRemoval);
    }

    SUBCASE("maintenance state") {
        VolumeStatus vol;
        vol.state = VolumeState::Maintenance;
        vol.name = "INT";

        auto encoded = vol.encode();
        auto decoded = VolumeStatus::decode(encoded);
        CHECK(decoded.state == VolumeState::Maintenance);
    }

    SUBCASE("empty volume name") {
        VolumeStatus vol;
        vol.name = "";
        vol.total_bytes = 100;
        vol.free_bytes = 50;

        auto encoded = vol.encode();
        auto decoded = VolumeStatus::decode(encoded);
        CHECK(decoded.name.empty());
        CHECK(decoded.total_bytes == 100);
        CHECK(decoded.free_bytes == 50);
    }

    SUBCASE("large byte values") {
        VolumeStatus vol;
        vol.name = "BIG";
        vol.total_bytes = 0xFFFFFFFF;
        vol.free_bytes = 0x80000000;

        auto encoded = vol.encode();
        auto decoded = VolumeStatus::decode(encoded);
        CHECK(decoded.total_bytes == 0xFFFFFFFF);
        CHECK(decoded.free_bytes == 0x80000000);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Connection timeout tests
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("ClientConnection timeout (6s rule)") {
    SUBCASE("not timed out within window") {
        ClientConnection conn;
        conn.client_address = 0x10;
        conn.connected = true;
        conn.last_activity_ms = 1000;

        CHECK_FALSE(conn.is_timed_out(5000)); // 4s elapsed < 6s
    }

    SUBCASE("timed out after 6 seconds") {
        ClientConnection conn;
        conn.client_address = 0x10;
        conn.connected = true;
        conn.last_activity_ms = 0;

        CHECK(conn.is_timed_out(6000)); // exactly 6s
    }

    SUBCASE("timed out well past deadline") {
        ClientConnection conn;
        conn.client_address = 0x10;
        conn.connected = true;
        conn.last_activity_ms = 0;

        CHECK(conn.is_timed_out(10000)); // 10s elapsed > 6s
    }

    SUBCASE("not timed out if not connected") {
        ClientConnection conn;
        conn.client_address = 0x10;
        conn.connected = false;
        conn.last_activity_ms = 0;

        CHECK_FALSE(conn.is_timed_out(10000)); // disconnected, never times out
    }

    SUBCASE("touch resets timeout") {
        ClientConnection conn;
        conn.client_address = 0x10;
        conn.connected = true;
        conn.last_activity_ms = 0;

        conn.touch(5000); // activity at 5s
        CHECK_FALSE(conn.is_timed_out(8000)); // 3s since last activity < 6s
        CHECK(conn.is_timed_out(11000));      // 6s since last activity >= 6s
    }
}

TEST_CASE("ConnectionManager timeout handling") {
    ConnectionManager mgr(4);

    // Connect a client
    mgr.connect(0x10, 0);
    CHECK(mgr.active_client_count() == 1);

    SUBCASE("client removed after timeout") {
        auto timed_out = mgr.update(6000);
        CHECK(timed_out.size() == 1);
        CHECK(timed_out[0] == 0x10);
        CHECK(mgr.active_client_count() == 0);
    }

    SUBCASE("activity prevents timeout") {
        mgr.update(3000); // 3s elapsed
        mgr.record_activity(0x10, mgr.elapsed_total());

        auto timed_out = mgr.update(4000); // total 7s, but activity at 3s
        CHECK(timed_out.empty());
        CHECK(mgr.active_client_count() == 1);
    }

    SUBCASE("multiple clients can timeout independently") {
        mgr.connect(0x20, 0);
        mgr.update(3000); // 3s elapsed
        mgr.record_activity(0x20, mgr.elapsed_total()); // client 0x20 active at 3s

        auto timed_out = mgr.update(3000); // 6s total
        // Client 0x10 should timeout (6s since last activity at 0)
        // Client 0x20 should not (3s since last activity at 3s)
        CHECK(timed_out.size() == 1);
        CHECK(timed_out[0] == 0x10);
        CHECK(mgr.active_client_count() == 1);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Status cadence switching tests (idle vs busy)
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Status cadence switching (idle vs busy)") {
    ConnectionManager mgr(4);

    SUBCASE("idle interval when no clients") {
        CHECK(mgr.current_status_interval() == FS_STATUS_IDLE_INTERVAL_MS);
        CHECK(mgr.current_status_interval() == 2000);
    }

    SUBCASE("busy interval when clients connected") {
        mgr.connect(0x10, 0);
        CHECK(mgr.current_status_interval() == FS_STATUS_BUSY_INTERVAL_MS);
        CHECK(mgr.current_status_interval() == 200);
    }

    SUBCASE("returns to idle after client disconnect") {
        mgr.connect(0x10, 0);
        CHECK(mgr.current_status_interval() == FS_STATUS_BUSY_INTERVAL_MS);

        mgr.disconnect(0x10);
        CHECK(mgr.current_status_interval() == FS_STATUS_IDLE_INTERVAL_MS);
    }

    SUBCASE("returns to idle after client timeout") {
        mgr.connect(0x10, 0);
        CHECK(mgr.has_active_clients());

        mgr.update(6000); // timeout
        CHECK_FALSE(mgr.has_active_clients());
        CHECK(mgr.current_status_interval() == FS_STATUS_IDLE_INTERVAL_MS);
    }

    SUBCASE("should_send_status respects interval") {
        // No clients: 2000ms interval
        CHECK_FALSE(mgr.should_send_status(1000)); // 1000 < 2000
        CHECK(mgr.should_send_status(1000));       // 2000 >= 2000
    }

    SUBCASE("busy cadence sends more frequently") {
        mgr.connect(0x10, 0); // Switch to busy

        CHECK_FALSE(mgr.should_send_status(100)); // 100 < 200
        CHECK(mgr.should_send_status(100));       // 200 >= 200
    }
}

TEST_CASE("Status burst rate limiting") {
    ConnectionManager mgr(4);

    SUBCASE("allows bursts up to limit") {
        for (u8 i = 0; i < FS_MAX_STATUS_BURST_PER_SEC; ++i) {
            CHECK(mgr.can_send_status_burst());
        }
        CHECK_FALSE(mgr.can_send_status_burst()); // 6th burst blocked
    }

    SUBCASE("burst counter resets after 1 second") {
        // Exhaust burst allowance
        for (u8 i = 0; i < FS_MAX_STATUS_BURST_PER_SEC; ++i) {
            mgr.can_send_status_burst();
        }
        CHECK_FALSE(mgr.can_send_status_burst());

        // Advance time past 1 second (update resets burst counter)
        mgr.update(1000);
        CHECK(mgr.can_send_status_burst());
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// NACK for undefined commands
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("FSNack for undefined commands") {
    SUBCASE("encode not-supported NACK") {
        FSNack nack;
        nack.command_code = 0x99; // undefined command
        nack.error_code = FS_NACK_NOT_SUPPORTED;

        auto encoded = nack.encode();
        CHECK(encoded.size() == 8);
        CHECK(encoded[0] == 0x99);
        CHECK(encoded[1] == FS_NACK_NOT_SUPPORTED);
        // Remaining bytes are 0xFF (reserved)
        CHECK(encoded[2] == 0xFF);
        CHECK(encoded[3] == 0xFF);
        CHECK(encoded[4] == 0xFF);
        CHECK(encoded[5] == 0xFF);
        CHECK(encoded[6] == 0xFF);
        CHECK(encoded[7] == 0xFF);
    }

    SUBCASE("encode invalid-access NACK") {
        FSNack nack;
        nack.command_code = FS_CMD_PREPARE_VOLUME_REMOVAL;
        nack.error_code = FS_NACK_INVALID_ACCESS;

        auto encoded = nack.encode();
        CHECK(encoded[0] == FS_CMD_PREPARE_VOLUME_REMOVAL);
        CHECK(encoded[1] == FS_NACK_INVALID_ACCESS);
    }

    SUBCASE("encode volume-busy NACK") {
        FSNack nack;
        nack.command_code = FS_CMD_MAINTAIN_VOLUME;
        nack.error_code = FS_NACK_VOLUME_BUSY;

        auto encoded = nack.encode();
        CHECK(encoded[0] == FS_CMD_MAINTAIN_VOLUME);
        CHECK(encoded[1] == FS_NACK_VOLUME_BUSY);
    }

    SUBCASE("all enhanced FS commands produce valid NACKs") {
        u8 commands[] = {
            FS_CMD_GET_PROPERTIES,
            FS_CMD_GET_VOLUME_STATUS,
            FS_CMD_PREPARE_VOLUME_REMOVAL,
            FS_CMD_MAINTAIN_VOLUME
        };

        for (auto cmd : commands) {
            FSNack nack;
            nack.command_code = cmd;
            nack.error_code = FS_NACK_NOT_SUPPORTED;

            auto encoded = nack.encode();
            CHECK(encoded.size() == 8);
            CHECK(encoded[0] == cmd);
            CHECK(encoded[1] == FS_NACK_NOT_SUPPORTED);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// ConnectionManager additional tests
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("ConnectionManager capacity") {
    ConnectionManager mgr(2); // Only allow 2 clients

    CHECK(mgr.connect(0x10, 0));
    CHECK(mgr.connect(0x20, 0));
    CHECK_FALSE(mgr.connect(0x30, 0)); // Should fail - at capacity

    mgr.disconnect(0x10);
    CHECK(mgr.connect(0x30, 0)); // Now there's room
}

TEST_CASE("ConnectionManager duplicate connect") {
    ConnectionManager mgr(4);

    mgr.connect(0x10, 0);
    mgr.connect(0x10, 100); // Same address, just updates activity

    CHECK(mgr.active_client_count() == 1);
}

TEST_CASE("ConnectionManager get_connection") {
    ConnectionManager mgr(4);

    mgr.connect(0x10, 500);
    auto conn = mgr.get_connection(0x10);
    REQUIRE(conn.has_value());
    CHECK(conn.value().client_address == 0x10);
    CHECK(conn.value().connected == true);

    auto missing = mgr.get_connection(0x99);
    CHECK_FALSE(missing.has_value());
}

TEST_CASE("ConnectionManager open file tracking") {
    ConnectionManager mgr(4);

    mgr.connect(0x10, 0);
    mgr.increment_open_files(0x10);
    mgr.increment_open_files(0x10);

    auto conn = mgr.get_connection(0x10);
    REQUIRE(conn.has_value());
    CHECK(conn.value().open_file_count == 2);

    mgr.decrement_open_files(0x10);
    conn = mgr.get_connection(0x10);
    CHECK(conn.value().open_file_count == 1);
}

TEST_CASE("ClientConnection reset") {
    ClientConnection conn;
    conn.client_address = 0x10;
    conn.connected = true;
    conn.open_file_count = 5;
    conn.last_activity_ms = 1000;

    conn.reset();
    CHECK(conn.connected == false);
    CHECK(conn.open_file_count == 0);
    CHECK(conn.last_activity_ms == 0);
    // Address is preserved
    CHECK(conn.client_address == 0x10);
}

TEST_CASE("Constants have expected values") {
    CHECK(FS_CLIENT_TIMEOUT_MS == 6000);
    CHECK(FS_STATUS_IDLE_INTERVAL_MS == 2000);
    CHECK(FS_STATUS_BUSY_INTERVAL_MS == 200);
    CHECK(FS_MAX_STATUS_BURST_PER_SEC == 5);

    CHECK(FS_CMD_GET_PROPERTIES == 0x70);
    CHECK(FS_CMD_GET_VOLUME_STATUS == 0x71);
    CHECK(FS_CMD_PREPARE_VOLUME_REMOVAL == 0x72);
    CHECK(FS_CMD_MAINTAIN_VOLUME == 0x73);

    CHECK(FS_NACK_NOT_SUPPORTED == 0x01);
    CHECK(FS_NACK_INVALID_ACCESS == 0x02);
    CHECK(FS_NACK_VOLUME_BUSY == 0x03);
}
