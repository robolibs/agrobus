#pragma once

#include "control_function.hpp"
#include <agrobus/net/event.hpp>
#include <agrobus/net/state_machine.hpp>

namespace agrobus::net {

    // ─── Address claim state ─────────────────────────────────────────────────────
    enum class ClaimState { None, WaitForClaim, SendRequest, WaitForContest, SendClaim, Claimed, Failed };

    // ─── Internal control function (our ECU) ─────────────────────────────────────
    class InternalCF {
        ControlFunction cf_;
        StateMachine<ClaimState> state_machine_{ClaimState::None};
        Address preferred_address_;
        u32 claim_timer_ms_ = 0;

      public:
        InternalCF(Name name, u8 port, Address preferred = NULL_ADDRESS) : preferred_address_(preferred) {
            cf_.name = name;
            cf_.can_port = port;
            cf_.type = CFType::Internal;
            cf_.address = preferred;
        }

        const ControlFunction &cf() const noexcept { return cf_; }
        ControlFunction &cf() noexcept { return cf_; }
        Name name() const noexcept { return cf_.name; }
        Address address() const noexcept { return cf_.address; }
        u8 port() const noexcept { return cf_.can_port; }
        Address preferred_address() const noexcept { return preferred_address_; }
        ClaimState claim_state() const noexcept { return state_machine_.state(); }

        void set_address(Address addr) noexcept { cf_.address = addr; }
        void set_name(Name name) noexcept { cf_.name = name; }
        void set_state(CFState state) noexcept { cf_.state = state; }

        StateMachine<ClaimState> &state_machine() noexcept { return state_machine_; }
        const StateMachine<ClaimState> &state_machine() const noexcept { return state_machine_; }

        u32 claim_timer() const noexcept { return claim_timer_ms_; }
        void add_claim_time(u32 ms) noexcept { claim_timer_ms_ += ms; }
        void reset_claim_timer() noexcept { claim_timer_ms_ = 0; }

        // Events
        Event<Address> on_address_claimed;
        Event<> on_address_lost;
    };
} // namespace agrobus::net
