#include <doctest/doctest.h>
#include <agrobus/net/tp.hpp>
#include <agrobus/net/constants.hpp>

using namespace agrobus::net;

TEST_CASE("TP timer - session timeout detection (WaitForCTS)") {
    TransportProtocol tp;

    bool timeout_fired = false;
    TPTimerSession timed_out_session;
    tp.on_session_timeout.subscribe([&](TPTimerSession& s) {
        timeout_fired = true;
        timed_out_session = s;
    });

    tp.track_session(0x28, 0x30, 0xFECA, TPSessionState::WaitForCTS);

    // Not yet timed out
    tp.update_sessions(TP_TIMEOUT_T3_MS - 1);
    CHECK_FALSE(timeout_fired);

    // Now exceed the threshold
    tp.update_sessions(2);
    CHECK(timeout_fired);
    CHECK(timed_out_session.timer_state == TPSessionState::TimedOut);
    CHECK(timed_out_session.abort_reason == tp_abort::TIMEOUT);
}

TEST_CASE("TP timer - session timeout detection (WaitForEndOfMsgAck)") {
    TransportProtocol tp;

    bool timeout_fired = false;
    tp.on_session_timeout.subscribe([&](TPTimerSession&) {
        timeout_fired = true;
    });

    tp.track_session(0x28, 0x30, 0xFECA, TPSessionState::WaitForEndOfMsgAck);
    tp.update_sessions(TP_TIMEOUT_T3_MS + 1);
    CHECK(timeout_fired);
}

TEST_CASE("TP timer - session timeout detection (Sending state uses T4)") {
    TransportProtocol tp;

    bool timeout_fired = false;
    tp.on_session_timeout.subscribe([&](TPTimerSession&) {
        timeout_fired = true;
    });

    tp.track_session(0x28, 0x30, 0xFECA, TPSessionState::Sending);

    // T4 is 1050ms - not yet timed out at 1049
    tp.update_sessions(TP_TIMEOUT_T4_MS - 1);
    CHECK_FALSE(timeout_fired);

    // Exceed T4
    tp.update_sessions(2);
    CHECK(timeout_fired);
}

TEST_CASE("TP timer - timeout generates abort frame for unicast") {
    TransportProtocol tp;

    tp.track_session(0x28, 0x30, 0xFECA, TPSessionState::WaitForCTS);
    auto frames = tp.update_sessions(TP_TIMEOUT_T3_MS + 1);

    REQUIRE(!frames.empty());
    CHECK(frames[0].data[0] == tp_cm::ABORT);
    CHECK(frames[0].data[1] == tp_abort::TIMEOUT);
    // Check PGN encoding in bytes 5-7
    CHECK(frames[0].data[5] == static_cast<u8>(0xFECA & 0xFF));
    CHECK(frames[0].data[6] == static_cast<u8>((0xFECA >> 8) & 0xFF));
    CHECK(frames[0].data[7] == static_cast<u8>((0xFECA >> 16) & 0xFF));
}

TEST_CASE("TP timer - no abort frame for broadcast timeout") {
    TransportProtocol tp;

    tp.track_session(0x28, BROADCAST_ADDRESS, 0xFECA, TPSessionState::WaitForCTS);
    auto frames = tp.update_sessions(TP_TIMEOUT_T3_MS + 1);

    CHECK(frames.empty());
}

TEST_CASE("TP timer - CTS keepalive emission when receiver is paused") {
    TransportProtocol tp;

    tp.track_session(0x28, 0x30, 0xFECA, TPSessionState::WaitForCTS);
    tp.set_receiver_paused(0x28, 0x30, 0xFECA);

    // Not enough time for keepalive yet
    auto frames = tp.update_sessions(TP_T_HOLD_MS - 1);
    CHECK(frames.empty());

    // Exceed Th - should send CTS keepalive
    frames = tp.update_sessions(2);
    // Session should not have timed out yet (only ~501ms elapsed, T3 is 1250ms)
    REQUIRE(!frames.empty());
    CHECK(frames[0].data[0] == tp_cm::CTS);
    CHECK(frames[0].data[1] == 0); // num_packets=0 (hold)
}

TEST_CASE("TP timer - CTS keepalive repeats periodically") {
    TransportProtocol tp;

    tp.track_session(0x28, 0x30, 0xFECA, TPSessionState::WaitForCTS);
    tp.set_receiver_paused(0x28, 0x30, 0xFECA);

    // First keepalive at 500ms
    auto frames1 = tp.update_sessions(TP_T_HOLD_MS);
    REQUIRE(!frames1.empty());
    CHECK(frames1[0].data[0] == tp_cm::CTS);

    // Second keepalive at 1000ms (need another 500ms)
    auto frames2 = tp.update_sessions(TP_T_HOLD_MS);
    REQUIRE(!frames2.empty());
    CHECK(frames2[0].data[0] == tp_cm::CTS);
}

TEST_CASE("TP timer - abort reason codes have correct values") {
    CHECK(tp_abort::ALREADY_IN_PROGRESS == 1);
    CHECK(tp_abort::NO_RESOURCES == 2);
    CHECK(tp_abort::TIMEOUT == 3);
    CHECK(tp_abort::CTS_WHILE_SENDING == 4);
    CHECK(tp_abort::MAX_RETRANSMITS == 5);
    CHECK(tp_abort::UNEXPECTED_DT == 6);
    CHECK(tp_abort::BAD_SEQUENCE == 7);
    CHECK(tp_abort::DUPLICATE_SEQUENCE == 8);
    CHECK(tp_abort::TOTAL_SIZE_TOO_BIG == 9);
}

TEST_CASE("TP timer - multiple sessions track independently") {
    TransportProtocol tp;

    u32 timeout_count = 0;
    tp.on_session_timeout.subscribe([&](TPTimerSession&) {
        timeout_count++;
    });

    tp.track_session(0x28, 0x30, 0xFECA, TPSessionState::WaitForCTS);
    tp.track_session(0x29, 0x31, 0xFECB, TPSessionState::Sending);

    // After 1050ms: Sending session (T4=1050ms) should timeout, WaitForCTS (T3=1250ms) should not
    tp.update_sessions(TP_TIMEOUT_T4_MS + 1);
    CHECK(timeout_count == 1);

    // Verify WaitForCTS is still active
    auto& sessions = tp.timer_sessions();
    bool found_active = false;
    for (const auto& s : sessions) {
        if (s.pgn == 0xFECA && s.is_active()) {
            found_active = true;
        }
    }
    CHECK(found_active);

    // Now the first session should timeout too
    tp.update_sessions(200); // total for session 1: 1051+200 = 1251 > 1250
    CHECK(timeout_count == 2);
}

TEST_CASE("TP timer - complete sessions are not timed out") {
    TransportProtocol tp;

    bool timeout_fired = false;
    tp.on_session_timeout.subscribe([&](TPTimerSession&) {
        timeout_fired = true;
    });

    tp.track_session(0x28, 0x30, 0xFECA, TPSessionState::WaitForCTS);
    // Mark it as complete before it can time out
    tp.set_session_state(0x28, 0x30, 0xFECA, TPSessionState::Complete);

    tp.update_sessions(TP_TIMEOUT_T3_MS + 100);
    CHECK_FALSE(timeout_fired);
}

TEST_CASE("TP timer - aborted sessions are not timed out") {
    TransportProtocol tp;

    bool timeout_fired = false;
    tp.on_session_timeout.subscribe([&](TPTimerSession&) {
        timeout_fired = true;
    });

    tp.track_session(0x28, 0x30, 0xFECA, TPSessionState::WaitForCTS);
    // Mark it as aborted
    tp.set_session_state(0x28, 0x30, 0xFECA, TPSessionState::Aborted);

    tp.update_sessions(TP_TIMEOUT_T3_MS + 100);
    CHECK_FALSE(timeout_fired);
}

TEST_CASE("TP timer - idle sessions are not timed out") {
    TransportProtocol tp;

    bool timeout_fired = false;
    tp.on_session_timeout.subscribe([&](TPTimerSession&) {
        timeout_fired = true;
    });

    tp.track_session(0x28, 0x30, 0xFECA, TPSessionState::Idle);
    tp.update_sessions(5000);
    CHECK_FALSE(timeout_fired);
}

TEST_CASE("TP timer - reset_session_timer resets elapsed time") {
    TransportProtocol tp;

    bool timeout_fired = false;
    tp.on_session_timeout.subscribe([&](TPTimerSession&) {
        timeout_fired = true;
    });

    tp.track_session(0x28, 0x30, 0xFECA, TPSessionState::WaitForCTS);

    // Accumulate time but not enough to timeout
    tp.update_sessions(TP_TIMEOUT_T3_MS - 100);
    CHECK_FALSE(timeout_fired);

    // Reset the timer
    tp.reset_session_timer(0x28, 0x30, 0xFECA);

    // Should not timeout since timer was reset
    tp.update_sessions(TP_TIMEOUT_T3_MS - 100);
    CHECK_FALSE(timeout_fired);

    // Now it should timeout
    tp.update_sessions(200);
    CHECK(timeout_fired);
}

TEST_CASE("TP timer - TPSessionState enum values") {
    CHECK(static_cast<u8>(TPSessionState::Idle) == 0);
    CHECK(static_cast<u8>(TPSessionState::WaitForCTS) == 1);
    CHECK(static_cast<u8>(TPSessionState::Sending) == 2);
    CHECK(static_cast<u8>(TPSessionState::WaitForEndOfMsgAck) == 3);
    CHECK(static_cast<u8>(TPSessionState::Complete) == 4);
    CHECK(static_cast<u8>(TPSessionState::Aborted) == 5);
    CHECK(static_cast<u8>(TPSessionState::TimedOut) == 6);
}

TEST_CASE("TP timer - TPTimerSession is_active checks") {
    TPTimerSession session;

    session.timer_state = TPSessionState::Idle;
    CHECK_FALSE(session.is_active());

    session.timer_state = TPSessionState::Complete;
    CHECK_FALSE(session.is_active());

    session.timer_state = TPSessionState::Aborted;
    CHECK_FALSE(session.is_active());

    session.timer_state = TPSessionState::TimedOut;
    CHECK_FALSE(session.is_active());

    session.timer_state = TPSessionState::WaitForCTS;
    CHECK(session.is_active());

    session.timer_state = TPSessionState::Sending;
    CHECK(session.is_active());

    session.timer_state = TPSessionState::WaitForEndOfMsgAck;
    CHECK(session.is_active());
}

TEST_CASE("TP timer - T_HOLD constant value") {
    CHECK(TP_T_HOLD_MS == 500);
}
