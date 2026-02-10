#include <doctest/doctest.h>
#include <agrobus/net/address_claimer.hpp>
#include <agrobus/net/internal_cf.hpp>
#include <agrobus/net/constants.hpp>

using namespace agrobus::net;

TEST_CASE("Address claim strictness: attempted_claim flag") {
    Name name;
    name.set_identity_number(1234);
    name.set_manufacturer_code(100);
    name.set_self_configurable(true);

    InternalCF cf(name, 0, 0x28);
    AddressClaimer claimer(&cf);

    SUBCASE("initially not attempted") {
        CHECK(!claimer.has_attempted_claim());
    }

    SUBCASE("attempted after start") {
        claimer.start();
        CHECK(claimer.has_attempted_claim());
    }

    SUBCASE("no response to request-for-claim if not attempted") {
        // Before calling start(), should not respond
        auto frames = claimer.handle_request_for_claim();
        CHECK(frames.empty());
    }
}

TEST_CASE("Address claim guard window prevents early success") {
    Name name;
    name.set_identity_number(500);
    name.set_manufacturer_code(50);
    name.set_self_configurable(true);

    InternalCF cf(name, 0, 0x28);
    AddressClaimer claimer(&cf);

    claimer.start();

    SUBCASE("claim not complete at 100ms") {
        claimer.update(100);
        CHECK(cf.claim_state() == ClaimState::WaitForContest);
        CHECK(claimer.guard_timer() == 100);
    }

    SUBCASE("claim not complete at 200ms") {
        claimer.update(200);
        CHECK(cf.claim_state() == ClaimState::WaitForContest);
        CHECK(claimer.guard_timer() == 200);
    }

    SUBCASE("claim not complete at 249ms") {
        claimer.update(249);
        CHECK(cf.claim_state() == ClaimState::WaitForContest);
    }

    SUBCASE("claim succeeds at exactly 250ms") {
        claimer.update(ADDRESS_CLAIM_TIMEOUT_MS);
        CHECK(cf.claim_state() == ClaimState::Claimed);
        CHECK(cf.address() == 0x28);
    }

    SUBCASE("claim succeeds after 250ms in increments") {
        claimer.update(100);
        CHECK(cf.claim_state() == ClaimState::WaitForContest);
        claimer.update(100);
        CHECK(cf.claim_state() == ClaimState::WaitForContest);
        claimer.update(50);
        CHECK(cf.claim_state() == ClaimState::Claimed);
        CHECK(cf.address() == 0x28);
    }

    SUBCASE("guard timer resets on start") {
        claimer.update(100);
        CHECK(claimer.guard_timer() == 100);
        // Calling start again resets
        claimer.start();
        CHECK(claimer.guard_timer() == 0);
    }
}

TEST_CASE("Address claim: contention within 250ms causes yield") {
    Name our_name;
    our_name.set_identity_number(200);
    our_name.set_manufacturer_code(50);
    our_name.set_self_configurable(true);

    InternalCF cf(our_name, 0, 0x28);
    AddressClaimer claimer(&cf);
    claimer.start();

    // Advance 100ms (within guard window)
    claimer.update(100);
    CHECK(cf.claim_state() == ClaimState::WaitForContest);

    SUBCASE("higher priority contender within guard window yields us") {
        Name contender;
        contender.set_identity_number(50); // Lower raw value = higher priority
        contender.set_manufacturer_code(50);
        contender.set_self_configurable(true);

        auto frames = claimer.handle_claim(0x28, contender);
        // We lost, self-configurable so should try next address
        CHECK(!frames.empty());
        CHECK(cf.claim_state() == ClaimState::WaitForContest); // trying new address
        CHECK(cf.address() != 0x28); // moved to different address
    }

    SUBCASE("lower priority contender within guard window - we defend") {
        Name contender;
        contender.set_identity_number(500); // Higher raw value = lower priority
        contender.set_manufacturer_code(50);
        contender.set_self_configurable(true);

        auto frames = claimer.handle_claim(0x28, contender);
        // We won - re-sent our claim
        CHECK(!frames.empty());
        CHECK(cf.claim_state() == ClaimState::WaitForContest);
        CHECK(cf.address() == 0x28); // kept our address
    }
}

TEST_CASE("Address claim: request-for-claim response when claimed") {
    Name name;
    name.set_identity_number(1000);
    name.set_manufacturer_code(100);
    name.set_self_configurable(false);

    InternalCF cf(name, 0, 0x20);
    AddressClaimer claimer(&cf);

    claimer.start();
    claimer.update(ADDRESS_CLAIM_TIMEOUT_MS); // Complete the claim
    CHECK(cf.claim_state() == ClaimState::Claimed);

    SUBCASE("responds with own address claim") {
        auto frames = claimer.handle_request_for_claim();
        REQUIRE(frames.size() == 1);
        CHECK(frames[0].source() == 0x20);
        CHECK(frames[0].pgn() == PGN_ADDRESS_CLAIMED);
    }
}

TEST_CASE("Address claim: request-for-claim response when failed (cannot-claim)") {
    Name our_name;
    our_name.set_identity_number(200);
    our_name.set_manufacturer_code(50);
    our_name.set_self_configurable(false); // Cannot self-configure

    InternalCF cf(our_name, 0, 0x28);
    AddressClaimer claimer(&cf);
    claimer.start();

    // Lose contest to higher priority contender
    Name contender;
    contender.set_identity_number(50); // Higher priority
    contender.set_manufacturer_code(50);
    claimer.handle_claim(0x28, contender);
    CHECK(cf.claim_state() == ClaimState::Failed);

    SUBCASE("responds with cannot-claim (SA=0xFE)") {
        auto frames = claimer.handle_request_for_claim();
        REQUIRE(frames.size() == 1);
        CHECK(frames[0].source() == NULL_ADDRESS);
        CHECK(frames[0].pgn() == PGN_ADDRESS_CLAIMED);
    }
}

TEST_CASE("Address claim: no response if not yet attempted") {
    Name name;
    name.set_identity_number(1234);
    name.set_manufacturer_code(100);
    name.set_self_configurable(true);

    InternalCF cf(name, 0, 0x28);
    AddressClaimer claimer(&cf);

    // Never call start()
    CHECK(!claimer.has_attempted_claim());
    auto frames = claimer.handle_request_for_claim();
    CHECK(frames.empty());
}

TEST_CASE("Address claim: DLC=8 padding on all frames") {
    Name name;
    name.set_identity_number(999);
    name.set_manufacturer_code(77);
    name.set_self_configurable(true);

    InternalCF cf(name, 0, 0x30);
    AddressClaimer claimer(&cf);

    SUBCASE("start frames are DLC=8") {
        auto frames = claimer.start();
        for (const auto &f : frames) {
            CHECK(f.length == 8);
        }
    }

    SUBCASE("claim response frames are DLC=8") {
        claimer.start();
        claimer.update(ADDRESS_CLAIM_TIMEOUT_MS);

        auto frames = claimer.handle_request_for_claim();
        for (const auto &f : frames) {
            CHECK(f.length == 8);
        }
    }

    SUBCASE("cannot-claim frame is DLC=8") {
        Name our_name;
        our_name.set_identity_number(200);
        our_name.set_manufacturer_code(50);
        our_name.set_self_configurable(false);

        InternalCF cf2(our_name, 0, 0x28);
        AddressClaimer claimer2(&cf2);
        claimer2.start();

        Name contender;
        contender.set_identity_number(50);
        contender.set_manufacturer_code(50);
        auto frames = claimer2.handle_claim(0x28, contender);
        for (const auto &f : frames) {
            CHECK(f.length == 8);
        }
    }

    SUBCASE("contest re-claim frame is DLC=8") {
        claimer.start();

        Name contender;
        contender.set_identity_number(9999); // Lower priority than us
        contender.set_manufacturer_code(77);
        auto frames = claimer.handle_claim(0x30, contender);
        for (const auto &f : frames) {
            CHECK(f.length == 8);
        }
    }

    SUBCASE("request for address claimed frame is padded to DLC=8") {
        auto frames = claimer.start();
        // First frame is the request-for-address-claimed
        REQUIRE(frames.size() >= 1);
        CHECK(frames[0].length == 8);
        // Verify padding bytes are 0xFF (bytes 3..7)
        CHECK(frames[0].data[3] == 0xFF);
        CHECK(frames[0].data[4] == 0xFF);
        CHECK(frames[0].data[5] == 0xFF);
        CHECK(frames[0].data[6] == 0xFF);
        CHECK(frames[0].data[7] == 0xFF);
    }
}

TEST_CASE("Address claim: guard timer resets on yield to new address") {
    Name our_name;
    our_name.set_identity_number(200);
    our_name.set_manufacturer_code(50);
    our_name.set_self_configurable(true);

    InternalCF cf(our_name, 0, 0x28);
    AddressClaimer claimer(&cf);
    claimer.start();

    // Advance guard timer
    claimer.update(100);
    CHECK(claimer.guard_timer() == 100);

    // Lose contest - guard timer should reset for new address attempt
    Name contender;
    contender.set_identity_number(50); // Higher priority
    contender.set_manufacturer_code(50);
    claimer.handle_claim(0x28, contender);

    // Guard timer was reset
    CHECK(claimer.guard_timer() == 0);

    // Must wait full 250ms for new address claim to succeed
    claimer.update(200);
    CHECK(cf.claim_state() == ClaimState::WaitForContest);
    claimer.update(50);
    CHECK(cf.claim_state() == ClaimState::Claimed);
}
