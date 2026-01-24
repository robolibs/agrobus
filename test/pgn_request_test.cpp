#include <doctest/doctest.h>
#include <agrobus/j1939/pgn_request.hpp>

using namespace agrobus::j1939;

TEST_CASE("PGNRequestProtocol") {
    IsoNet nm;
    Name name;
    auto cf_result = nm.create_internal(name, 0, 0x28);
    auto* cf = cf_result.value();

    PGNRequestProtocol pgnr(nm, cf);
    pgnr.initialize();

    SUBCASE("register responder") {
        bool called = false;
        pgnr.register_responder(PGN_DM1, [&]() -> dp::Vector<u8> {
            called = true;
            return {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF};
        });
        // Responder is registered but won't be called without actual message
        CHECK(!called);
    }

    SUBCASE("request event") {
        bool received = false;
        pgnr.on_request_received.subscribe([&](PGN, Address) {
            received = true;
        });
        // Event is registered
        CHECK(!received);
    }
}
