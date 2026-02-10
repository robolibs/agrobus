#pragma once

#include <agrobus/net/types.hpp>
#include <datapod/datapod.hpp>
#include <functional>

namespace agrobus::net {

    // ─── Listener token for unsubscription ────────────────────────────────────────
    using ListenerToken = u32;
    inline constexpr ListenerToken INVALID_TOKEN = 0;

    // ─── Type-safe event dispatcher ──────────────────────────────────────────────
    // Supports:
    //   - subscribe/unsubscribe with tokens
    //   - safe removal during dispatch (deferred)
    //   - operator+= for convenience
    template <typename... Args> class Event {
        struct Listener {
            ListenerToken token = 0;
            std::function<void(Args...)> fn;
            bool pending_remove = false;
        };

        dp::Vector<Listener> listeners_;
        ListenerToken next_token_ = 1;
        bool dispatching_ = false;

      public:
        // Subscribe and get a token for later removal
        ListenerToken subscribe(std::function<void(Args...)> fn) {
            ListenerToken token = next_token_++;
            listeners_.push_back({token, std::move(fn), false});
            return token;
        }

        // Remove a listener by token
        bool unsubscribe(ListenerToken token) {
            for (auto it = listeners_.begin(); it != listeners_.end(); ++it) {
                if (it->token == token) {
                    if (dispatching_) {
                        // Defer removal until dispatch is complete
                        it->pending_remove = true;
                    } else {
                        listeners_.erase(it);
                    }
                    return true;
                }
            }
            return false;
        }

        void emit(Args... args) {
            dispatching_ = true;
            for (auto &listener : listeners_) {
                if (!listener.pending_remove && listener.fn) {
                    listener.fn(args...);
                }
            }
            dispatching_ = false;

            // Clean up deferred removals
            for (auto it = listeners_.begin(); it != listeners_.end();) {
                if (it->pending_remove) {
                    it = listeners_.erase(it);
                } else {
                    ++it;
                }
            }
        }

        dp::usize count() const noexcept {
            dp::usize active = 0;
            for (const auto &l : listeners_) {
                if (!l.pending_remove)
                    active++;
            }
            return active;
        }

        void clear() { listeners_.clear(); }

        ListenerToken operator+=(std::function<void(Args...)> fn) { return subscribe(std::move(fn)); }
    };
} // namespace agrobus::net
