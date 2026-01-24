#include <doctest/doctest.h>
#include <agrobus/net/pgn.hpp>

using namespace agrobus::net;

TEST_CASE("PGN lookup") {
    SUBCASE("known PGN") {
        auto info = pgn_lookup(PGN_DM1);
        CHECK(info.has_value());
        CHECK(info->pgn == PGN_DM1);
        CHECK(info->is_broadcast);
    }

    SUBCASE("unknown PGN") {
        auto info = pgn_lookup(0x1FFFF);
        CHECK(!info.has_value());
    }

    SUBCASE("request PGN is not broadcast") {
        auto info = pgn_lookup(PGN_REQUEST);
        CHECK(info.has_value());
        CHECK(!info->is_broadcast);
    }
}

TEST_CASE("PGN validation") {
    CHECK(pgn_is_valid(0x00000));
    CHECK(pgn_is_valid(PGN_DM1));
    CHECK(pgn_is_valid(0x3FFFF));
    CHECK(!pgn_is_valid(0x40000));
}

TEST_CASE("PGN broadcast detection") {
    // PDU format >= 240 = broadcast
    CHECK(pgn_is_broadcast(PGN_DM1));        // 0xFECA, PF=0xFE >= 240
    CHECK(pgn_is_broadcast(PGN_VEHICLE_SPEED)); // 0xFEF1, PF=0xFE
    CHECK(!pgn_is_broadcast(PGN_REQUEST));    // 0xEA00, PF=0xEA < 240
    CHECK(!pgn_is_broadcast(PGN_TP_CM));      // 0xEC00, PF=0xEC < 240
}

TEST_CASE("PGN PDU format extraction") {
    CHECK(pgn_pdu_format(PGN_DM1) == 0xFE);
    CHECK(pgn_pdu_format(PGN_REQUEST) == 0xEA);
    CHECK(pgn_pdu_format(PGN_TP_CM) == 0xEC);
}
