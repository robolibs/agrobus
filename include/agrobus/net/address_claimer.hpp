#pragma once

#include "internal_cf.hpp"
#include <agrobus/net/constants.hpp>
#include <agrobus/net/error.hpp>
#include <agrobus/net/frame.hpp>
#include <datapod/datapod.hpp>
#include <echo/echo.hpp>

namespace agrobus::net {

    // ─── Address claiming protocol (ISO 11783-5 §4.4.2) ──────────────────────────
    class AddressClaimer {
        InternalCF *cf_;
        u32 timeout_ms_;
        u32 rtxd_ms_; // Random transmit delay (ISO 11783-5 §3.4): 0.6ms × random[0-255]

        // ISO 11783-5 §4.4.2: A CF that has NOT attempted claim SHALL NOT
        // send any message except request-for-address-claimed
        bool attempted_claim_ = false;

        // Guard window: after sending address claim, wait 250ms + RTxD before
        // declaring success. If a contending claim arrives within this window
        // with higher priority NAME, yield.
        u32 claim_guard_timer_ms_ = 0;

        // ISO 11783-5 §4.4.3: When re-claiming after yielding, apply RTxD delay
        // before sending the new claim frame.
        bool reclaim_pending_ = false;
        u32 reclaim_delay_timer_ms_ = 0;
        Address reclaim_address_ = NULL_ADDRESS;

      public:
        // rtxd_ms: Random transmit delay in ms (0-153, per ISO 11783-5 §3.4).
        // Caller should provide 0.6 × random_byte (0-255) as the RTxD value.
        explicit AddressClaimer(InternalCF *cf, u32 timeout_ms = ADDRESS_CLAIM_TIMEOUT_MS, u32 rtxd_ms = 0)
            : cf_(cf), timeout_ms_(timeout_ms + rtxd_ms), rtxd_ms_(rtxd_ms) {}

        // Query whether this CF has attempted an address claim
        bool has_attempted_claim() const noexcept { return attempted_claim_; }

        // Query the guard timer value (ms since claim was sent)
        u32 guard_timer() const noexcept { return claim_guard_timer_ms_; }

        // Start the address claim process
        dp::Vector<Frame> start() {
            dp::Vector<Frame> frames;
            echo::category("isobus.network.claim")
                .debug("starting address claim: preferred=", cf_->preferred_address());
            cf_->state_machine().transition(ClaimState::SendRequest);

            // Mark that we have attempted a claim
            attempted_claim_ = true;

            // Send PGN request for address claimed (DLC=8, pad with 0xFF per J1939-21)
            Frame req;
            req.id = Identifier::encode(Priority::Default, PGN_REQUEST, NULL_ADDRESS, BROADCAST_ADDRESS);
            dp::Array<u8, 8> req_data;
            req_data.fill(0xFF);
            req_data[0] = static_cast<u8>(PGN_ADDRESS_CLAIMED & 0xFF);
            req_data[1] = static_cast<u8>((PGN_ADDRESS_CLAIMED >> 8) & 0xFF);
            req_data[2] = static_cast<u8>((PGN_ADDRESS_CLAIMED >> 16) & 0xFF);
            req.data = req_data;
            req.length = 8;
            frames.push_back(req);

            cf_->state_machine().transition(ClaimState::SendClaim);
            cf_->reset_claim_timer();
            claim_guard_timer_ms_ = 0;

            // Send our address claim (DLC=8 guaranteed by make_claim_frame)
            frames.push_back(make_claim_frame(cf_->preferred_address()));
            cf_->state_machine().transition(ClaimState::WaitForContest);

            return frames;
        }

        // Process elapsed time, returns frames to send
        dp::Vector<Frame> update(u32 elapsed_ms) {
            dp::Vector<Frame> frames;

            // Handle pending re-claim with RTxD delay
            if (reclaim_pending_) {
                reclaim_delay_timer_ms_ += elapsed_ms;
                if (reclaim_delay_timer_ms_ >= rtxd_ms_) {
                    reclaim_pending_ = false;
                    cf_->set_address(reclaim_address_);
                    cf_->reset_claim_timer();
                    claim_guard_timer_ms_ = 0;
                    cf_->state_machine().transition(ClaimState::SendClaim);
                    frames.push_back(make_claim_frame(reclaim_address_));
                    cf_->state_machine().transition(ClaimState::WaitForContest);
                    echo::category("isobus.network.claim").debug("re-claim sent after RTxD: addr=", reclaim_address_);
                }
                return frames;
            }

            if (cf_->claim_state() == ClaimState::WaitForContest) {
                cf_->add_claim_time(elapsed_ms);
                claim_guard_timer_ms_ += elapsed_ms;

                if (claim_guard_timer_ms_ >= timeout_ms_) {
                    // Guard window expired without contention - claim successful
                    // (don't snap back to preferred if we already moved to a candidate)
                    cf_->state_machine().transition(ClaimState::Claimed);
                    if (cf_->address() == NULL_ADDRESS) {
                        cf_->set_address(cf_->preferred_address());
                    }
                    cf_->set_state(CFState::Online);
                    cf_->on_address_claimed.emit(cf_->address());
                    echo::category("isobus.network.claim").info("Address claimed: ", cf_->address());
                }
            }

            return frames;
        }

        // Handle incoming address claim from another device
        dp::Vector<Frame> handle_claim(Address claimed_address, Name other_name) {
            dp::Vector<Frame> frames;

            if (claimed_address != cf_->address() && claimed_address != cf_->preferred_address()) {
                return frames; // Not our address
            }

            if (cf_->name() < other_name) {
                // We win - re-send our claim (DLC=8)
                echo::category("isobus.network.claim").debug("Won address contest against: ", other_name.raw);
                frames.push_back(make_claim_frame(cf_->address()));
            } else {
                // We lose - must yield
                echo::category("isobus.network.claim").warn("Lost address contest, yielding: ", claimed_address);
                cf_->set_state(CFState::Offline);
                cf_->on_address_lost.emit();

                if (cf_->name().self_configurable()) {
                    // Try next address with RTxD delay per ISO 11783-5 §4.4.3
                    Address next = find_next_address(claimed_address);
                    if (next <= MAX_ADDRESS) {
                        if (rtxd_ms_ > 0) {
                            // Queue re-claim with RTxD delay
                            reclaim_pending_ = true;
                            reclaim_delay_timer_ms_ = 0;
                            reclaim_address_ = next;
                            echo::category("isobus.network.claim")
                                .debug("re-claim queued with RTxD=", rtxd_ms_, "ms: addr=", next);
                        } else {
                            // No RTxD configured, send immediately
                            cf_->set_address(next);
                            cf_->reset_claim_timer();
                            claim_guard_timer_ms_ = 0;
                            cf_->state_machine().transition(ClaimState::SendClaim);
                            frames.push_back(make_claim_frame(next));
                            cf_->state_machine().transition(ClaimState::WaitForContest);
                        }
                    } else {
                        echo::category("isobus.network.claim").error("no available address, claim failed");
                        cf_->state_machine().transition(ClaimState::Failed);
                        cf_->set_address(NULL_ADDRESS);
                        // Send cannot claim (SA=0xFE, DLC=8)
                        frames.push_back(make_claim_frame(NULL_ADDRESS));
                    }
                } else {
                    echo::category("isobus.network.claim").error("not self-configurable, claim failed");
                    cf_->state_machine().transition(ClaimState::Failed);
                    cf_->set_address(NULL_ADDRESS);
                    frames.push_back(make_claim_frame(NULL_ADDRESS));
                }
            }

            return frames;
        }

        // Handle request-for-address-claimed (ISO 11783-5 §4.4.2)
        // Returns frames to send as response:
        //   - If claimed: respond with own address claim
        //   - If unable to claim (lost contention): respond with cannot-claim (SA=0xFE)
        //   - If not yet attempted: do NOT respond
        dp::Vector<Frame> handle_request_for_claim() {
            dp::Vector<Frame> frames;

            if (!attempted_claim_) {
                // §4.4.2: A CF that has NOT attempted claim SHALL NOT respond
                echo::category("isobus.network.claim").debug("ignoring request-for-claim: not yet attempted");
                return frames;
            }

            if (cf_->claim_state() == ClaimState::Claimed) {
                // Respond with our address claim (DLC=8)
                echo::category("isobus.network.claim")
                    .debug("responding to request-for-claim with address: ", cf_->address());
                frames.push_back(make_claim_frame(cf_->address()));
            } else if (cf_->claim_state() == ClaimState::Failed) {
                // Respond with cannot-claim (SA=0xFE, DLC=8)
                echo::category("isobus.network.claim").debug("responding to request-for-claim with cannot-claim");
                frames.push_back(make_claim_frame(NULL_ADDRESS));
            }
            // If in WaitForContest or other intermediate state, respond with current claim
            else if (cf_->claim_state() == ClaimState::WaitForContest) {
                echo::category("isobus.network.claim")
                    .debug("responding to request-for-claim during contest: ", cf_->address());
                frames.push_back(make_claim_frame(cf_->address()));
            }

            return frames;
        }

      private:
        // All frames are DLC=8 with NAME bytes (exactly 8 bytes) per J1939
        Frame make_claim_frame(Address addr) const noexcept {
            Frame f;
            f.id = Identifier::encode(Priority::Default, PGN_ADDRESS_CLAIMED, addr, BROADCAST_ADDRESS);
            auto name_bytes = cf_->name().to_bytes();
            f.data = name_bytes;
            f.length = 8; // DLC=8 per J1939-21
            return f;
        }

        Address find_next_address(Address current) const noexcept {
            // Simple linear search for next available address
            Address next = (current < MAX_ADDRESS) ? current + 1 : 0;
            if (next == cf_->preferred_address())
                ++next;
            return next;
        }
    };
} // namespace agrobus::net
