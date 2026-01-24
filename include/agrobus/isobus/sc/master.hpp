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

    // ─── ISO 11783-14 Sequence Control Master ────────────────────────────────────
    class SCMaster {
        IsoNet &net_;
        InternalCF *cf_;
        SCMasterConfig config_;
        StateMachine<SCState> state_machine_{SCState::Idle};

        dp::Vector<SequenceStep> steps_;
        usize current_step_index_ = 0;

        u32 status_timer_ms_ = 0;
        u32 ready_timer_ms_ = 0;
        u32 active_timer_ms_ = 0;
        bool client_ack_received_ = false;
        bool busy_nv_memory_ = false;   // Byte 5, bit 1: NV memory access in progress
        bool busy_parsing_scd_ = false; // Byte 5, bit 2: parsing SCD in progress

      public:
        SCMaster(IsoNet &net, InternalCF *cf, SCMasterConfig config = {}) : net_(net), cf_(cf), config_(config) {

            // Forward state machine transitions to our event
            state_machine_.on_transition.subscribe([this](SCState old_state, SCState new_state) {
                on_state_change.emit(old_state, new_state);
                echo::category("isobus.sc.master")
                    .debug("state: ", static_cast<int>(old_state), " -> ", static_cast<int>(new_state));
            });
        }

        // ─── Initialization ─────────────────────────────────────────────────────
        Result<void> initialize() {
            if (!cf_) {
                return Result<void>::err(Error::invalid_state("control function not set"));
            }
            net_.register_pgn_callback(PGN_SC_CLIENT_STATUS, [this](const Message &msg) { handle_client_status(msg); });
            echo::category("isobus.sc.master").debug("initialized");
            return {};
        }

        // ─── Step management ─────────────────────────────────────────────────────
        Result<void> add_step(SequenceStep step) {
            if (!state_machine_.is(SCState::Idle)) {
                return Result<void>::err(Error::invalid_state("can only add steps in Idle state"));
            }
            steps_.push_back(std::move(step));
            return {};
        }

        const dp::Vector<SequenceStep> &steps() const noexcept { return steps_; }

        dp::Optional<SequenceStep> current_step() const noexcept {
            if (current_step_index_ < steps_.size()) {
                return steps_[current_step_index_];
            }
            return dp::nullopt;
        }

        // ─── Sequence control ────────────────────────────────────────────────────
        Result<void> start() {
            if (!state_machine_.is(SCState::Idle)) {
                return Result<void>::err(Error::invalid_state("can only start from Idle"));
            }
            if (steps_.empty()) {
                return Result<void>::err(Error::invalid_state("no steps defined"));
            }

            current_step_index_ = 0;
            ready_timer_ms_ = 0;
            status_timer_ms_ = 0;
            state_machine_.transition(SCState::Ready);
            echo::category("isobus.sc.master").info("sequence started, entering Ready");
            return {};
        }

        Result<void> abort() {
            if (state_machine_.is(SCState::Idle) || state_machine_.is(SCState::Complete) ||
                state_machine_.is(SCState::Error)) {
                return Result<void>::err(Error::invalid_state("nothing to abort"));
            }
            state_machine_.transition(SCState::Error);
            echo::category("isobus.sc.master").warn("sequence aborted");
            return {};
        }

        Result<void> pause() {
            if (!state_machine_.is(SCState::Active)) {
                return Result<void>::err(Error::invalid_state("can only pause in Active state"));
            }
            state_machine_.transition(SCState::Paused);
            echo::category("isobus.sc.master").info("sequence paused");
            return {};
        }

        Result<void> resume() {
            if (!state_machine_.is(SCState::Paused)) {
                return Result<void>::err(Error::invalid_state("can only resume from Paused state"));
            }
            active_timer_ms_ = 0;
            client_ack_received_ = false;
            state_machine_.transition(SCState::Active);
            echo::category("isobus.sc.master").info("sequence resumed");
            return {};
        }

        // ─── Step completion ─────────────────────────────────────────────────────
        Result<void> step_completed(u16 step_id) {
            if (!state_machine_.is(SCState::Active)) {
                return Result<void>::err(Error::invalid_state("not in Active state"));
            }
            if (current_step_index_ >= steps_.size()) {
                return Result<void>::err(Error::invalid_state("no current step"));
            }
            if (steps_[current_step_index_].step_id != step_id) {
                return Result<void>::err(Error::invalid_state("step_id mismatch"));
            }

            steps_[current_step_index_].completed = true;
            on_step_completed.emit(step_id);
            echo::category("isobus.sc.master").debug("step completed: ", step_id);

            // Advance to next step
            current_step_index_++;
            if (current_step_index_ >= steps_.size()) {
                // All steps complete
                state_machine_.transition(SCState::Complete);
                on_sequence_complete.emit();
                echo::category("isobus.sc.master").info("sequence complete");
            } else {
                // Start next step
                active_timer_ms_ = 0;
                client_ack_received_ = false;
                on_step_started.emit(steps_[current_step_index_].step_id);
                echo::category("isobus.sc.master").debug("step started: ", steps_[current_step_index_].step_id);
            }
            return {};
        }

        // ─── Periodic update ─────────────────────────────────────────────────────
        void update(u32 elapsed_ms) {
            if (state_machine_.is(SCState::Idle) || state_machine_.is(SCState::Complete) ||
                state_machine_.is(SCState::Error)) {
                return;
            }

            // Send periodic status messages
            status_timer_ms_ += elapsed_ms;
            if (status_timer_ms_ >= config_.status_interval_ms) {
                status_timer_ms_ -= config_.status_interval_ms;
                send_master_status();
            }

            if (state_machine_.is(SCState::Ready)) {
                ready_timer_ms_ += elapsed_ms;
                if (ready_timer_ms_ >= config_.ready_timeout_ms) {
                    state_machine_.transition(SCState::Error);
                    on_timeout.emit(dp::String("ready timeout"));
                    echo::category("isobus.sc.master").warn("ready timeout (", config_.ready_timeout_ms, "ms)");
                }
            } else if (state_machine_.is(SCState::Active)) {
                if (!client_ack_received_) {
                    active_timer_ms_ += elapsed_ms;
                    if (active_timer_ms_ >= config_.active_timeout_ms) {
                        state_machine_.transition(SCState::Error);
                        on_timeout.emit(dp::String("active timeout - no client ack"));
                        echo::category("isobus.sc.master").warn("active timeout (", config_.active_timeout_ms, "ms)");
                    }
                }
            }
        }

        // ─── State access ────────────────────────────────────────────────────────
        SCState state() const noexcept { return state_machine_.state(); }
        bool is(SCState s) const noexcept { return state_machine_.is(s); }

        // ─── Busy flags (ISO 11783-14 F.2, byte 5) ──────────────────────────────
        void set_busy_nv_memory(bool busy) noexcept { busy_nv_memory_ = busy; }
        void set_busy_parsing_scd(bool busy) noexcept { busy_parsing_scd_ = busy; }

        // ─── Events ──────────────────────────────────────────────────────────────
        Event<SCState, SCState> on_state_change;
        Event<u16> on_step_started;
        Event<u16> on_step_completed;
        Event<> on_sequence_complete;
        Event<dp::String> on_timeout;
        Event<Address, SCState> on_client_status;

      private:
        // ─── ISO 11783-14 state mapping helpers ──────────────────────────────────
        SCMasterState iso_master_state() const noexcept {
            switch (state_machine_.state()) {
            case SCState::Idle:
            case SCState::Complete:
                return SCMasterState::Inactive;
            default:
                return SCMasterState::Active;
            }
        }

        SCSequenceState iso_sequence_state() const noexcept {
            switch (state_machine_.state()) {
            case SCState::Ready:
                return SCSequenceState::Ready;
            case SCState::Active:
                return SCSequenceState::PlayBack;
            case SCState::Paused:
                return SCSequenceState::Ready; // Paused returns to Ready state
            case SCState::Error:
                return SCSequenceState::Abort;
            default:
                return SCSequenceState::Reserved;
            }
        }

        // ─── ISO 11783-14 F.2: SCMasterStatus message ──────────────────────────────
        void send_master_status() {
            if (!cf_)
                return;

            dp::Vector<u8> data(8, 0xFF);
            data[0] = SC_MSG_CODE_MASTER;                  // Byte 1: Message code
            data[1] = static_cast<u8>(iso_master_state()); // Byte 2: SCM state
            // Byte 3: Sequence number (0x00-0x31 valid, 0xFF = N/A)
            if (iso_master_state() == SCMasterState::Inactive || iso_sequence_state() == SCSequenceState::Ready) {
                data[2] = 0xFF;
            } else {
                // Clamp to max valid sequence number (0x31 = 49)
                data[2] = static_cast<u8>(current_step_index_ <= 0x31 ? current_step_index_ : 0x31);
            }
            data[3] = static_cast<u8>(iso_sequence_state()); // Byte 4: Sequence state
            // Byte 5: Busy flags (bit 1=NV memory, bit 2=parsing SCD, bits 3-8 reserved as 0)
            data[4] = static_cast<u8>((busy_nv_memory_ ? 0x01 : 0) | (busy_parsing_scd_ ? 0x02 : 0));
            // Bytes 6-8: Reserved (0xFF, already set)

            net_.send(PGN_SC_MASTER_STATUS, data, cf_, nullptr, Priority::Default);
        }

        // ─── ISO 11783-14 F.3: Decode SCClientStatus message ────────────────────────
        void handle_client_status(const Message &msg) {
            if (msg.data.size() < 5)
                return;

            // Validate message code
            u8 msg_code = msg.get_u8(0);
            if (msg_code != SC_MSG_CODE_CLIENT)
                return;

            SCClientState client_state = static_cast<SCClientState>(msg.get_u8(1));
            SCSequenceState seq_state = static_cast<SCSequenceState>(msg.get_u8(3));
            Address client_addr = msg.source;

            // Map ISO states to internal SCState
            SCState mapped_state = SCState::Idle;
            if (client_state == SCClientState::Enabled) {
                switch (seq_state) {
                case SCSequenceState::Ready:
                    mapped_state = SCState::Ready;
                    break;
                case SCSequenceState::PlayBack:
                    mapped_state = SCState::Active;
                    break;
                case SCSequenceState::Abort:
                    mapped_state = SCState::Error;
                    break;
                default:
                    mapped_state = SCState::Ready;
                    break;
                }
            }

            on_client_status.emit(client_addr, mapped_state);

            // In Ready state, if client reports Ready, transition to Active
            if (state_machine_.is(SCState::Ready) && mapped_state == SCState::Ready) {
                active_timer_ms_ = 0;
                client_ack_received_ = false;
                state_machine_.transition(SCState::Active);
                if (!steps_.empty()) {
                    on_step_started.emit(steps_[0].step_id);
                }
                echo::category("isobus.sc.master").info("client ready, entering Active");
            }

            // In Active state, client ack resets the active timeout
            if (state_machine_.is(SCState::Active) && mapped_state == SCState::Active) {
                client_ack_received_ = true;
                active_timer_ms_ = 0;
            }
        }
    };
} // namespace agrobus::isobus::sc
