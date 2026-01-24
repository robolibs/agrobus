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

TEST_CASE("FileServer - initialization") {
    IsoNet nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x10).value();

    FileServer server(nm, cf, FileServerConfig{}.path("/data"));
    server.initialize();

    CHECK(server.base_path() == "/data");
    CHECK(server.files().empty());
}

TEST_CASE("FileServer - file management") {
    IsoNet nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x10).value();

    FileServer server(nm, cf);

    SUBCASE("Add files") {
        server.add_file("task1.xml");
        server.add_file("prescription.bin");
        CHECK(server.files().size() == 2);
    }

    SUBCASE("Remove file") {
        server.add_file("task1.xml");
        server.add_file("task2.xml");
        server.remove_file("task1.xml");
        CHECK(server.files().size() == 1);
        CHECK(server.files()[0].name == "task2.xml");
    }

    SUBCASE("Set base path") {
        server.set_base_path("/new/path");
        CHECK(server.base_path() == "/new/path");
    }
}

TEST_CASE("FileServer - events") {
    IsoNet nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x10).value();

    FileServer server(nm, cf);

    bool read_requested = false;
    server.on_file_read_request.subscribe([&](dp::String filename, Address) {
        read_requested = true;
        CHECK(filename == "test.bin");
    });

    // Event not triggered without message
    CHECK_FALSE(read_requested);
}

TEST_CASE("FileClient - initialization") {
    IsoNet nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x10).value();

    FileClient client(nm, cf);
    client.initialize();

    CHECK(client.state() == FileClientState::Idle);
    CHECK(client.current_handle() == 0);
}

TEST_CASE("FileClient - state management") {
    IsoNet nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x10).value();

    FileClient client(nm, cf);
    client.initialize();

    SUBCASE("Events can be subscribed") {
        bool opened = false;
        client.on_file_opened.subscribe([&](u8 handle) {
            opened = true;
            CHECK(handle > 0);
        });
        CHECK_FALSE(opened);
    }
}
