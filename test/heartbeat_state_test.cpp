#include <doctest/doctest.h>
#include <agrobus/j1939/heartbeat.hpp>

using namespace agrobus::j1939;

// ─── HeartbeatSender tests ───────────────────────────────────────────────────

TEST_CASE("HeartbeatSender: init sequence") {
    HeartbeatSender sender;

    SUBCASE("first call returns 251 (INIT)") {
        CHECK(sender.next_sequence() == hb_seq::INIT);
    }

    SUBCASE("second call returns 0") {
        sender.next_sequence(); // 251
        CHECK(sender.next_sequence() == 0);
    }

    SUBCASE("sequence progresses 0, 1, 2, ...") {
        sender.next_sequence(); // 251
        for (u8 i = 0; i < 10; ++i) {
            CHECK(sender.next_sequence() == i);
        }
    }
}

TEST_CASE("HeartbeatSender: rollover at 250") {
    HeartbeatSender sender;

    // Send init
    sender.next_sequence();

    // Advance to 250
    for (u16 i = 0; i <= 250; ++i) {
        u8 seq = sender.next_sequence();
        CHECK(seq == static_cast<u8>(i));
    }

    // Next after 250 should be 0 (rollover)
    CHECK(sender.next_sequence() == 0);

    // Continue from 0
    CHECK(sender.next_sequence() == 1);
    CHECK(sender.next_sequence() == 2);
}

TEST_CASE("HeartbeatSender: signal_error sends 254 exactly once then resumes") {
    HeartbeatSender sender;
    sender.next_sequence(); // init (251)
    sender.next_sequence(); // 0
    sender.next_sequence(); // 1

    sender.signal_error();
    CHECK(sender.sequence_ == hb_seq::SENDER_ERROR);
    CHECK(sender.special_pending_);

    // Next call returns 254 (the error signal)
    CHECK(sender.next_sequence() == hb_seq::SENDER_ERROR);
    CHECK(!sender.special_pending_);

    // After error, resumes at 0
    CHECK(sender.next_sequence() == 0);
    CHECK(sender.next_sequence() == 1);
}

TEST_CASE("HeartbeatSender: signal_shutdown sends 255 exactly once then resumes") {
    HeartbeatSender sender;
    sender.next_sequence(); // init (251)
    sender.next_sequence(); // 0
    sender.next_sequence(); // 1

    sender.signal_shutdown();
    CHECK(sender.sequence_ == hb_seq::SHUTDOWN);
    CHECK(sender.special_pending_);

    // Next call returns 255 (the shutdown signal)
    CHECK(sender.next_sequence() == hb_seq::SHUTDOWN);
    CHECK(!sender.special_pending_);

    // After shutdown, resumes at 0
    CHECK(sender.next_sequence() == 0);
    CHECK(sender.next_sequence() == 1);
}

TEST_CASE("HeartbeatSender: update timer") {
    HeartbeatSender sender;

    SUBCASE("not ready before interval") {
        CHECK(!sender.update(50));
        CHECK(!sender.update(49));
    }

    SUBCASE("ready at interval boundary") {
        CHECK(sender.update(100));
    }

    SUBCASE("ready after accumulation") {
        CHECK(!sender.update(50));
        CHECK(sender.update(50));
    }

    SUBCASE("timer resets after trigger") {
        CHECK(sender.update(100));
        CHECK(!sender.update(50));
        CHECK(sender.update(50));
    }
}

TEST_CASE("HeartbeatSender: reset") {
    HeartbeatSender sender;
    sender.next_sequence(); // init
    sender.next_sequence(); // 0
    sender.update(50);

    sender.reset();
    CHECK(sender.init_sent_ == false);
    CHECK(sender.timer_ms_ == 0);
    CHECK(sender.sequence_ == hb_seq::INIT);

    // After reset, first call should return INIT again
    CHECK(sender.next_sequence() == hb_seq::INIT);
}

// ─── HeartbeatReceiver tests ─────────────────────────────────────────────────

TEST_CASE("HeartbeatReceiver: initial state") {
    HeartbeatReceiver rx;
    CHECK(rx.state() == HBReceiverState::Normal);
    CHECK(rx.is_healthy());
    CHECK(!rx.first_received_);
}

TEST_CASE("HeartbeatReceiver: normal operation") {
    HeartbeatReceiver rx;

    // First heartbeat just records
    rx.process(0);
    CHECK(rx.state() == HBReceiverState::Normal);
    CHECK(rx.first_received_);
    CHECK(rx.last_sequence_ == 0);

    // Sequential heartbeats stay normal
    rx.process(1);
    CHECK(rx.state() == HBReceiverState::Normal);
    rx.process(2);
    CHECK(rx.state() == HBReceiverState::Normal);
    rx.process(3);
    CHECK(rx.state() == HBReceiverState::Normal);
}

TEST_CASE("HeartbeatReceiver: detects repeated sequence -> SequenceError") {
    HeartbeatReceiver rx;

    rx.process(5); // first
    rx.process(6); // normal
    CHECK(rx.state() == HBReceiverState::Normal);

    // Send same sequence again
    rx.process(6);
    CHECK(rx.state() == HBReceiverState::SequenceError);
}

TEST_CASE("HeartbeatReceiver: detects jump > 3 -> SequenceError") {
    HeartbeatReceiver rx;

    rx.process(10); // first
    rx.process(11); // normal (jump=1)
    CHECK(rx.state() == HBReceiverState::Normal);

    SUBCASE("jump of 4 causes error") {
        rx.process(15); // jump = 4 > 3
        CHECK(rx.state() == HBReceiverState::SequenceError);
    }

    SUBCASE("jump of 3 is ok") {
        rx.process(14); // jump = 3, exactly at limit
        CHECK(rx.state() == HBReceiverState::Normal);
    }

    SUBCASE("jump of 2 is ok") {
        rx.process(13); // jump = 2
        CHECK(rx.state() == HBReceiverState::Normal);
    }

    SUBCASE("jump of 1 is ok") {
        rx.process(12); // jump = 1
        CHECK(rx.state() == HBReceiverState::Normal);
    }
}

TEST_CASE("HeartbeatReceiver: rollover jump handling") {
    HeartbeatReceiver rx;

    SUBCASE("249 -> 0 is jump of 2 (ok)") {
        rx.process(249); // first
        rx.process(250); // normal
        rx.process(0);   // rollover: jump = (251 - 250 + 0) = 1
        CHECK(rx.state() == HBReceiverState::Normal);
    }

    SUBCASE("248 -> 0 is jump of 3 (ok)") {
        rx.process(248); // first
        rx.process(249); // normal
        rx.process(0);   // rollover: jump = (251 - 249 + 0) = 2
        CHECK(rx.state() == HBReceiverState::Normal);
    }

    SUBCASE("250 -> 0 is jump of 1 (ok)") {
        rx.process(249); // first
        rx.process(250); // normal
        rx.process(0);   // rollover: 251 - 250 + 0 = 1
        CHECK(rx.state() == HBReceiverState::Normal);
    }

    SUBCASE("247 -> 0 from 248 is jump of 3 (ok)") {
        rx.process(247); // first
        rx.process(248); // normal
        rx.process(0);   // rollover: 251 - 248 + 0 = 3
        CHECK(rx.state() == HBReceiverState::Normal);
    }

    SUBCASE("246 -> 0 from 247 is jump of 4 (error)") {
        rx.process(246); // first
        rx.process(247); // normal
        rx.process(0);   // rollover: 251 - 247 + 0 = 4 > 3
        CHECK(rx.state() == HBReceiverState::SequenceError);
    }
}

TEST_CASE("HeartbeatReceiver: ignores 252/253 (reserved)") {
    HeartbeatReceiver rx;

    rx.process(5); // first

    SUBCASE("252 is ignored") {
        rx.process(hb_seq::RESERVED_LOW);
        CHECK(rx.state() == HBReceiverState::Normal);
        CHECK(rx.last_sequence_ == 5); // unchanged
    }

    SUBCASE("253 is ignored") {
        rx.process(hb_seq::RESERVED_HIGH);
        CHECK(rx.state() == HBReceiverState::Normal);
        CHECK(rx.last_sequence_ == 5); // unchanged
    }
}

TEST_CASE("HeartbeatReceiver: fires on_shutdown_received") {
    HeartbeatReceiver rx;
    bool shutdown_fired = false;
    rx.on_shutdown_received.subscribe([&]() { shutdown_fired = true; });

    rx.process(5); // first
    rx.process(hb_seq::SHUTDOWN);
    CHECK(shutdown_fired);
}

TEST_CASE("HeartbeatReceiver: fires on_sender_error") {
    HeartbeatReceiver rx;
    bool error_fired = false;
    rx.on_sender_error.subscribe([&]() { error_fired = true; });

    rx.process(5); // first
    rx.process(hb_seq::SENDER_ERROR);
    CHECK(error_fired);
}

TEST_CASE("HeartbeatReceiver: recovers from SequenceError after 8 correct") {
    HeartbeatReceiver rx;

    rx.process(0);  // first
    rx.process(1);  // normal

    // Trigger sequence error with a jump > 3
    rx.process(10); // jump = 9
    CHECK(rx.state() == HBReceiverState::SequenceError);

    // Send 7 consecutive correct sequences (not enough to recover)
    for (u8 i = 11; i <= 17; ++i) {
        rx.process(i);
        CHECK(rx.state() == HBReceiverState::SequenceError);
    }

    // 8th correct sequence recovers
    rx.process(18);
    CHECK(rx.state() == HBReceiverState::Normal);
    CHECK(rx.is_healthy());
}

TEST_CASE("HeartbeatReceiver: recovery resets on another error") {
    HeartbeatReceiver rx;

    rx.process(0);  // first
    rx.process(1);  // normal
    rx.process(10); // jump error
    CHECK(rx.state() == HBReceiverState::SequenceError);

    // Send 5 correct sequences
    for (u8 i = 11; i <= 15; ++i) {
        rx.process(i);
    }
    CHECK(rx.state() == HBReceiverState::SequenceError);
    CHECK(rx.recovery_counter_ == 5);

    // Another error resets recovery counter
    rx.process(15); // repeated
    CHECK(rx.recovery_counter_ == 0);
    CHECK(rx.state() == HBReceiverState::SequenceError);

    // Now need full 8 correct again
    for (u8 i = 16; i <= 23; ++i) {
        rx.process(i);
    }
    CHECK(rx.state() == HBReceiverState::Normal);
}

TEST_CASE("HeartbeatReceiver: comm error on timeout > 300ms") {
    HeartbeatReceiver rx;

    rx.process(0); // first received, starts timeout tracking

    SUBCASE("no error at 300ms") {
        rx.update(300);
        CHECK(rx.state() == HBReceiverState::Normal);
    }

    SUBCASE("error at 301ms") {
        rx.update(301);
        CHECK(rx.state() == HBReceiverState::CommError);
        CHECK(!rx.is_healthy());
    }

    SUBCASE("error accumulates over multiple updates") {
        rx.update(100);
        CHECK(rx.state() == HBReceiverState::Normal);
        rx.update(100);
        CHECK(rx.state() == HBReceiverState::Normal);
        rx.update(101);
        CHECK(rx.state() == HBReceiverState::CommError);
    }
}

TEST_CASE("HeartbeatReceiver: no comm error before first message") {
    HeartbeatReceiver rx;

    // Even with long timeout, no error if never received anything
    rx.update(1000);
    CHECK(rx.state() == HBReceiverState::Normal);
}

TEST_CASE("HeartbeatReceiver: recovers from CommError on next valid heartbeat") {
    HeartbeatReceiver rx;

    rx.process(0); // first
    rx.update(301); // comm error
    CHECK(rx.state() == HBReceiverState::CommError);

    // Any valid heartbeat recovers
    rx.process(5);
    CHECK(rx.state() == HBReceiverState::Normal);
    CHECK(rx.is_healthy());
}

TEST_CASE("HeartbeatReceiver: state change events") {
    HeartbeatReceiver rx;

    HBReceiverState old_state = HBReceiverState::Normal;
    HBReceiverState new_state = HBReceiverState::Normal;
    u32 change_count = 0;

    rx.on_state_change.subscribe([&](HBReceiverState o, HBReceiverState n) {
        old_state = o;
        new_state = n;
        change_count++;
    });

    SUBCASE("Normal -> SequenceError event") {
        rx.process(0);  // first
        rx.process(1);  // normal
        rx.process(1);  // repeated -> error
        CHECK(change_count == 1);
        CHECK(old_state == HBReceiverState::Normal);
        CHECK(new_state == HBReceiverState::SequenceError);
    }

    SUBCASE("Normal -> CommError event") {
        rx.process(0); // first
        rx.update(301);
        CHECK(change_count == 1);
        CHECK(old_state == HBReceiverState::Normal);
        CHECK(new_state == HBReceiverState::CommError);
    }

    SUBCASE("CommError -> Normal event on recovery") {
        rx.process(0); // first
        rx.update(301); // comm error
        CHECK(change_count == 1);

        rx.process(5); // recover
        CHECK(change_count == 2);
        CHECK(old_state == HBReceiverState::CommError);
        CHECK(new_state == HBReceiverState::Normal);
    }

    SUBCASE("SequenceError -> Normal event after 8 good") {
        rx.process(0);  // first
        rx.process(1);  // normal
        rx.process(10); // error
        CHECK(change_count == 1);

        for (u8 i = 11; i <= 18; ++i) {
            rx.process(i);
        }
        CHECK(change_count == 2);
        CHECK(old_state == HBReceiverState::SequenceError);
        CHECK(new_state == HBReceiverState::Normal);
    }
}

TEST_CASE("HeartbeatReceiver: process resets comm timer") {
    HeartbeatReceiver rx;

    rx.process(0);   // first
    rx.update(200);  // 200ms elapsed
    CHECK(rx.state() == HBReceiverState::Normal);

    rx.process(1);   // resets timer
    rx.update(200);  // 200ms since last process (not 400ms total)
    CHECK(rx.state() == HBReceiverState::Normal);

    rx.update(101);  // now 301ms since last process
    CHECK(rx.state() == HBReceiverState::CommError);
}

TEST_CASE("HeartbeatReceiver: SequenceError -> CommError on timeout") {
    HeartbeatReceiver rx;

    rx.process(0);  // first
    rx.process(1);  // normal
    rx.process(1);  // repeated -> SequenceError
    CHECK(rx.state() == HBReceiverState::SequenceError);

    // Timeout while in SequenceError should transition to CommError
    rx.update(301);
    CHECK(rx.state() == HBReceiverState::CommError);
}

TEST_CASE("HeartbeatSender and HeartbeatReceiver: integration") {
    HeartbeatSender tx;
    HeartbeatReceiver rx;

    // Send init and first few normal sequences through receiver
    u8 seq = tx.next_sequence(); // 251 (INIT)
    rx.process(seq);
    CHECK(rx.state() == HBReceiverState::Normal);
    CHECK(rx.last_sequence_ == 251);

    seq = tx.next_sequence(); // 0
    rx.process(seq);
    CHECK(rx.state() == HBReceiverState::Normal);

    seq = tx.next_sequence(); // 1
    rx.process(seq);
    CHECK(rx.state() == HBReceiverState::Normal);

    seq = tx.next_sequence(); // 2
    rx.process(seq);
    CHECK(rx.state() == HBReceiverState::Normal);

    seq = tx.next_sequence(); // 3
    rx.process(seq);
    CHECK(rx.state() == HBReceiverState::Normal);
}

TEST_CASE("HeartbeatReceiver: INIT 251 mid-stream triggers reset event") {
    HeartbeatReceiver rx;
    bool reset_fired = false;
    rx.on_reset_received.subscribe([&]() { reset_fired = true; });

    rx.process(0); // first
    rx.process(1); // normal
    rx.process(2); // normal
    CHECK(rx.state() == HBReceiverState::Normal);

    // Sender resets and sends 251
    rx.process(hb_seq::INIT);
    CHECK(reset_fired);
    CHECK(rx.state() == HBReceiverState::Normal); // No error state
    CHECK(rx.last_sequence_ == hb_seq::INIT);     // Synchronized

    // After reset, 0 is the expected next value (jump of 1 from INIT)
    rx.process(0);
    CHECK(rx.state() == HBReceiverState::Normal);
    rx.process(1);
    CHECK(rx.state() == HBReceiverState::Normal);
}

TEST_CASE("HeartbeatReceiver: init sequence 251 handled as first") {
    HeartbeatReceiver rx;

    // Receiver gets INIT (251) as first message - just records it
    rx.process(hb_seq::INIT);
    CHECK(rx.state() == HBReceiverState::Normal);
    CHECK(rx.last_sequence_ == hb_seq::INIT);

    // Then 0 - jump from 251 to 0: compute_jump(251, 0)
    // Since from==INIT(251), compute_jump returns 1 (next expected is 0)
    // Jump of 1 is <=3, so stays Normal.
    rx.process(0);
    CHECK(rx.state() == HBReceiverState::Normal);
}
