#pragma once

#include <agrobus/net/types.hpp>
#include <datapod/datapod.hpp>

namespace agrobus::net {

    // ─── Timer utility for periodic operations ───────────────────────────────────
    class Timer {
        u32 interval_ms_ = 0;
        u32 elapsed_ms_ = 0;
        bool running_ = false;
        bool auto_reset_ = true;

      public:
        Timer() = default;
        explicit Timer(u32 interval_ms, bool auto_reset = true) : interval_ms_(interval_ms), auto_reset_(auto_reset) {}

        void set_interval(u32 ms) noexcept { interval_ms_ = ms; }
        u32 interval() const noexcept { return interval_ms_; }

        void start() noexcept {
            running_ = true;
            elapsed_ms_ = 0;
        }

        void stop() noexcept { running_ = false; }
        void reset() noexcept { elapsed_ms_ = 0; }

        bool running() const noexcept { return running_; }

        // Returns true if the timer expired during this update
        bool update(u32 delta_ms) noexcept {
            if (!running_ || interval_ms_ == 0)
                return false;

            elapsed_ms_ += delta_ms;
            if (elapsed_ms_ >= interval_ms_) {
                if (auto_reset_) {
                    elapsed_ms_ -= interval_ms_;
                } else {
                    running_ = false;
                }
                return true;
            }
            return false;
        }

        u32 elapsed() const noexcept { return elapsed_ms_; }
        u32 remaining() const noexcept {
            if (!running_ || elapsed_ms_ >= interval_ms_)
                return 0;
            return interval_ms_ - elapsed_ms_;
        }

        f32 progress() const noexcept {
            if (interval_ms_ == 0)
                return 0.0f;
            return static_cast<f32>(elapsed_ms_) / static_cast<f32>(interval_ms_);
        }

        bool expired() const noexcept { return running_ && elapsed_ms_ >= interval_ms_; }
    };

    // ─── One-shot timeout ─────────────────────────────────────────────────────────
    class Timeout {
        u32 timeout_ms_ = 0;
        u32 elapsed_ms_ = 0;
        bool active_ = false;

      public:
        Timeout() = default;
        explicit Timeout(u32 timeout_ms) : timeout_ms_(timeout_ms) {}

        void start(u32 timeout_ms) noexcept {
            timeout_ms_ = timeout_ms;
            elapsed_ms_ = 0;
            active_ = true;
        }

        void start() noexcept {
            elapsed_ms_ = 0;
            active_ = true;
        }

        void cancel() noexcept { active_ = false; }

        bool update(u32 delta_ms) noexcept {
            if (!active_)
                return false;
            elapsed_ms_ += delta_ms;
            if (elapsed_ms_ >= timeout_ms_) {
                active_ = false;
                return true; // Timed out
            }
            return false;
        }

        bool active() const noexcept { return active_; }
        bool timed_out() const noexcept { return !active_ && elapsed_ms_ >= timeout_ms_; }
        u32 elapsed() const noexcept { return elapsed_ms_; }
    };
} // namespace agrobus::net
