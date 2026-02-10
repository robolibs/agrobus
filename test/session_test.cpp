#include <doctest/doctest.h>
#include <agrobus/net/session.hpp>

using namespace agrobus::net;

// ═════════════════════════════════════════════════════════════════════════════
// Enumeration Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("TransportDirection enumeration") {
    SUBCASE("values exist") {
        TransportDirection tx = TransportDirection::Transmit;
        TransportDirection rx = TransportDirection::Receive;

        CHECK(tx != rx);
    }
}

TEST_CASE("TransportAbortReason enumeration") {
    SUBCASE("abort reason values") {
        CHECK(static_cast<u8>(TransportAbortReason::None) == 0);
        CHECK(static_cast<u8>(TransportAbortReason::Timeout) == 1);
        CHECK(static_cast<u8>(TransportAbortReason::AlreadyInSession) == 2);
        CHECK(static_cast<u8>(TransportAbortReason::ResourcesUnavailable) == 3);
        CHECK(static_cast<u8>(TransportAbortReason::BadSequence) == 4);
        CHECK(static_cast<u8>(TransportAbortReason::UnexpectedDataSize) == 5);
        CHECK(static_cast<u8>(TransportAbortReason::DuplicateSequence) == 6);
        CHECK(static_cast<u8>(TransportAbortReason::MaxRetransmitsExceeded) == 7);
        CHECK(static_cast<u8>(TransportAbortReason::UnexpectedPGN) == 8);
        CHECK(static_cast<u8>(TransportAbortReason::ConnectionModeError) == 9);
    }
}

TEST_CASE("SessionState enumeration") {
    SUBCASE("state values") {
        // Just verify they exist and are distinct
        CHECK(SessionState::None != SessionState::WaitingForCTS);
        CHECK(SessionState::WaitingForCTS != SessionState::SendingData);
        CHECK(SessionState::SendingData != SessionState::WaitingForEndOfMsg);
        CHECK(SessionState::WaitingForEndOfMsg != SessionState::ReceivingData);
        CHECK(SessionState::ReceivingData != SessionState::WaitingForData);
        CHECK(SessionState::WaitingForData != SessionState::Complete);
        CHECK(SessionState::Complete != SessionState::Aborted);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// TransportSession Initialization Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("TransportSession default initialization") {
    TransportSession session;

    CHECK(session.direction == TransportDirection::Receive);
    CHECK(session.state == SessionState::None);
    CHECK(session.pgn == 0);
    CHECK(session.data.size() == 0);
    CHECK(session.total_bytes == 0);
    CHECK(session.bytes_transferred == 0);
    CHECK(session.source_address == NULL_ADDRESS);
    CHECK(session.destination_address == BROADCAST_ADDRESS);
    CHECK(session.can_port == 0);
    CHECK(session.priority == Priority::Default);
    CHECK(session.last_sequence == 0);
    CHECK(session.packets_to_send == 0);
    CHECK(session.next_packet_to_send == 0);
    CHECK(session.cts_window_start == 1);
    CHECK(session.cts_window_size == 0);
    CHECK(session.dpo_packet_offset == 0);
    CHECK(session.timer_ms == 0);
}

// ═════════════════════════════════════════════════════════════════════════════
// TransportSession Helper Method Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("TransportSession progress()") {
    TransportSession session;

    SUBCASE("zero bytes total") {
        session.total_bytes = 0;
        session.bytes_transferred = 0;
        CHECK(session.progress() == 0.0f);
    }

    SUBCASE("partial progress") {
        session.total_bytes = 100;
        session.bytes_transferred = 25;
        CHECK(session.progress() == doctest::Approx(0.25f));

        session.bytes_transferred = 50;
        CHECK(session.progress() == doctest::Approx(0.50f));

        session.bytes_transferred = 75;
        CHECK(session.progress() == doctest::Approx(0.75f));
    }

    SUBCASE("complete progress") {
        session.total_bytes = 100;
        session.bytes_transferred = 100;
        CHECK(session.progress() == doctest::Approx(1.0f));
    }

    SUBCASE("various data sizes") {
        session.total_bytes = 1785;
        session.bytes_transferred = 0;
        CHECK(session.progress() == doctest::Approx(0.0f));

        session.bytes_transferred = 500;
        CHECK(session.progress() == doctest::Approx(500.0f / 1785.0f));

        session.bytes_transferred = 1785;
        CHECK(session.progress() == doctest::Approx(1.0f));
    }
}

TEST_CASE("TransportSession total_packets()") {
    TransportSession session;

    SUBCASE("zero bytes") {
        session.total_bytes = 0;
        CHECK(session.total_packets() == 0);
    }

    SUBCASE("exactly one packet") {
        session.total_bytes = 7; // TP_BYTES_PER_FRAME = 7
        CHECK(session.total_packets() == 1);
    }

    SUBCASE("multiple full packets") {
        session.total_bytes = 14; // 2 * 7
        CHECK(session.total_packets() == 2);

        session.total_bytes = 21; // 3 * 7
        CHECK(session.total_packets() == 3);
    }

    SUBCASE("partial last packet") {
        session.total_bytes = 8; // 1 full + 1 partial
        CHECK(session.total_packets() == 2);

        session.total_bytes = 15; // 2 full + 1 partial
        CHECK(session.total_packets() == 3);

        session.total_bytes = 100; // 14 full + 1 partial
        CHECK(session.total_packets() == 15);
    }

    SUBCASE("large data") {
        session.total_bytes = 1785; // J1939 max TP size
        CHECK(session.total_packets() == 255); // (1785 + 6) / 7

        session.total_bytes = 117440512; // ETP max size (16 MB)
        CHECK(session.total_packets() == 16777216); // 2^24 packets
    }
}

TEST_CASE("TransportSession is_broadcast()") {
    TransportSession session;

    SUBCASE("broadcast address") {
        session.destination_address = BROADCAST_ADDRESS;
        CHECK(session.is_broadcast());
    }

    SUBCASE("specific address") {
        session.destination_address = 0x42;
        CHECK_FALSE(session.is_broadcast());

        session.destination_address = 0x00;
        CHECK_FALSE(session.is_broadcast());

        session.destination_address = 0xFE;
        CHECK_FALSE(session.is_broadcast());
    }
}

TEST_CASE("TransportSession is_complete()") {
    TransportSession session;

    SUBCASE("not complete by default") {
        CHECK_FALSE(session.is_complete());
    }

    SUBCASE("complete state") {
        session.state = SessionState::Complete;
        CHECK(session.is_complete());
    }

    SUBCASE("other states not complete") {
        session.state = SessionState::None;
        CHECK_FALSE(session.is_complete());

        session.state = SessionState::WaitingForCTS;
        CHECK_FALSE(session.is_complete());

        session.state = SessionState::SendingData;
        CHECK_FALSE(session.is_complete());

        session.state = SessionState::ReceivingData;
        CHECK_FALSE(session.is_complete());

        session.state = SessionState::Aborted;
        CHECK_FALSE(session.is_complete());
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// TransportSession Field Assignment Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("TransportSession field assignment") {
    TransportSession session;

    SUBCASE("direction") {
        session.direction = TransportDirection::Transmit;
        CHECK(session.direction == TransportDirection::Transmit);

        session.direction = TransportDirection::Receive;
        CHECK(session.direction == TransportDirection::Receive);
    }

    SUBCASE("state") {
        session.state = SessionState::WaitingForCTS;
        CHECK(session.state == SessionState::WaitingForCTS);

        session.state = SessionState::Complete;
        CHECK(session.state == SessionState::Complete);
    }

    SUBCASE("addresses") {
        session.source_address = 0x42;
        CHECK(session.source_address == 0x42);

        session.destination_address = 0x50;
        CHECK(session.destination_address == 0x50);
    }

    SUBCASE("pgn and data") {
        session.pgn = 0xEF00;
        CHECK(session.pgn == 0xEF00);

        session.data.push_back(0xAA);
        session.data.push_back(0xBB);
        CHECK(session.data.size() == 2);
        CHECK(session.data[0] == 0xAA);
        CHECK(session.data[1] == 0xBB);
    }

    SUBCASE("sequence tracking") {
        session.last_sequence = 5;
        session.packets_to_send = 10;
        session.next_packet_to_send = 3;

        CHECK(session.last_sequence == 5);
        CHECK(session.packets_to_send == 10);
        CHECK(session.next_packet_to_send == 3);
    }

    SUBCASE("CTS windowing") {
        session.cts_window_start = 10;
        session.cts_window_size = 5;

        CHECK(session.cts_window_start == 10);
        CHECK(session.cts_window_size == 5);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Realistic Usage Scenarios
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("TransportSession realistic scenarios") {
    SUBCASE("small TP.BAM transmission") {
        TransportSession session;
        session.direction = TransportDirection::Transmit;
        session.destination_address = BROADCAST_ADDRESS;
        session.pgn = 0xEF00;
        session.total_bytes = 20; // 3 packets

        CHECK(session.is_broadcast());
        CHECK(session.total_packets() == 3);
        CHECK(session.progress() == 0.0f);

        // Simulate sending data
        session.bytes_transferred = 7;
        CHECK(session.progress() == doctest::Approx(0.35f));

        session.bytes_transferred = 14;
        CHECK(session.progress() == doctest::Approx(0.70f));

        session.bytes_transferred = 20;
        CHECK(session.progress() == doctest::Approx(1.0f));

        session.state = SessionState::Complete;
        CHECK(session.is_complete());
    }

    SUBCASE("large TP.CMDT reception") {
        TransportSession session;
        session.direction = TransportDirection::Receive;
        session.source_address = 0x25;
        session.destination_address = 0x42;
        session.pgn = 0xEC00;
        session.total_bytes = 1785; // Max TP size

        CHECK_FALSE(session.is_broadcast());
        CHECK(session.total_packets() == 255);

        // Simulate receiving windowed data
        session.state = SessionState::WaitingForData;
        session.cts_window_start = 1;
        session.cts_window_size = 16;

        // Receive first window
        session.bytes_transferred = 16 * 7; // 112 bytes
        CHECK(session.progress() == doctest::Approx(112.0f / 1785.0f));

        // Receive all data
        session.bytes_transferred = 1785;
        CHECK(session.progress() == doctest::Approx(1.0f));

        session.state = SessionState::Complete;
        CHECK(session.is_complete());
    }

    SUBCASE("ETP transmission") {
        TransportSession session;
        session.direction = TransportDirection::Transmit;
        session.source_address = 0x80;
        session.destination_address = 0x42;
        session.pgn = 0xC000;
        session.total_bytes = 100000; // Large ETP transfer

        CHECK_FALSE(session.is_broadcast());
        u32 expected_packets = (100000 + 6) / 7;
        CHECK(session.total_packets() == expected_packets);

        // DPO windowing
        session.dpo_packet_offset = 0;
        session.packets_to_send = 255;

        session.bytes_transferred = 255 * 7; // First window
        CHECK(session.progress() == doctest::Approx((255.0f * 7.0f) / 100000.0f));
    }

    SUBCASE("aborted session") {
        TransportSession session;
        session.direction = TransportDirection::Receive;
        session.total_bytes = 100;
        session.bytes_transferred = 50;

        CHECK(session.progress() == doctest::Approx(0.5f));

        session.state = SessionState::Aborted;
        CHECK_FALSE(session.is_complete());
        CHECK(session.state == SessionState::Aborted);
    }
}
