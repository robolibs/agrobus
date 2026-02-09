#include <doctest/doctest.h>
#include <agrobus/j1939/proprietary.hpp>

using namespace agrobus::j1939;
using namespace agrobus::net;

// ═════════════════════════════════════════════════════════════════════════════
// ProprietaryMsg Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("ProprietaryMsg structure") {
    SUBCASE("default initialization") {
        ProprietaryMsg msg;

        CHECK(msg.pgn == PGN_PROPRIETARY_A);
        CHECK(msg.data.size() == 0);
        CHECK(msg.source == NULL_ADDRESS);
        CHECK(msg.destination == BROADCAST_ADDRESS);
    }

    SUBCASE("field assignment") {
        ProprietaryMsg msg;
        msg.pgn = PGN_PROPRIETARY_B_BASE + 0x42;
        msg.data = {0xAA, 0xBB, 0xCC};
        msg.source = 0x25;
        msg.destination = 0x80;

        CHECK(msg.pgn == (PGN_PROPRIETARY_B_BASE + 0x42));
        CHECK(msg.data.size() == 3);
        CHECK(msg.source == 0x25);
        CHECK(msg.destination == 0x80);
    }
}

TEST_CASE("ProprietaryMsg type identification") {
    SUBCASE("Proprietary A identification") {
        ProprietaryMsg msg;
        msg.pgn = PGN_PROPRIETARY_A;

        CHECK(msg.is_proprietary_a());
        CHECK_FALSE(msg.is_proprietary_a2());
        CHECK_FALSE(msg.is_proprietary_b());
    }

    SUBCASE("Proprietary A2 identification") {
        ProprietaryMsg msg;
        msg.pgn = PGN_PROPRIETARY_A2;

        CHECK_FALSE(msg.is_proprietary_a());
        CHECK(msg.is_proprietary_a2());
        CHECK_FALSE(msg.is_proprietary_b());
    }

    SUBCASE("Proprietary B identification") {
        ProprietaryMsg msg;
        msg.pgn = PGN_PROPRIETARY_B_BASE;

        CHECK_FALSE(msg.is_proprietary_a());
        CHECK_FALSE(msg.is_proprietary_a2());
        CHECK(msg.is_proprietary_b());
    }

    SUBCASE("Proprietary B range") {
        // Proprietary B spans 0xFF00 to 0xFFFF
        ProprietaryMsg msg1;
        msg1.pgn = PGN_PROPRIETARY_B_BASE; // 0xFF00
        CHECK(msg1.is_proprietary_b());

        ProprietaryMsg msg2;
        msg2.pgn = 0xFFFF; // Last B PGN
        CHECK(msg2.is_proprietary_b());

        ProprietaryMsg msg3;
        msg3.pgn = 0xFF80; // Middle of B range
        CHECK(msg3.is_proprietary_b());
    }
}

TEST_CASE("ProprietaryMsg group extension") {
    SUBCASE("group extension extraction") {
        ProprietaryMsg msg;
        msg.pgn = PGN_PROPRIETARY_B_BASE + 0x42;

        CHECK(msg.group_extension() == 0x42);
    }

    SUBCASE("various group extensions") {
        ProprietaryMsg msg;

        msg.pgn = PGN_PROPRIETARY_B_BASE + 0x00;
        CHECK(msg.group_extension() == 0x00);

        msg.pgn = PGN_PROPRIETARY_B_BASE + 0x7F;
        CHECK(msg.group_extension() == 0x7F);

        msg.pgn = PGN_PROPRIETARY_B_BASE + 0xFF;
        CHECK(msg.group_extension() == 0xFF);
    }

    SUBCASE("group extension for non-B PGNs") {
        // group_extension() works on any PGN (extracts low byte)
        ProprietaryMsg msg;
        msg.pgn = PGN_PROPRIETARY_A; // 0xEF00

        CHECK(msg.group_extension() == 0x00);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// PGN Constants Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("Proprietary PGN constants") {
    SUBCASE("PGN values") {
        // Verify PGN constants are defined correctly
        CHECK(PGN_PROPRIETARY_A == 0xEF00);
        CHECK(PGN_PROPRIETARY_A2 == 0x1EF00);
        CHECK(PGN_PROPRIETARY_B_BASE == 0xFF00);
    }

    SUBCASE("Proprietary B range calculation") {
        // Proprietary B is 0xFF00-0xFFFF (256 PGNs)
        PGN first = PGN_PROPRIETARY_B_BASE;
        PGN last = 0xFFFF;

        CHECK(first == 0xFF00);
        CHECK(last == 0xFFFF);
        CHECK((last - first + 1) == 256);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Message Characteristics Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("Proprietary message characteristics") {
    SUBCASE("Proprietary A is destination-specific") {
        ProprietaryMsg msg;
        msg.pgn = PGN_PROPRIETARY_A;
        msg.destination = 0x42;

        CHECK(msg.is_proprietary_a());
        CHECK(msg.destination != BROADCAST_ADDRESS);
    }

    SUBCASE("Proprietary A2 is destination-specific") {
        ProprietaryMsg msg;
        msg.pgn = PGN_PROPRIETARY_A2;
        msg.destination = 0x80;

        CHECK(msg.is_proprietary_a2());
        CHECK(msg.destination != BROADCAST_ADDRESS);
    }

    SUBCASE("Proprietary B is broadcast") {
        ProprietaryMsg msg;
        msg.pgn = PGN_PROPRIETARY_B_BASE + 0x10;
        msg.destination = BROADCAST_ADDRESS;

        CHECK(msg.is_proprietary_b());
        CHECK(msg.destination == BROADCAST_ADDRESS);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Data Payload Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("Proprietary message data payloads") {
    SUBCASE("small payload (single frame)") {
        ProprietaryMsg msg;
        msg.data = {0x01, 0x02, 0x03, 0x04};

        CHECK(msg.data.size() == 4);
        CHECK(msg.data.size() <= 8); // Fits in single CAN frame
    }

    SUBCASE("full single frame payload") {
        ProprietaryMsg msg;
        msg.data = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};

        CHECK(msg.data.size() == 8);
    }

    SUBCASE("large payload (requires transport protocol)") {
        ProprietaryMsg msg;
        msg.pgn = PGN_PROPRIETARY_A; // Supports TP

        // Create large payload
        for (int i = 0; i < 100; ++i) {
            msg.data.push_back(static_cast<u8>(i));
        }

        CHECK(msg.data.size() == 100);
        CHECK(msg.data.size() > 8); // Requires TP
        CHECK(msg.is_proprietary_a());
    }

    SUBCASE("maximum TP payload") {
        ProprietaryMsg msg;
        msg.pgn = PGN_PROPRIETARY_A;

        // J1939 TP max is 1785 bytes
        for (int i = 0; i < 1785; ++i) {
            msg.data.push_back(static_cast<u8>(i & 0xFF));
        }

        CHECK(msg.data.size() == 1785);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Realistic Usage Scenarios
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("Proprietary message scenarios") {
    SUBCASE("manufacturer-specific command (Proprietary A)") {
        // ECU sends proprietary command to specific device
        ProprietaryMsg cmd;
        cmd.pgn = PGN_PROPRIETARY_A;
        cmd.source = 0x25;
        cmd.destination = 0x80;

        // Custom command format (manufacturer-defined)
        cmd.data.push_back(0x01); // Command ID
        cmd.data.push_back(0x42); // Parameter 1
        cmd.data.push_back(0x10); // Parameter 2

        CHECK(cmd.is_proprietary_a());
        CHECK(cmd.destination == 0x80);
        CHECK(cmd.data.size() == 3);
    }

    SUBCASE("extended data transfer (Proprietary A2)") {
        // Large data transfer using extended data page
        ProprietaryMsg transfer;
        transfer.pgn = PGN_PROPRIETARY_A2;
        transfer.source = 0x30;
        transfer.destination = 0x50;

        // Large dataset
        for (int i = 0; i < 500; ++i) {
            transfer.data.push_back(static_cast<u8>(i % 256));
        }

        CHECK(transfer.is_proprietary_a2());
        CHECK(transfer.data.size() == 500);
    }

    SUBCASE("broadcast status message (Proprietary B)") {
        // Manufacturer broadcasts periodic status
        ProprietaryMsg status;
        status.pgn = PGN_PROPRIETARY_B_BASE + 0x10; // Group extension 0x10
        status.source = 0x42;
        status.destination = BROADCAST_ADDRESS;

        // Status data
        status.data = {0x01, 0x02, 0x03, 0x04, 0x05};

        CHECK(status.is_proprietary_b());
        CHECK(status.group_extension() == 0x10);
        CHECK(status.destination == BROADCAST_ADDRESS);
    }

    SUBCASE("manufacturer-specific diagnostic (Proprietary B)") {
        // Custom diagnostic broadcast
        ProprietaryMsg diag;
        diag.pgn = PGN_PROPRIETARY_B_BASE + 0xD0; // Custom diagnostic group
        diag.source = 0x28;

        // Diagnostic payload
        diag.data = {0xFF, 0x00, 0x12, 0x34};

        CHECK(diag.is_proprietary_b());
        CHECK(diag.group_extension() == 0xD0);
    }

    SUBCASE("multiple Proprietary B group extensions") {
        // Manufacturer uses multiple group extensions
        ProprietaryMsg msg1;
        msg1.pgn = PGN_PROPRIETARY_B_BASE + 0x10;
        CHECK(msg1.group_extension() == 0x10);

        ProprietaryMsg msg2;
        msg2.pgn = PGN_PROPRIETARY_B_BASE + 0x20;
        CHECK(msg2.group_extension() == 0x20);

        ProprietaryMsg msg3;
        msg3.pgn = PGN_PROPRIETARY_B_BASE + 0x30;
        CHECK(msg3.group_extension() == 0x30);

        CHECK(msg1.is_proprietary_b());
        CHECK(msg2.is_proprietary_b());
        CHECK(msg3.is_proprietary_b());
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Edge Cases
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("Proprietary message edge cases") {
    SUBCASE("empty data payload") {
        ProprietaryMsg msg;
        msg.pgn = PGN_PROPRIETARY_B_BASE;
        msg.data.clear();

        CHECK(msg.data.size() == 0);
        CHECK(msg.is_proprietary_b());
    }

    SUBCASE("maximum group extension") {
        ProprietaryMsg msg;
        msg.pgn = PGN_PROPRIETARY_B_BASE + 0xFF;

        CHECK(msg.group_extension() == 0xFF);
        CHECK(msg.is_proprietary_b());
    }

    SUBCASE("boundary PGN values") {
        ProprietaryMsg msg;

        // First Proprietary B
        msg.pgn = 0xFF00;
        CHECK(msg.is_proprietary_b());
        CHECK_FALSE(msg.is_proprietary_a());

        // Last Proprietary B
        msg.pgn = 0xFFFF;
        CHECK(msg.is_proprietary_b());

        // Just below Proprietary B
        msg.pgn = 0xFEFF;
        CHECK_FALSE(msg.is_proprietary_b());
    }

    SUBCASE("null addresses") {
        ProprietaryMsg msg;
        msg.source = NULL_ADDRESS;
        msg.destination = NULL_ADDRESS;

        CHECK(msg.source == NULL_ADDRESS);
        CHECK(msg.destination == NULL_ADDRESS);
    }
}
