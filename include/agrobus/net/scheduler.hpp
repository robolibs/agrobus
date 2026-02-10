#pragma once

#include "timer.hpp"
#include <agrobus/net/types.hpp>
#include <datapod/datapod.hpp>
#include <functional>

namespace agrobus::net {

    // ─── Periodic task scheduler ──────────────────────────────────────────────────
    // Lightweight scheduling mechanism for periodic messages and retries.
    // Similar in spirit to AgIsoStack++'s ProcessingFlags.

    struct PeriodicTask {
        dp::String name;
        u32 interval_ms = 0;
        u32 elapsed_ms = 0;
        bool enabled = true;
        u8 max_retries = 0; // 0 = unlimited
        u8 retry_count = 0;
        std::function<bool()> callback; // Returns true if task completed, false to retry

        bool due() const noexcept { return enabled && elapsed_ms >= interval_ms; }
    };

    class Scheduler {
        dp::Vector<PeriodicTask> tasks_;

      public:
        Scheduler() = default;

        // Add a periodic task. Returns the task index.
        usize add(dp::String name, u32 interval_ms, std::function<bool()> callback, u8 max_retries = 0) {
            PeriodicTask task;
            task.name = std::move(name);
            task.interval_ms = interval_ms;
            task.callback = std::move(callback);
            task.max_retries = max_retries;
            tasks_.push_back(std::move(task));
            return tasks_.size() - 1;
        }

        // Enable/disable a task by index
        void enable(usize index, bool enabled = true) {
            if (index < tasks_.size()) {
                tasks_[index].enabled = enabled;
                if (enabled) {
                    tasks_[index].elapsed_ms = 0;
                    tasks_[index].retry_count = 0;
                }
            }
        }

        void disable(usize index) { enable(index, false); }

        // Set a flag to immediately trigger a task on next update
        void trigger(usize index) {
            if (index < tasks_.size()) {
                tasks_[index].elapsed_ms = tasks_[index].interval_ms;
            }
        }

        // Update all tasks
        void update(u32 elapsed_ms) {
            for (auto &task : tasks_) {
                if (!task.enabled)
                    continue;

                task.elapsed_ms += elapsed_ms;
                if (task.due()) {
                    task.elapsed_ms = 0;
                    if (task.callback) {
                        bool completed = task.callback();
                        if (!completed) {
                            task.retry_count++;
                            if (task.max_retries > 0 && task.retry_count >= task.max_retries) {
                                task.enabled = false;
                            }
                        } else {
                            task.retry_count = 0;
                        }
                    }
                }
            }
        }

        usize count() const noexcept { return tasks_.size(); }
        bool is_enabled(usize index) const noexcept { return index < tasks_.size() && tasks_[index].enabled; }

        void clear() { tasks_.clear(); }
    };

    // ─── Processing flags (bit-based task triggering) ─────────────────────────────
    // Simple flag-based scheduler: set a flag, process clears it.
    // Each flag maps to a callback that runs when the flag is set.
    class ProcessingFlags {
        u32 flags_ = 0;
        dp::Vector<std::function<void()>> handlers_;

      public:
        static constexpr u32 MAX_FLAGS = 32;

        ProcessingFlags() = default;

        // Register a handler for a flag index (0-31)
        void register_flag(u8 index, std::function<void()> handler) {
            while (handlers_.size() <= index) {
                handlers_.push_back(nullptr);
            }
            handlers_[index] = std::move(handler);
        }

        // Set a flag (will be processed on next update)
        void set(u8 index) noexcept {
            if (index < MAX_FLAGS) {
                flags_ |= (1U << index);
            }
        }

        // Clear a specific flag
        void clear(u8 index) noexcept {
            if (index < MAX_FLAGS) {
                flags_ &= ~(1U << index);
            }
        }

        // Check if a flag is set
        bool is_set(u8 index) const noexcept {
            if (index >= MAX_FLAGS)
                return false;
            return (flags_ & (1U << index)) != 0;
        }

        // Process all set flags, clearing them after processing
        void process() {
            for (u8 i = 0; i < MAX_FLAGS && i < handlers_.size(); ++i) {
                if ((flags_ & (1U << i)) && handlers_[i]) {
                    flags_ &= ~(1U << i);
                    handlers_[i]();
                }
            }
        }

        u32 pending() const noexcept { return flags_; }
        bool any_pending() const noexcept { return flags_ != 0; }
    };
} // namespace agrobus::net
