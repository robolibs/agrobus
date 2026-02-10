#pragma once

#include <agrobus/net/constants.hpp>
#include <agrobus/net/event.hpp>
#include <agrobus/net/message.hpp>
#include <agrobus/net/network_manager.hpp>
#include <agrobus/net/types.hpp>
#include <datapod/datapod.hpp>
#include <echo/echo.hpp>

namespace agrobus::j1939 {
    using namespace agrobus::net;

    // ─── Shortcut button states ──────────────────────────────────────────────────
    enum class ShortcutButtonState : u8 { Released = 0, Pressed = 1, Error = 2, NotAvailable = 3 };

    // ─── Shortcut button interface ───────────────────────────────────────────────
    class ShortcutButtonInterface {
        IsoNet &net_;
        InternalCF *cf_;
        ShortcutButtonState current_state_ = ShortcutButtonState::Released;

      public:
        ShortcutButtonInterface(IsoNet &net, InternalCF *cf) : net_(net), cf_(cf) {}

        Result<void> initialize() {
            if (!cf_) {
                return Result<void>::err(Error::invalid_state("control function not set"));
            }
            net_.register_pgn_callback(PGN_SHORTCUT_BUTTON,
                                       [this](const Message &msg) { handle_shortcut_button(msg); });
            echo::category("isobus.shortcut").debug("initialized");
            return {};
        }

        ShortcutButtonState state() const noexcept { return current_state_; }
        bool is_pressed() const noexcept { return current_state_ == ShortcutButtonState::Pressed; }

        Result<void> send(ShortcutButtonState state) {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = (static_cast<u8>(state) & 0x03);
            return net_.send(PGN_SHORTCUT_BUTTON, data, cf_);
        }

        Event<ShortcutButtonState> on_state_change;

      private:
        void handle_shortcut_button(const Message &msg) {
            echo::category("isobus.shortcut").trace("shortcut button event");
            if (msg.data.size() < 1)
                return;
            auto new_state = static_cast<ShortcutButtonState>(msg.data[0] & 0x03);
            if (new_state != current_state_) {
                current_state_ = new_state;
                on_state_change.emit(new_state);
            }
        }
    };
} // namespace agrobus::j1939
