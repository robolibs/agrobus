#pragma once

#include <agrobus/net/constants.hpp>
#include <agrobus/net/error.hpp>
#include <agrobus/net/event.hpp>
#include <agrobus/net/message.hpp>
#include <agrobus/net/network_manager.hpp>
#include <agrobus/net/types.hpp>
#include <datapod/datapod.hpp>
#include <echo/echo.hpp>

namespace agrobus::j1939 {
    using namespace agrobus::net;

    // ─── Heartbeat special sequence values (ISO 11783-7 §8) ──────────────────────
    namespace hb_seq {
        inline constexpr u8 INIT = 251;
        inline constexpr u8 RESERVED_LOW = 252;
        inline constexpr u8 RESERVED_HIGH = 253;
        inline constexpr u8 SENDER_ERROR = 254;
        inline constexpr u8 SHUTDOWN = 255;
        inline constexpr u8 MAX_NORMAL = 250;
    } // namespace hb_seq

    // ─── Heartbeat timing and recovery constants ─────────────────────────────────
    inline constexpr u32 HB_INTERVAL_MS = 100;
    inline constexpr u32 HB_COMM_ERROR_TIMEOUT_MS = 300;
    inline constexpr u8 HB_RECOVERY_COUNT = 8;
    inline constexpr u8 HB_MAX_JUMP = 3;

    // ─── Heartbeat receiver state ────────────────────────────────────────────────
    enum class HBReceiverState : u8 { Normal, SequenceError, CommError };

    // ─── Heartbeat sender (ISO 11783-7 §8) ───────────────────────────────────────
    // Initial message SHALL have sequence = 251.
    // Subsequent: 0, 1, 2, ... 250, 0, 1, ... (rollover)
    struct HeartbeatSender {
        u8 sequence_ = hb_seq::INIT;
        bool init_sent_ = false;
        bool special_pending_ = false; // One-shot special value pending
        u32 timer_ms_ = 0;

        // Get next sequence to send
        u8 next_sequence() {
            if (!init_sent_) {
                init_sent_ = true;
                sequence_ = hb_seq::INIT;
                return hb_seq::INIT;
            }
            // One-shot special values (error=254, shutdown=255)
            if (special_pending_) {
                special_pending_ = false;
                return sequence_; // Send the special value exactly once
            }
            // After init or special, sequence goes 0,1,...250,0,1,...
            if (sequence_ >= hb_seq::INIT) {
                // After INIT(251), ERROR(254), or SHUTDOWN(255), restart at 0
                sequence_ = 0;
            } else {
                sequence_ = (sequence_ >= hb_seq::MAX_NORMAL) ? 0 : sequence_ + 1;
            }
            return sequence_;
        }

        // Signal sender error (254) - sent exactly once on next heartbeat
        void signal_error() {
            sequence_ = hb_seq::SENDER_ERROR;
            special_pending_ = true;
        }

        // Signal graceful shutdown (255) - sent exactly once on next heartbeat
        void signal_shutdown() {
            sequence_ = hb_seq::SHUTDOWN;
            special_pending_ = true;
        }

        // Update timer, returns true when it's time to send
        bool update(u32 elapsed_ms) {
            timer_ms_ += elapsed_ms;
            if (timer_ms_ >= HB_INTERVAL_MS) {
                timer_ms_ -= HB_INTERVAL_MS;
                return true;
            }
            return false;
        }

        // Reset sender to initial state
        void reset() {
            sequence_ = hb_seq::INIT;
            init_sent_ = false;
            special_pending_ = false;
            timer_ms_ = 0;
        }
    };

    // ─── Heartbeat receiver state machine (ISO 11783-7 §8) ───────────────────────
    // States: Normal, SequenceError, CommError
    // Transitions:
    //   Normal -> SequenceError: repeated sequence or jump > 3
    //   Normal -> CommError: interval > 300 ms
    //   SequenceError -> Normal: 8 consecutive correct sequences
    //   SequenceError -> CommError: interval > 300 ms
    //   CommError -> Normal: on receiving any valid heartbeat
    struct HeartbeatReceiver {
        HBReceiverState state_ = HBReceiverState::Normal;
        u8 last_sequence_ = 0xFF;
        u8 recovery_counter_ = 0;
        u32 time_since_last_ms_ = 0;
        bool first_received_ = false;

        // Process a received sequence number
        void process(u8 sequence) {
            // Reset comm timer on any received message
            time_since_last_ms_ = 0;

            // 252/253: reserved, ignored by receiver
            if (sequence == hb_seq::RESERVED_LOW || sequence == hb_seq::RESERVED_HIGH) {
                return;
            }

            // 254: sender error indicator
            if (sequence == hb_seq::SENDER_ERROR) {
                on_sender_error.emit();
                return;
            }

            // 255: graceful shutdown
            if (sequence == hb_seq::SHUTDOWN) {
                on_shutdown_received.emit();
                return;
            }

            // Recover from CommError on any valid heartbeat
            if (state_ == HBReceiverState::CommError) {
                auto old_state = state_;
                state_ = HBReceiverState::Normal;
                recovery_counter_ = 0;
                last_sequence_ = sequence;
                on_state_change.emit(old_state, state_);
                echo::category("isobus.heartbeat.rx").debug("recovered from CommError");
                return;
            }

            if (!first_received_) {
                // First heartbeat ever - just record it
                first_received_ = true;
                last_sequence_ = sequence;
                return;
            }

            // 251 (INIT): Sender has reset. Synchronize and expect 0 next.
            // Per ISO 11783-7 §8.3.3: "the recipient shall recognize that the
            // transmitting CF has reset" and synchronize.
            if (sequence == hb_seq::INIT) {
                last_sequence_ = hb_seq::INIT; // Accept 0 as next valid sequence
                on_reset_received.emit();
                echo::category("isobus.heartbeat.rx").debug("sender reset detected (251)");
                return;
            }

            // Check for sequence errors
            bool is_error = false;

            // Check for repeated sequence (same as last, and not init 251)
            if (sequence == last_sequence_) {
                is_error = true;
            } else {
                // Check for jump > HB_MAX_JUMP
                // Normal sequence is 0..250, rolling over from 250 to 0
                u8 jump = compute_jump(last_sequence_, sequence);
                if (jump > HB_MAX_JUMP) {
                    is_error = true;
                }
            }

            if (state_ == HBReceiverState::Normal) {
                if (is_error) {
                    auto old_state = state_;
                    state_ = HBReceiverState::SequenceError;
                    recovery_counter_ = 0;
                    on_state_change.emit(old_state, state_);
                    echo::category("isobus.heartbeat.rx")
                        .warn("sequence error: last=", last_sequence_, " got=", sequence);
                }
            } else if (state_ == HBReceiverState::SequenceError) {
                if (!is_error) {
                    recovery_counter_++;
                    if (recovery_counter_ >= HB_RECOVERY_COUNT) {
                        auto old_state = state_;
                        state_ = HBReceiverState::Normal;
                        recovery_counter_ = 0;
                        on_state_change.emit(old_state, state_);
                        echo::category("isobus.heartbeat.rx")
                            .debug("recovered from SequenceError after ", HB_RECOVERY_COUNT, " good sequences");
                    }
                } else {
                    // Reset recovery counter on another error
                    recovery_counter_ = 0;
                }
            }

            last_sequence_ = sequence;
        }

        // Update timer, checks for comm timeout (>300ms)
        void update(u32 elapsed_ms) {
            if (!first_received_) {
                return; // Don't track timeout before first message
            }

            time_since_last_ms_ += elapsed_ms;

            if (time_since_last_ms_ > HB_COMM_ERROR_TIMEOUT_MS) {
                if (state_ != HBReceiverState::CommError) {
                    auto old_state = state_;
                    state_ = HBReceiverState::CommError;
                    recovery_counter_ = 0;
                    on_state_change.emit(old_state, state_);
                    echo::category("isobus.heartbeat.rx")
                        .warn("comm error: no heartbeat for ", time_since_last_ms_, "ms");
                }
            }
        }

        // Query state
        HBReceiverState state() const noexcept { return state_; }
        bool is_healthy() const noexcept { return state_ == HBReceiverState::Normal; }

        // Events
        Event<HBReceiverState, HBReceiverState> on_state_change; // (old_state, new_state)
        Event<> on_shutdown_received;
        Event<> on_sender_error;
        Event<> on_reset_received; // Sender reset detected (251)

      private:
        // Compute forward jump distance accounting for rollover at MAX_NORMAL+1 (251 values: 0..250)
        static u8 compute_jump(u8 from, u8 to) {
            // After INIT(251), the expected next is 0 -> jump of 1
            if (from == hb_seq::INIT) {
                return (to == 0) ? 1 : static_cast<u8>(to + 1);
            }
            // Both are in range 0..250
            if (to > from) {
                return to - from;
            }
            // Rollover: e.g. from=249, to=0 means jump of 2 (249->250->0)
            return static_cast<u8>((hb_seq::MAX_NORMAL + 1) - from + to);
        }
    };

    // ─── Heartbeat configuration ────────────────────────────────────────────────
    struct HeartbeatConfig {
        u32 interval_ms = HEARTBEAT_INTERVAL_MS;
        bool auto_enable = false;

        HeartbeatConfig &interval(u32 ms) {
            interval_ms = ms;
            return *this;
        }
        HeartbeatConfig &auto_start(bool enable = true) {
            auto_enable = enable;
            return *this;
        }
    };

    // ─── ISO 11783-7 Heartbeat protocol ─────────────────────────────────────────
    class HeartbeatProtocol {
        IsoNet &net_;
        InternalCF *cf_;
        u32 interval_ms_;
        u32 timer_ms_ = 0;
        HeartbeatSender sender_; // ISO-compliant sequence generator (init=251, then 0-250 rollover)
        bool enabled_ = false;

        // Remote heartbeat tracking
        struct RemoteHeartbeat {
            Address address = NULL_ADDRESS;
            u8 last_sequence = 0;
            u32 missed_count = 0;
            u32 timer_ms = 0;
        };
        dp::Vector<RemoteHeartbeat> remotes_;

      public:
        HeartbeatProtocol(IsoNet &net, InternalCF *cf, HeartbeatConfig config = {})
            : net_(net), cf_(cf), interval_ms_(config.interval_ms), enabled_(config.auto_enable) {}

        Result<void> initialize() {
            if (!cf_) {
                return Result<void>::err(Error::invalid_state("control function not set"));
            }
            net_.register_pgn_callback(PGN_HEARTBEAT, [this](const Message &msg) { handle_heartbeat(msg); });
            echo::category("isobus.heartbeat").debug("initialized");
            return {};
        }

        void enable() noexcept { enabled_ = true; }
        void disable() noexcept { enabled_ = false; }
        bool is_enabled() const noexcept { return enabled_; }

        // Signal sender error (sequence=254) on next heartbeat
        void signal_error() { sender_.signal_error(); }

        // Signal graceful shutdown (sequence=255) on next heartbeat
        void signal_shutdown() { sender_.signal_shutdown(); }

        // Reset sender to initial state (will send 251 on next heartbeat)
        void reset_sender() { sender_.reset(); }

        void set_interval(u32 ms) noexcept { interval_ms_ = ms; }
        u32 interval() const noexcept { return interval_ms_; }

        void update(u32 elapsed_ms) {
            if (enabled_) {
                timer_ms_ += elapsed_ms;
                if (timer_ms_ >= interval_ms_) {
                    timer_ms_ -= interval_ms_;
                    send_heartbeat();
                }
            }

            // Check remote heartbeats
            for (auto &remote : remotes_) {
                remote.timer_ms += elapsed_ms;
                if (remote.timer_ms >= interval_ms_ * 3) {
                    // Missed heartbeat
                    remote.missed_count++;
                    remote.timer_ms = 0;
                    on_heartbeat_missed.emit(remote.address, remote.missed_count);
                    echo::category("isobus.heartbeat")
                        .warn("peer timeout: addr=", remote.address, " count=", remote.missed_count);
                }
            }
        }

        // Track a remote device's heartbeat
        Result<void> track(Address address) {
            for (const auto &r : remotes_) {
                if (r.address == address)
                    return {};
            }
            remotes_.push_back({address, 0, 0, 0});
            echo::category("isobus.heartbeat").debug("tracking peer: addr=", address);
            return {};
        }

        Result<void> untrack(Address address) {
            for (auto it = remotes_.begin(); it != remotes_.end(); ++it) {
                if (it->address == address) {
                    remotes_.erase(it);
                    echo::category("isobus.heartbeat").debug("untracking peer: addr=", address);
                    return {};
                }
            }
            return Result<void>::err(Error::invalid_state("address not tracked"));
        }

        // Events
        Event<Address, u8> on_heartbeat_received; // (source, sequence)
        Event<Address, u32> on_heartbeat_missed;  // (source, missed_count)

      private:
        void send_heartbeat() {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = sender_.next_sequence();
            net_.send(PGN_HEARTBEAT, data, cf_, nullptr, Priority::Default);
            echo::category("isobus.heartbeat").trace("heartbeat sent: seq=", data[0]);
        }

        void handle_heartbeat(const Message &msg) {
            u8 seq = msg.data[0];
            on_heartbeat_received.emit(msg.source, seq);

            for (auto &remote : remotes_) {
                if (remote.address == msg.source) {
                    remote.last_sequence = seq;
                    remote.missed_count = 0;
                    remote.timer_ms = 0;
                    return;
                }
            }
        }
    };
} // namespace agrobus::j1939
