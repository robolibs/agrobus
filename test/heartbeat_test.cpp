#include <doctest/doctest.h>
#include <agrobus/j1939/heartbeat.hpp>

using namespace agrobus::j1939;

// ═════════════════════════════════════════════════════════════════════════════
// HeartbeatSender Tests (ISO 11783-7 §8)
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("HeartbeatSender state machine") {
    SUBCASE("initial sequence") {
        HeartbeatSender sender;

        // First heartbeat SHALL be 251 (INIT)
        CHECK(sender.next_sequence() == hb_seq::INIT);
    }

    SUBCASE("sequence after init") {
        HeartbeatSender sender;

        sender.next_sequence(); // 251 (INIT)

        // After INIT(251), sequence should start at 0
        CHECK(sender.next_sequence() == 0);
        CHECK(sender.next_sequence() == 1);
        CHECK(sender.next_sequence() == 2);
    }

    SUBCASE("normal sequence progression 0-250") {
        HeartbeatSender sender;

        sender.next_sequence(); // 251 (INIT)
        sender.next_sequence(); // 0

        // Sequence 0..250
        for (u16 i = 1; i <= hb_seq::MAX_NORMAL; ++i) {
            CHECK(sender.next_sequence() == static_cast<u8>(i));
        }
    }

    SUBCASE("sequence rollover at 250") {
        HeartbeatSender sender;

        sender.next_sequence(); // 251 (INIT)

        // Go through full cycle: 0..250
        for (u16 i = 0; i <= hb_seq::MAX_NORMAL; ++i) {
            sender.next_sequence();
        }

        // After 250, should roll over to 0
        CHECK(sender.next_sequence() == 0);
        CHECK(sender.next_sequence() == 1);
    }

    SUBCASE("signal_error sends 254 exactly once") {
        HeartbeatSender sender;

        sender.next_sequence(); // 251 (INIT)
        sender.next_sequence(); // 0
        sender.next_sequence(); // 1

        sender.signal_error();

        // Next sequence should be 254 (SENDER_ERROR)
        CHECK(sender.next_sequence() == hb_seq::SENDER_ERROR);

        // After error, restart at 0
        CHECK(sender.next_sequence() == 0);
        CHECK(sender.next_sequence() == 1);
    }

    SUBCASE("signal_shutdown sends 255 exactly once") {
        HeartbeatSender sender;

        sender.next_sequence(); // 251 (INIT)
        sender.next_sequence(); // 0
        sender.next_sequence(); // 1);

        sender.signal_shutdown();

        // Next sequence should be 255 (SHUTDOWN)
        CHECK(sender.next_sequence() == hb_seq::SHUTDOWN);

        // After shutdown, restart at 0
        CHECK(sender.next_sequence() == 0);
        CHECK(sender.next_sequence() == 1);
    }

    SUBCASE("reset returns to initial state") {
        HeartbeatSender sender;

        sender.next_sequence(); // 251 (INIT)
        sender.next_sequence(); // 0
        sender.next_sequence(); // 1
        sender.next_sequence(); // 2

        sender.reset();

        // After reset, should send INIT again
        CHECK(sender.next_sequence() == hb_seq::INIT);
        CHECK(sender.next_sequence() == 0);
    }

    SUBCASE("update timer triggers at 100ms interval") {
        HeartbeatSender sender;

        CHECK_FALSE(sender.update(50));   // 50ms total - not yet
        CHECK_FALSE(sender.update(30));   // 80ms total - not yet
        CHECK_FALSE(sender.update(10));   // 90ms total - not yet
        CHECK(sender.update(10));         // 100ms total - trigger!

        // Timer should reset, accumulating remainder
        CHECK_FALSE(sender.update(50));   // 50ms - not yet
        CHECK(sender.update(50));         // 100ms - trigger!
    }

    SUBCASE("multiple error signals") {
        HeartbeatSender sender;

        sender.next_sequence(); // 251 (INIT)
        sender.next_sequence(); // 0

        sender.signal_error();
        CHECK(sender.next_sequence() == hb_seq::SENDER_ERROR);

        // Signal error again
        sender.signal_error();
        CHECK(sender.next_sequence() == hb_seq::SENDER_ERROR);

        // Back to normal
        CHECK(sender.next_sequence() == 0);
    }

    SUBCASE("shutdown after error") {
        HeartbeatSender sender;

        sender.next_sequence(); // 251 (INIT)
        sender.next_sequence(); // 0

        sender.signal_error();
        CHECK(sender.next_sequence() == hb_seq::SENDER_ERROR);

        sender.signal_shutdown();
        CHECK(sender.next_sequence() == hb_seq::SHUTDOWN);

        CHECK(sender.next_sequence() == 0);
    }
}

// ...continuing in next message due to length...

// ═════════════════════════════════════════════════════════════════════════════
// HeartbeatReceiver Tests (ISO 11783-7 §8)
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("HeartbeatReceiver state machine") {
    SUBCASE("initial state is Normal") {
        HeartbeatReceiver receiver;
        CHECK(receiver.state() == HBReceiverState::Normal);
        CHECK(receiver.is_healthy());
    }

    SUBCASE("normal sequence progression") {
        HeartbeatReceiver receiver;

        receiver.process(hb_seq::INIT); // 251
        receiver.process(0);
        receiver.process(1);
        receiver.process(2);
        receiver.process(3);

        CHECK(receiver.state() == HBReceiverState::Normal);
    }

    SUBCASE("sequence error - repeated sequence") {
        HeartbeatReceiver receiver;

        receiver.process(hb_seq::INIT);
        receiver.process(0);
        receiver.process(1);
        receiver.process(1); // REPEAT - should trigger error

        CHECK(receiver.state() == HBReceiverState::SequenceError);
        CHECK_FALSE(receiver.is_healthy());
    }

    SUBCASE("sequence error - jump > 3") {
        HeartbeatReceiver receiver;

        receiver.process(hb_seq::INIT);
        receiver.process(0);
        receiver.process(1);
        receiver.process(5); // Jump of 4 - should trigger error

        CHECK(receiver.state() == HBReceiverState::SequenceError);
    }

    SUBCASE("sequence error recovery - 8 good sequences") {
        HeartbeatReceiver receiver;

        receiver.process(hb_seq::INIT);
        receiver.process(0);
        receiver.process(0); // ERROR: repeat

        CHECK(receiver.state() == HBReceiverState::SequenceError);

        // Send 8 consecutive good sequences to recover
        for (u8 i = 1; i <= HB_RECOVERY_COUNT; ++i) {
            receiver.process(i);
            if (i < HB_RECOVERY_COUNT) {
                CHECK(receiver.state() == HBReceiverState::SequenceError);
            } else {
                CHECK(receiver.state() == HBReceiverState::Normal);
            }
        }
    }

    SUBCASE("comm error - timeout > 300ms") {
        HeartbeatReceiver receiver;

        receiver.process(hb_seq::INIT);
        receiver.process(0);

        receiver.update(150); // 150ms - OK
        CHECK(receiver.state() == HBReceiverState::Normal);

        receiver.update(150); // 300ms total - OK
        CHECK(receiver.state() == HBReceiverState::Normal);

        receiver.update(1);   // 301ms total - TIMEOUT!
        CHECK(receiver.state() == HBReceiverState::CommError);
        CHECK_FALSE(receiver.is_healthy());
    }

    SUBCASE("comm error recovery on any valid heartbeat") {
        HeartbeatReceiver receiver;

        receiver.process(hb_seq::INIT);
        receiver.process(0);
        receiver.update(301); // Trigger CommError

        CHECK(receiver.state() == HBReceiverState::CommError);

        // Any valid heartbeat recovers immediately
        receiver.process(1);

        CHECK(receiver.state() == HBReceiverState::Normal);
        CHECK(receiver.is_healthy());
    }

    SUBCASE("rollover from 250 to 0") {
        HeartbeatReceiver receiver;

        receiver.process(hb_seq::INIT);
        receiver.process(0);

        // Progress to near the end of the sequence
        for (u16 i = 1; i < 248; ++i) {
            receiver.process(static_cast<u8>(i));
        }

        // Test rollover boundary
        receiver.process(248);
        receiver.process(249);
        receiver.process(250);
        receiver.process(0); // Rollover - jump of 1

        CHECK(receiver.state() == HBReceiverState::Normal);

        receiver.process(1);
        CHECK(receiver.state() == HBReceiverState::Normal);
    }
}
