#pragma once

#include "types.hpp"
#include <agrobus/net/error.hpp>
#include <agrobus/net/event.hpp>
#include <agrobus/net/message.hpp>
#include <agrobus/net/network_manager.hpp>
#include <agrobus/net/state_machine.hpp>
#include <datapod/datapod.hpp>
#include <echo/echo.hpp>

namespace agrobus::isobus::sc {
    using namespace agrobus::net;

    // ─── ISO 11783-14 Sequence Control Client ────────────────────────────────────
    class SCClient {
        IsoNet &net_;
        InternalCF *cf_;
        SCClientConfig config_;
        StateMachine<SCState> state_machine_{SCState::Idle};

        bool busy_ = false;
        u32 busy_timer_ms_ = 0;

        u32 last_status_sent_ms_ = 0;
        u32 time_since_last_status_ms_ = 0;
        bool status_pending_ = false;

        u16 current_step_id_ = 0;

      public:
        SCClient(IsoNet &net, InternalCF *cf, SCClientConfig config = {}) : net_(net), cf_(cf), config_(config) {

            // Forward state machine transitions to our event
            state_machine_.on_transition.subscribe([this](SCState old_state, SCState new_state) {
                on_state_change.emit(old_state, new_state);
                echo::category("isobus.sc.client")
                    .debug("state: ", static_cast<int>(old_state), " -> ", static_cast<int>(new_state));
            });
        }

        // ─── Initialization ─────────────────────────────────────────────────────
        Result<void> initialize() {
            if (!cf_) {
                return Result<void>::err(Error::invalid_state("control function not set"));
            }
            net_.register_pgn_callback(PGN_SC_MASTER_STATUS, [this](const Message &msg) { handle_master_status(msg); });
            echo::category("isobus.sc.client").debug("initialized");
            return {};
        }

        // ─── Busy signaling ─────────────────────────────────────────────────────
        void set_busy(bool busy) {
            if (busy_ == busy)
                return;
            busy_ = busy;
            if (busy_) {
                busy_timer_ms_ = 0;
                echo::category("isobus.sc.client").debug("busy signaled");
            } else {
                busy_timer_ms_ = 0;
                echo::category("isobus.sc.client").debug("busy cleared");
            }
            request_status_send();
        }

        bool is_busy() const noexcept { return busy_; }

        // ─── Step completion ─────────────────────────────────────────────────────
        Result<void> report_step_complete(u16 step_id) {
            if (!state_machine_.is(SCState::Active)) {
                return Result<void>::err(Error::invalid_state("not in Active state"));
            }
            if (step_id != current_step_id_) {
                return Result<void>::err(Error::invalid_state("step_id mismatch"));
            }
            echo::category("isobus.sc.client").debug("step complete reported: ", step_id);
            request_status_send();
            return {};
        }

        // ─── Periodic update ─────────────────────────────────────────────────────
        void update(u32 elapsed_ms) {
            time_since_last_status_ms_ += elapsed_ms;

            // Handle busy pause timeout measurement
            if (busy_ && (state_machine_.is(SCState::Active) || state_machine_.is(SCState::Paused))) {
                busy_timer_ms_ += elapsed_ms;
                if (busy_timer_ms_ >= config_.busy_pause_timeout_ms) {
                    state_machine_.transition(SCState::Error);
                    echo::category("isobus.sc.client")
                        .warn("busy pause timeout (", config_.busy_pause_timeout_ms, "ms)");
                }
            }

            // Send deferred status if spacing allows
            if (status_pending_ && time_since_last_status_ms_ >= config_.min_status_spacing_ms) {
                send_client_status();
                status_pending_ = false;
            }
        }

        // ─── State access ────────────────────────────────────────────────────────
        SCState state() const noexcept { return state_machine_.state(); }
        bool is(SCState s) const noexcept { return state_machine_.is(s); }
        u32 time_since_last_status() const noexcept { return time_since_last_status_ms_; }

        // ─── Events ──────────────────────────────────────────────────────────────
        Event<> on_sequence_start;
        Event<u16> on_step_request;
        Event<> on_pause;
        Event<> on_resume;
        Event<> on_abort;
        Event<SCState, SCState> on_state_change;

      private:
        void request_status_send() {
            if (time_since_last_status_ms_ >= config_.min_status_spacing_ms) {
                send_client_status();
            } else {
                status_pending_ = true;
            }
        }

        // ─── ISO state mapping helpers ──────────────────────────────────────────────
        SCClientState iso_client_state() const noexcept {
            return (state_machine_.state() == SCState::Idle) ? SCClientState::Disabled : SCClientState::Enabled;
        }

        SCSequenceState iso_sequence_state() const noexcept {
            switch (state_machine_.state()) {
            case SCState::Ready:
                return SCSequenceState::Ready;
            case SCState::Active:
                return SCSequenceState::PlayBack;
            case SCState::Error:
                return SCSequenceState::Abort;
            default:
                return SCSequenceState::Reserved;
            }
        }

        // ─── ISO 11783-14 F.3: SCClientStatus message ──────────────────────────────
        void send_client_status() {
            if (!cf_)
                return;

            dp::Vector<u8> data(8, 0xFF);
            data[0] = SC_MSG_CODE_CLIENT;                  // Byte 1: Message code
            data[1] = static_cast<u8>(iso_client_state()); // Byte 2: SCC state
            // Byte 3: Sequence number
            data[2] = (iso_client_state() == SCClientState::Disabled || iso_sequence_state() == SCSequenceState::Ready)
                          ? static_cast<u8>(0xFF)
                          : static_cast<u8>(current_step_id_ & 0xFF);
            data[3] = static_cast<u8>(iso_sequence_state());        // Byte 4: Sequence state
            data[4] = static_cast<u8>(SCClientFuncError::NoErrors); // Byte 5: Error state
            // Bytes 6-8: Reserved (0xFF, already set)

            net_.send(PGN_SC_CLIENT_STATUS, data, cf_, nullptr, Priority::Default);
            time_since_last_status_ms_ = 0;
        }

        // ─── ISO 11783-14 F.2: Decode SCMasterStatus message ────────────────────────
        void handle_master_status(const Message &msg) {
            if (msg.data.size() < 5)
                return;

            // Validate message code
            u8 msg_code = msg.get_u8(0);
            if (msg_code != SC_MSG_CODE_MASTER)
                return;

            SCMasterState master_ms = static_cast<SCMasterState>(msg.get_u8(1));
            SCSequenceState seq_state = static_cast<SCSequenceState>(msg.get_u8(3));

            // Map ISO states to internal SCState
            SCState master_state = SCState::Idle;
            if (master_ms == SCMasterState::Active) {
                switch (seq_state) {
                case SCSequenceState::Ready:
                    master_state = SCState::Ready;
                    break;
                case SCSequenceState::PlayBack:
                    master_state = SCState::Active;
                    break;
                case SCSequenceState::Abort:
                    master_state = SCState::Error;
                    break;
                default:
                    master_state = SCState::Ready;
                    break;
                }
            }

            u16 step_id = 0; // Not directly in ISO status, derived from sequence number
            u8 seq_num = msg.get_u8(2);
            if (seq_num != 0xFF) {
                step_id = seq_num;
            }

            switch (master_state) {
            case SCState::Ready:
                if (state_machine_.is(SCState::Idle)) {
                    state_machine_.transition(SCState::Ready);
                    on_sequence_start.emit();
                    echo::category("isobus.sc.client").info("sequence start received, entering Ready");
                    request_status_send();
                }
                break;

            case SCState::Active:
                if (state_machine_.is(SCState::Ready) || state_machine_.is(SCState::Active)) {
                    if (!state_machine_.is(SCState::Active)) {
                        state_machine_.transition(SCState::Active);
                    }
                    current_step_id_ = step_id;
                    on_step_request.emit(step_id);
                    request_status_send();
                }
                break;

            case SCState::Paused:
                if (state_machine_.is(SCState::Active)) {
                    state_machine_.transition(SCState::Paused);
                    on_pause.emit();
                    request_status_send();
                }
                break;

            case SCState::Complete:
                if (state_machine_.is(SCState::Active) || state_machine_.is(SCState::Paused)) {
                    state_machine_.transition(SCState::Complete);
                    request_status_send();
                }
                break;

            case SCState::Error:
                state_machine_.transition(SCState::Error);
                on_abort.emit();
                request_status_send();
                break;

            case SCState::Idle:
                if (!state_machine_.is(SCState::Idle)) {
                    state_machine_.transition(SCState::Idle);
                    on_abort.emit();
                    request_status_send();
                }
                break;
            }
        }
    };
} // namespace agrobus::isobus::sc
