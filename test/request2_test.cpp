#include <doctest/doctest.h>
#include <agrobus/j1939/request2.hpp>

using namespace agrobus::j1939;
using namespace agrobus::net;

// ═════════════════════════════════════════════════════════════════════════════
// Request2Msg Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("Request2Msg structure") {
    SUBCASE("default initialization") {
        Request2Msg msg;

        CHECK(msg.requested_pgn == 0);
        CHECK(msg.extended_id.size() == 0);
        CHECK_FALSE(msg.use_transfer);
    }

    SUBCASE("field assignment") {
        Request2Msg msg;
        msg.requested_pgn = 0xFE6E; // NMEA GPS Position
        msg.extended_id = {0x01, 0x02};
        msg.use_transfer = true;

        CHECK(msg.requested_pgn == 0xFE6E);
        CHECK(msg.extended_id.size() == 2);
        CHECK(msg.use_transfer);
    }
}

TEST_CASE("Request2Msg encode/decode") {
    SUBCASE("basic request without extended ID") {
        Request2Msg req;
        req.requested_pgn = 0xFEF5; // Ambient Conditions
        req.use_transfer = false;

        auto encoded = req.encode();
        CHECK(encoded.size() == 8);

        auto decoded = Request2Msg::decode(encoded);
        CHECK(decoded.requested_pgn == 0xFEF5);
        CHECK_FALSE(decoded.use_transfer);
        CHECK(decoded.extended_id.size() == 0);
    }

    SUBCASE("request with extended ID") {
        Request2Msg req;
        req.requested_pgn = 0xEF00; // Proprietary A
        req.extended_id = {0x42, 0xAA};
        req.use_transfer = false;

        auto encoded = req.encode();
        auto decoded = Request2Msg::decode(encoded);

        CHECK(decoded.requested_pgn == 0xEF00);
        CHECK(decoded.extended_id.size() == 2);
        CHECK(decoded.extended_id[0] == 0x42);
        CHECK(decoded.extended_id[1] == 0xAA);
        CHECK_FALSE(decoded.use_transfer);
    }

    SUBCASE("request with transfer flag") {
        Request2Msg req;
        req.requested_pgn = 0xFE6E;
        req.use_transfer = true;

        auto encoded = req.encode();
        auto decoded = Request2Msg::decode(encoded);

        CHECK(decoded.requested_pgn == 0xFE6E);
        CHECK(decoded.use_transfer);
    }

    SUBCASE("request with maximum extended ID (3 bytes)") {
        Request2Msg req;
        req.requested_pgn = 0x12345;
        req.extended_id = {0x11, 0x22, 0x33};

        auto encoded = req.encode();
        auto decoded = Request2Msg::decode(encoded);

        CHECK(decoded.requested_pgn == 0x12345);
        CHECK(decoded.extended_id.size() == 3);
        CHECK(decoded.extended_id[0] == 0x11);
        CHECK(decoded.extended_id[1] == 0x22);
        CHECK(decoded.extended_id[2] == 0x33);
    }

    SUBCASE("extended ID truncated beyond 3 bytes") {
        Request2Msg req;
        req.requested_pgn = 0x10000;
        req.extended_id = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE}; // 5 bytes

        auto encoded = req.encode();
        // Only first 3 bytes should be encoded
        CHECK(encoded[4] == 0xAA);
        CHECK(encoded[5] == 0xBB);
        CHECK(encoded[6] == 0xCC);
        CHECK(encoded[7] == 0xFF); // DD and EE not encoded
    }
}

TEST_CASE("Request2Msg PGN encoding") {
    SUBCASE("24-bit PGN encoding") {
        Request2Msg req;
        req.requested_pgn = 0x123456;

        auto encoded = req.encode();
        CHECK(encoded[0] == 0x56); // LSB
        CHECK(encoded[1] == 0x34);
        CHECK(encoded[2] == 0x12); // MSB
    }

    SUBCASE("common PGNs") {
        Request2Msg req;

        // Engine Temperature
        req.requested_pgn = 0xFEEE;
        auto enc1 = req.encode();
        auto dec1 = Request2Msg::decode(enc1);
        CHECK(dec1.requested_pgn == 0xFEEE);

        // Vehicle Position
        req.requested_pgn = 0xFEF3;
        auto enc2 = req.encode();
        auto dec2 = Request2Msg::decode(enc2);
        CHECK(dec2.requested_pgn == 0xFEF3);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// TransferMsg Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("TransferMsg structure") {
    SUBCASE("default initialization") {
        TransferMsg msg;

        CHECK(msg.original_pgn == 0);
        CHECK(msg.data.size() == 0);
    }

    SUBCASE("field assignment") {
        TransferMsg msg;
        msg.original_pgn = 0xFEF5;
        msg.data = {0x01, 0x02, 0x03, 0x04};

        CHECK(msg.original_pgn == 0xFEF5);
        CHECK(msg.data.size() == 4);
    }
}

TEST_CASE("TransferMsg encode/decode") {
    SUBCASE("basic transfer") {
        TransferMsg transfer;
        transfer.original_pgn = 0xFE6E;
        transfer.data = {0xAA, 0xBB, 0xCC};

        auto encoded = transfer.encode();
        CHECK(encoded.size() == 6); // 3 bytes PGN + 3 bytes data

        auto decoded = TransferMsg::decode(encoded);
        CHECK(decoded.original_pgn == 0xFE6E);
        CHECK(decoded.data.size() == 3);
        CHECK(decoded.data[0] == 0xAA);
        CHECK(decoded.data[1] == 0xBB);
        CHECK(decoded.data[2] == 0xCC);
    }

    SUBCASE("transfer with empty data") {
        TransferMsg transfer;
        transfer.original_pgn = 0x12345;

        auto encoded = transfer.encode();
        CHECK(encoded.size() == 3); // Only PGN

        auto decoded = TransferMsg::decode(encoded);
        CHECK(decoded.original_pgn == 0x12345);
        CHECK(decoded.data.size() == 0);
    }

    SUBCASE("transfer with large data") {
        TransferMsg transfer;
        transfer.original_pgn = 0xEF00;

        // Large payload
        for (int i = 0; i < 100; ++i) {
            transfer.data.push_back(static_cast<u8>(i));
        }

        auto encoded = transfer.encode();
        CHECK(encoded.size() == 103); // 3 bytes PGN + 100 bytes data

        auto decoded = TransferMsg::decode(encoded);
        CHECK(decoded.original_pgn == 0xEF00);
        CHECK(decoded.data.size() == 100);
        for (int i = 0; i < 100; ++i) {
            CHECK(decoded.data[i] == static_cast<u8>(i));
        }
    }

    SUBCASE("PGN encoding in transfer") {
        TransferMsg transfer;
        transfer.original_pgn = 0xABCDEF;
        transfer.data = {0x00};

        auto encoded = transfer.encode();
        CHECK(encoded[0] == 0xEF); // LSB
        CHECK(encoded[1] == 0xCD);
        CHECK(encoded[2] == 0xAB); // MSB (only 24 bits)
        CHECK(encoded[3] == 0x00); // Data starts here
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Round-Trip Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("Request2/Transfer round-trip") {
    SUBCASE("Request2 round-trip") {
        Request2Msg original;
        original.requested_pgn = 0x123456;
        original.extended_id = {0xAA, 0xBB};
        original.use_transfer = true;

        auto encoded = original.encode();
        auto decoded = Request2Msg::decode(encoded);

        CHECK(decoded.requested_pgn == original.requested_pgn);
        CHECK(decoded.extended_id.size() == original.extended_id.size());
        for (usize i = 0; i < original.extended_id.size(); ++i) {
            CHECK(decoded.extended_id[i] == original.extended_id[i]);
        }
        CHECK(decoded.use_transfer == original.use_transfer);
    }

    SUBCASE("Transfer round-trip") {
        TransferMsg original;
        original.original_pgn = 0xFEDCBA;
        original.data = {0x11, 0x22, 0x33, 0x44, 0x55};

        auto encoded = original.encode();
        auto decoded = TransferMsg::decode(encoded);

        CHECK(decoded.original_pgn == original.original_pgn);
        REQUIRE(decoded.data.size() == original.data.size());
        for (usize i = 0; i < original.data.size(); ++i) {
            CHECK(decoded.data[i] == original.data[i]);
        }
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Extended ID Handling Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("Extended ID handling") {
    SUBCASE("0xFF padding ignored during decode") {
        dp::Vector<u8> data(8, 0xFF);
        // PGN = 0x123456
        data[0] = 0x56;
        data[1] = 0x34;
        data[2] = 0x12;
        data[3] = 0x00; // use_transfer = false
        // Extended ID fields are 0xFF (padding)

        auto decoded = Request2Msg::decode(data);
        CHECK(decoded.requested_pgn == 0x123456);
        CHECK(decoded.extended_id.size() == 0); // 0xFF bytes not added
    }

    SUBCASE("mixed valid and 0xFF in extended ID") {
        dp::Vector<u8> data(8, 0xFF);
        data[0] = 0x00;
        data[1] = 0x00;
        data[2] = 0x10;
        data[3] = 0x00;
        data[4] = 0xAA; // Valid
        data[5] = 0xFF; // Padding (skipped)
        data[6] = 0xBB; // Valid (collected)

        auto decoded = Request2Msg::decode(data);
        CHECK(decoded.extended_id.size() == 2); // 0xAA and 0xBB (0xFF skipped)
        CHECK(decoded.extended_id[0] == 0xAA);
        CHECK(decoded.extended_id[1] == 0xBB);
    }

    SUBCASE("single byte extended ID") {
        Request2Msg req;
        req.requested_pgn = 0x1000;
        req.extended_id = {0x42};

        auto encoded = req.encode();
        auto decoded = Request2Msg::decode(encoded);

        CHECK(decoded.extended_id.size() == 1);
        CHECK(decoded.extended_id[0] == 0x42);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Realistic Scenarios
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("Request2 protocol scenarios") {
    SUBCASE("simple PGN request (like standard Request)") {
        // Request2 can be used like standard Request (without extended ID)
        Request2Msg req;
        req.requested_pgn = 0xFEF5; // Ambient Conditions
        req.use_transfer = false;

        auto encoded = req.encode();
        auto decoded = Request2Msg::decode(encoded);

        CHECK(decoded.requested_pgn == 0xFEF5);
        CHECK(decoded.extended_id.size() == 0);
        CHECK_FALSE(decoded.use_transfer);
    }

    SUBCASE("request with instance identifier") {
        // Request specific instance using extended ID
        Request2Msg req;
        req.requested_pgn = 0xFE6E; // GPS Position
        req.extended_id = {0x02}; // Instance 2
        req.use_transfer = false;

        auto encoded = req.encode();
        auto decoded = Request2Msg::decode(encoded);

        CHECK(decoded.requested_pgn == 0xFE6E);
        CHECK(decoded.extended_id.size() == 1);
        CHECK(decoded.extended_id[0] == 0x02);
    }

    SUBCASE("request with transfer response") {
        // Large data response should use Transfer PGN
        Request2Msg req;
        req.requested_pgn = 0xEF00; // Proprietary A
        req.extended_id = {0x10, 0x20};
        req.use_transfer = true;

        auto encoded = req.encode();
        auto decoded = Request2Msg::decode(encoded);

        CHECK(decoded.use_transfer);

        // Responder would send via Transfer PGN
        TransferMsg resp;
        resp.original_pgn = decoded.requested_pgn;
        resp.data = {0xAA, 0xBB, 0xCC, 0xDD};

        auto transfer_encoded = resp.encode();
        auto transfer_decoded = TransferMsg::decode(transfer_encoded);

        CHECK(transfer_decoded.original_pgn == req.requested_pgn);
        CHECK(transfer_decoded.data.size() == 4);
    }

    SUBCASE("multi-instance device query") {
        // Query multiple instances of same device type
        for (u8 instance = 0; instance < 4; ++instance) {
            Request2Msg req;
            req.requested_pgn = 0xFDA4; // Hydraulic Pressure
            req.extended_id = {instance};

            auto encoded = req.encode();
            auto decoded = Request2Msg::decode(encoded);

            CHECK(decoded.extended_id[0] == instance);
        }
    }

    SUBCASE("proprietary data with extended identifier") {
        // Manufacturer-specific request with complex extended ID
        Request2Msg req;
        req.requested_pgn = 0xEF00; // Proprietary A
        req.extended_id = {0x01, 0x42, 0xAA}; // Custom identifier
        req.use_transfer = true;

        auto encoded = req.encode();
        auto decoded = Request2Msg::decode(encoded);

        CHECK(decoded.requested_pgn == 0xEF00);
        CHECK(decoded.extended_id.size() == 3);
        CHECK(decoded.use_transfer);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Edge Cases
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("Request2/Transfer edge cases") {
    SUBCASE("minimum valid Request2 data") {
        dp::Vector<u8> data = {0x00, 0x10, 0x00, 0x00}; // PGN 0x1000, no transfer
        auto decoded = Request2Msg::decode(data);

        CHECK(decoded.requested_pgn == 0x1000);
        CHECK_FALSE(decoded.use_transfer);
    }

    SUBCASE("empty decode data") {
        dp::Vector<u8> empty;
        auto req_decoded = Request2Msg::decode(empty);
        auto transfer_decoded = TransferMsg::decode(empty);

        CHECK(req_decoded.requested_pgn == 0);
        CHECK(transfer_decoded.original_pgn == 0);
    }

    SUBCASE("maximum PGN value") {
        Request2Msg req;
        req.requested_pgn = 0xFFFFFF; // Max 24-bit

        auto encoded = req.encode();
        auto decoded = Request2Msg::decode(encoded);

        CHECK(decoded.requested_pgn == 0xFFFFFF);
    }

    SUBCASE("transfer flag bit masking") {
        dp::Vector<u8> data(8, 0xFF);
        data[0] = 0x00;
        data[1] = 0x00;
        data[2] = 0x00;
        data[3] = 0xFF; // All bits set, but only bit 0 matters

        auto decoded = Request2Msg::decode(data);
        CHECK(decoded.use_transfer); // Bit 0 is set
    }
}
