#include <doctest/doctest.h>
#include <agrobus/j1939/dm_memory.hpp>

using namespace agrobus::j1939;
using namespace agrobus::net;

// ═════════════════════════════════════════════════════════════════════════════
// DM14 Request Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("DM14Command enumeration") {
    SUBCASE("command values") {
        CHECK(static_cast<u8>(DM14Command::Read) == 0);
        CHECK(static_cast<u8>(DM14Command::Write) == 1);
        CHECK(static_cast<u8>(DM14Command::StatusRequest) == 2);
        CHECK(static_cast<u8>(DM14Command::Erase) == 3);
        CHECK(static_cast<u8>(DM14Command::BootLoad) == 4);
        CHECK(static_cast<u8>(DM14Command::EdcpGeneration) == 5);
        CHECK(static_cast<u8>(DM14Command::Reserved) == 0xFF);
    }
}

TEST_CASE("DM14PointerType enumeration") {
    SUBCASE("pointer type values") {
        CHECK(static_cast<u8>(DM14PointerType::DirectPhysical) == 0);
        CHECK(static_cast<u8>(DM14PointerType::DirectVirtual) == 1);
        CHECK(static_cast<u8>(DM14PointerType::Indirect) == 2);
        CHECK(static_cast<u8>(DM14PointerType::NotAvailable) == 3);
    }
}

TEST_CASE("DM14Request encode/decode") {
    SUBCASE("basic read request") {
        DM14Request req;
        req.command = DM14Command::Read;
        req.pointer_type = DM14PointerType::DirectPhysical;
        req.address = 0x123456;
        req.length = 0x0100;
        req.key = 0xAA;

        auto encoded = req.encode();
        CHECK(encoded.size() == 8);

        auto decoded = DM14Request::decode(encoded);
        CHECK(decoded.command == DM14Command::Read);
        CHECK(decoded.pointer_type == DM14PointerType::DirectPhysical);
        CHECK(decoded.address == 0x123456);
        CHECK(decoded.length == 0x0100);
        CHECK(decoded.key == 0xAA);
    }

    SUBCASE("write request") {
        DM14Request req;
        req.command = DM14Command::Write;
        req.pointer_type = DM14PointerType::DirectVirtual;
        req.address = 0xABCDEF;
        req.length = 512;
        req.key = 0x55;

        auto encoded = req.encode();
        auto decoded = DM14Request::decode(encoded);

        CHECK(decoded.command == DM14Command::Write);
        CHECK(decoded.pointer_type == DM14PointerType::DirectVirtual);
        CHECK(decoded.address == 0xABCDEF);
        CHECK(decoded.length == 512);
        CHECK(decoded.key == 0x55);
    }

    SUBCASE("erase request") {
        DM14Request req;
        req.command = DM14Command::Erase;
        req.address = 0x100000;
        req.length = 0x1000;

        auto encoded = req.encode();
        auto decoded = DM14Request::decode(encoded);

        CHECK(decoded.command == DM14Command::Erase);
        CHECK(decoded.address == 0x100000);
        CHECK(decoded.length == 0x1000);
    }

    SUBCASE("bit field packing") {
        DM14Request req;
        req.command = DM14Command::Read;       // 0b000
        req.pointer_type = DM14PointerType::DirectVirtual; // 0b01

        auto encoded = req.encode();
        // Byte 0: command[2:0] | reserved[3] | pointer_type[5:4] | reserved[7:6]
        // Expected: 0b00010000 = 0x10
        CHECK((encoded[0] & 0x07) == 0x00); // Command bits
        CHECK(((encoded[0] >> 4) & 0x03) == 0x01); // Pointer type bits
    }

    SUBCASE("address encoding (24-bit)") {
        DM14Request req;
        req.address = 0xAABBCC;

        auto encoded = req.encode();
        CHECK(encoded[3] == 0xCC); // LSB
        CHECK(encoded[4] == 0xBB);
        CHECK(encoded[5] == 0xAA); // MSB
    }

    SUBCASE("length encoding (16-bit)") {
        DM14Request req;
        req.length = 0x1234;

        auto encoded = req.encode();
        CHECK(encoded[1] == 0x34); // LSB
        CHECK(encoded[2] == 0x12); // MSB
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// DM15 Response Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("DM15Status enumeration") {
    SUBCASE("status values") {
        CHECK(static_cast<u8>(DM15Status::Proceed) == 0);
        CHECK(static_cast<u8>(DM15Status::Busy) == 1);
        CHECK(static_cast<u8>(DM15Status::Completed) == 2);
        CHECK(static_cast<u8>(DM15Status::Error) == 3);
        CHECK(static_cast<u8>(DM15Status::EdcpFault) == 4);
        CHECK(static_cast<u8>(DM15Status::Reserved) == 0xFF);
    }
}

TEST_CASE("DM15Response encode/decode") {
    SUBCASE("proceed response") {
        DM15Response resp;
        resp.status = DM15Status::Proceed;
        resp.length = 256;
        resp.address = 0x100000;
        resp.edcp_extension = 0x00;
        resp.seed = 0x42;

        auto encoded = resp.encode();
        CHECK(encoded.size() == 8);

        auto decoded = DM15Response::decode(encoded);
        CHECK(decoded.status == DM15Status::Proceed);
        CHECK(decoded.length == 256);
        CHECK(decoded.address == 0x100000);
        CHECK(decoded.edcp_extension == 0x00);
        CHECK(decoded.seed == 0x42);
    }

    SUBCASE("busy response") {
        DM15Response resp;
        resp.status = DM15Status::Busy;
        resp.length = 0;
        resp.address = 0x123456;

        auto encoded = resp.encode();
        auto decoded = DM15Response::decode(encoded);

        CHECK(decoded.status == DM15Status::Busy);
    }

    SUBCASE("completed response") {
        DM15Response resp;
        resp.status = DM15Status::Completed;
        resp.length = 1024;
        resp.address = 0xFFFFFF;

        auto encoded = resp.encode();
        auto decoded = DM15Response::decode(encoded);

        CHECK(decoded.status == DM15Status::Completed);
        CHECK(decoded.length == 1024);
        CHECK(decoded.address == 0xFFFFFF);
    }

    SUBCASE("error response") {
        DM15Response resp;
        resp.status = DM15Status::Error;

        auto encoded = resp.encode();
        auto decoded = DM15Response::decode(encoded);

        CHECK(decoded.status == DM15Status::Error);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// DM16 Transfer Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("DM16Transfer encode/decode") {
    SUBCASE("small data transfer") {
        DM16Transfer transfer;
        transfer.num_bytes = 4;
        transfer.data = {0xAA, 0xBB, 0xCC, 0xDD};

        auto encoded = transfer.encode();
        CHECK(encoded.size() == 8);
        CHECK(encoded[0] == 4); // num_bytes

        auto decoded = DM16Transfer::decode(encoded);
        CHECK(decoded.num_bytes == 4);
        CHECK(decoded.data.size() == 4);
        CHECK(decoded.data[0] == 0xAA);
        CHECK(decoded.data[1] == 0xBB);
        CHECK(decoded.data[2] == 0xCC);
        CHECK(decoded.data[3] == 0xDD);
    }

    SUBCASE("full frame transfer") {
        DM16Transfer transfer;
        transfer.num_bytes = 7;
        transfer.data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};

        auto encoded = transfer.encode();
        auto decoded = DM16Transfer::decode(encoded);

        CHECK(decoded.num_bytes == 7);
        CHECK(decoded.data.size() == 7);
        for (u8 i = 0; i < 7; ++i) {
            CHECK(decoded.data[i] == i + 1);
        }
    }

    SUBCASE("single byte transfer") {
        DM16Transfer transfer;
        transfer.num_bytes = 1;
        transfer.data = {0xFF};

        auto encoded = transfer.encode();
        auto decoded = DM16Transfer::decode(encoded);

        CHECK(decoded.num_bytes == 1);
        CHECK(decoded.data.size() == 1);
        CHECK(decoded.data[0] == 0xFF);
    }

    SUBCASE("empty transfer") {
        DM16Transfer transfer;
        transfer.num_bytes = 0;

        auto encoded = transfer.encode();
        auto decoded = DM16Transfer::decode(encoded);

        CHECK(decoded.num_bytes == 0);
        CHECK(decoded.data.size() == 0);
    }

    SUBCASE("data truncation to 7 bytes") {
        DM16Transfer transfer;
        transfer.num_bytes = 10; // Claims 10 bytes
        transfer.data = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99};

        auto encoded = transfer.encode();
        // Only 7 bytes fit in single frame
        CHECK(encoded[1] == 0x00);
        CHECK(encoded[2] == 0x11);
        CHECK(encoded[7] == 0x66); // Last byte that fits
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// ECU Identification Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("ECUIdentification encode/decode") {
    SUBCASE("full identification") {
        ECUIdentification id;
        id.ecu_part_number = "PN12345";
        id.ecu_serial_number = "SN67890";
        id.ecu_location = "Engine Bay";
        id.ecu_type = "ECU-A";
        id.ecu_manufacturer = "ACME Corp";

        auto encoded = id.encode();
        auto decoded = ECUIdentification::decode(encoded);

        CHECK(decoded.ecu_part_number == "PN12345");
        CHECK(decoded.ecu_serial_number == "SN67890");
        CHECK(decoded.ecu_location == "Engine Bay");
        CHECK(decoded.ecu_type == "ECU-A");
        CHECK(decoded.ecu_manufacturer == "ACME Corp");
    }

    SUBCASE("empty fields") {
        ECUIdentification id;
        id.ecu_part_number = "";
        id.ecu_serial_number = "";
        id.ecu_location = "";
        id.ecu_type = "";
        id.ecu_manufacturer = "";

        auto encoded = id.encode();
        // Should have 5 asterisks
        int asterisk_count = 0;
        for (u8 b : encoded) {
            if (b == '*') asterisk_count++;
        }
        CHECK(asterisk_count == 5);

        auto decoded = ECUIdentification::decode(encoded);
        CHECK(decoded.ecu_part_number == "");
        CHECK(decoded.ecu_serial_number == "");
    }

    SUBCASE("asterisk delimiter") {
        ECUIdentification id;
        id.ecu_part_number = "ABC";
        id.ecu_serial_number = "123";

        auto encoded = id.encode();
        // Check for asterisk delimiters
        bool found_delimiter = false;
        for (usize i = 0; i < encoded.size(); ++i) {
            if (encoded[i] == '*') {
                found_delimiter = true;
                break;
            }
        }
        CHECK(found_delimiter);
    }

    SUBCASE("partial data decoding") {
        dp::Vector<u8> partial = {'A', 'B', 'C', '*', '1', '2', '3', '*'};
        auto decoded = ECUIdentification::decode(partial);

        CHECK(decoded.ecu_part_number == "ABC");
        CHECK(decoded.ecu_serial_number == "123");
        // Other fields should be empty
        CHECK(decoded.ecu_location == "");
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Round-Trip Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("DM14/DM15/DM16 round-trip") {
    SUBCASE("DM14 request round-trip") {
        DM14Request original;
        original.command = DM14Command::Write;
        original.pointer_type = DM14PointerType::Indirect;
        original.address = 0x555555;
        original.length = 0xABCD;
        original.key = 0x33;

        auto encoded = original.encode();
        auto decoded = DM14Request::decode(encoded);

        CHECK(decoded.command == original.command);
        CHECK(decoded.pointer_type == original.pointer_type);
        CHECK(decoded.address == original.address);
        CHECK(decoded.length == original.length);
        CHECK(decoded.key == original.key);
    }

    SUBCASE("DM15 response round-trip") {
        DM15Response original;
        original.status = DM15Status::EdcpFault;
        original.length = 0x7FFF;
        original.address = 0x111111;
        original.edcp_extension = 0x99;
        original.seed = 0x77;

        auto encoded = original.encode();
        auto decoded = DM15Response::decode(encoded);

        CHECK(decoded.status == original.status);
        CHECK(decoded.length == original.length);
        CHECK(decoded.address == original.address);
        CHECK(decoded.edcp_extension == original.edcp_extension);
        CHECK(decoded.seed == original.seed);
    }

    SUBCASE("DM16 transfer round-trip") {
        DM16Transfer original;
        original.num_bytes = 5;
        original.data = {0x10, 0x20, 0x30, 0x40, 0x50};

        auto encoded = original.encode();
        auto decoded = DM16Transfer::decode(encoded);

        CHECK(decoded.num_bytes == original.num_bytes);
        REQUIRE(decoded.data.size() == original.data.size());
        for (usize i = 0; i < original.data.size(); ++i) {
            CHECK(decoded.data[i] == original.data[i]);
        }
    }

    SUBCASE("ECU ID round-trip") {
        ECUIdentification original;
        original.ecu_part_number = "TEST123";
        original.ecu_serial_number = "XYZ789";
        original.ecu_location = "Cab";
        original.ecu_type = "Controller";
        original.ecu_manufacturer = "TestCo";

        auto encoded = original.encode();
        auto decoded = ECUIdentification::decode(encoded);

        CHECK(decoded.ecu_part_number == original.ecu_part_number);
        CHECK(decoded.ecu_serial_number == original.ecu_serial_number);
        CHECK(decoded.ecu_location == original.ecu_location);
        CHECK(decoded.ecu_type == original.ecu_type);
        CHECK(decoded.ecu_manufacturer == original.ecu_manufacturer);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Realistic Scenarios
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("Memory access protocol scenarios") {
    SUBCASE("read operation sequence") {
        // 1. Service tool sends DM14 read request
        DM14Request read_req;
        read_req.command = DM14Command::Read;
        read_req.pointer_type = DM14PointerType::DirectPhysical;
        read_req.address = 0x100000;
        read_req.length = 64;

        auto req_encoded = read_req.encode();
        auto req_decoded = DM14Request::decode(req_encoded);

        // 2. ECU sends DM15 proceed response
        DM15Response proceed_resp;
        proceed_resp.status = DM15Status::Proceed;
        proceed_resp.length = req_decoded.length;
        proceed_resp.address = req_decoded.address;

        auto resp_encoded = proceed_resp.encode();
        auto resp_decoded = DM15Response::decode(resp_encoded);

        CHECK(resp_decoded.status == DM15Status::Proceed);
        CHECK(resp_decoded.address == read_req.address);
    }

    SUBCASE("write operation with security") {
        // 1. Service tool requests write with key
        DM14Request write_req;
        write_req.command = DM14Command::Write;
        write_req.address = 0x200000;
        write_req.length = 128;
        write_req.key = 0xAB; // Security response

        // 2. ECU validates and responds
        DM15Response write_resp;
        write_resp.status = DM15Status::Proceed;
        write_resp.address = write_req.address;

        // 3. Data transfer via DM16
        DM16Transfer transfer;
        transfer.num_bytes = 7;
        for (u8 i = 0; i < 7; ++i) {
            transfer.data.push_back(i * 10);
        }

        auto transfer_decoded = DM16Transfer::decode(transfer.encode());
        CHECK(transfer_decoded.data.size() == 7);
    }

    SUBCASE("erase operation") {
        DM14Request erase_req;
        erase_req.command = DM14Command::Erase;
        erase_req.address = 0x300000;
        erase_req.length = 4096; // Erase 4KB sector

        auto encoded = erase_req.encode();
        auto decoded = DM14Request::decode(encoded);

        CHECK(decoded.command == DM14Command::Erase);
        CHECK(decoded.length == 4096);
    }

    SUBCASE("bootload mode activation") {
        DM14Request bootload_req;
        bootload_req.command = DM14Command::BootLoad;

        auto encoded = bootload_req.encode();
        auto decoded = DM14Request::decode(encoded);

        CHECK(decoded.command == DM14Command::BootLoad);
    }
}
