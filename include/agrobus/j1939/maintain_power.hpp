#pragma once

#include <agrobus/net/constants.hpp>
#include <agrobus/net/event.hpp>
#include <agrobus/net/internal_cf.hpp>
#include <agrobus/net/message.hpp>
#include <agrobus/net/network_manager.hpp>
#include <agrobus/net/types.hpp>
#include <datapod/datapod.hpp>
#include <echo/echo.hpp>

namespace agrobus::j1939 {
    using namespace agrobus::net;

    // ─── Maintain power states ───────────────────────────────────────────────────
    enum class KeySwitchState : u8 { Off = 0, NotOff = 1, Error = 2, NotAvailable = 3 };

    enum class MaintainPowerRequest : u8 { NoRequest = 0, ECURequest = 1, Error = 2, NotAvailable = 3 };

    // ─── Maintain power data ─────────────────────────────────────────────────────
    struct MaintainPowerData {
        KeySwitchState key_switch = KeySwitchState::NotAvailable;
        MaintainPowerRequest maintain_request = MaintainPowerRequest::NotAvailable;
        u8 max_time_min = 0xFF; // Maximum time of tractor power remaining (minutes, 0xFF=N/A)
        u64 timestamp_us = 0;

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = (static_cast<u8>(key_switch) & 0x03) | ((static_cast<u8>(maintain_request) & 0x03) << 2);
            data[1] = max_time_min;
            return data;
        }

        static MaintainPowerData decode(const dp::Vector<u8> &data) {
            MaintainPowerData mpd;
            if (data.size() >= 2) {
                mpd.key_switch = static_cast<KeySwitchState>(data[0] & 0x03);
                mpd.maintain_request = static_cast<MaintainPowerRequest>((data[0] >> 2) & 0x03);
                mpd.max_time_min = data[1];
            }
            return mpd;
        }
    };

    // ─── Maintain power interface ────────────────────────────────────────────────
    class MaintainPowerInterface {
        IsoNet &net_;
        InternalCF *cf_;
        dp::Optional<MaintainPowerData> latest_;

      public:
        MaintainPowerInterface(IsoNet &net, InternalCF *cf) : net_(net), cf_(cf) {}

        Result<void> initialize() {
            if (!cf_) {
                return Result<void>::err(Error::invalid_state("control function not set"));
            }
            net_.register_pgn_callback(PGN_MAINTAIN_POWER, [this](const Message &msg) { handle_maintain_power(msg); });
            echo::category("isobus.power").debug("initialized");
            return {};
        }

        dp::Optional<MaintainPowerData> latest() const noexcept { return latest_; }

        Result<void> send(const MaintainPowerData &mpd) { return net_.send(PGN_MAINTAIN_POWER, mpd.encode(), cf_); }

        Event<const MaintainPowerData &> on_maintain_power;

      private:
        void handle_maintain_power(const Message &msg) {
            echo::category("isobus.power").trace("maintain power received");
            if (msg.data.size() < 2)
                return;

            MaintainPowerData mpd = MaintainPowerData::decode(msg.data);
            mpd.timestamp_us = msg.timestamp_us;
            latest_ = mpd;
            on_maintain_power.emit(mpd);
        }
    };

    // ─── Power Management State Machine (ISO 11783-9 §4.6) ──────────────────────
    // Implements the key-off power management sequence:
    //   Running → ShutdownPending (2s min) → Maintaining (up to 3 min) → PowerOff
    //
    // For TECU: manages power relay timing based on CF requests.
    // For CF: requests power extension and monitors shutdown state.

    enum class PowerState : u8 {
        Running,         // Key switch is on
        ShutdownPending, // Key-off detected, minimum 2s hold
        Maintaining,     // CFs have requested power extension
        PowerOff         // Power should be cut
    };

    class PowerManager {
        IsoNet &net_;
        InternalCF *cf_;
        PowerState state_ = PowerState::Running;
        u32 shutdown_timer_ms_ = 0;     // Time since key-off
        u32 maintain_timer_ms_ = 0;     // Time since last maintain request received
        u32 broadcast_timer_ms_ = 0;    // Timer for periodic broadcasts
        u32 request_timer_ms_ = 0;      // Timer for sending maintain requests
        bool is_tecu_ = false;          // True if acting as power source (TECU)
        bool requesting_power_ = false; // True if this CF needs power extension

      public:
        PowerManager(IsoNet &net, InternalCF *cf, bool is_tecu = false) : net_(net), cf_(cf), is_tecu_(is_tecu) {}

        Result<void> initialize() {
            if (!cf_) {
                return Result<void>::err(Error::invalid_state("control function not set"));
            }
            net_.register_pgn_callback(PGN_MAINTAIN_POWER, [this](const Message &msg) { handle_message(msg); });
            echo::category("isobus.power").debug("power manager initialized, tecu=", is_tecu_);
            return {};
        }

        PowerState state() const noexcept { return state_; }

        // ─── TECU: Signal key-off event ─────────────────────────────────────────
        void key_off() {
            if (state_ == PowerState::Running) {
                state_ = PowerState::ShutdownPending;
                shutdown_timer_ms_ = 0;
                maintain_timer_ms_ = 0;
                echo::category("isobus.power").info("key-off detected, shutdown pending");
                on_state_change.emit(PowerState::ShutdownPending);
            }
        }

        // ─── TECU: Signal key-on event (abort shutdown) ─────────────────────────
        void key_on() {
            if (state_ != PowerState::Running) {
                state_ = PowerState::Running;
                shutdown_timer_ms_ = 0;
                echo::category("isobus.power").info("key-on, power restored");
                on_state_change.emit(PowerState::Running);
            }
        }

        // ─── CF: Request power extension ────────────────────────────────────────
        void request_power(bool need_power) {
            requesting_power_ = need_power;
            if (need_power) {
                request_timer_ms_ = POWER_MAINTAIN_REPEAT_MS; // Send immediately
            }
        }

        // ─── Update (call periodically with elapsed time) ───────────────────────
        dp::Vector<Frame> update(u32 elapsed_ms) {
            dp::Vector<Frame> frames;

            if (is_tecu_) {
                update_tecu(elapsed_ms);
            } else {
                update_cf(elapsed_ms);
            }

            return frames;
        }

        // Events
        Event<PowerState> on_state_change;
        Event<> on_power_off; // Final power-off signal (TECU only)

      private:
        void update_tecu(u32 elapsed_ms) {
            broadcast_timer_ms_ += elapsed_ms;

            // Broadcast key switch state every 100ms
            if (broadcast_timer_ms_ >= HEARTBEAT_INTERVAL_MS) {
                broadcast_timer_ms_ = 0;
                MaintainPowerData mpd;
                mpd.key_switch = (state_ == PowerState::Running) ? KeySwitchState::NotOff : KeySwitchState::Off;
                mpd.maintain_request = MaintainPowerRequest::NoRequest;
                if (state_ != PowerState::Running && state_ != PowerState::PowerOff) {
                    u32 remaining_ms = POWER_MAX_EXTENSION_MS - shutdown_timer_ms_;
                    mpd.max_time_min = static_cast<u8>(remaining_ms / 60000);
                }
                net_.send(PGN_MAINTAIN_POWER, mpd.encode(), cf_);
            }

            if (state_ == PowerState::ShutdownPending) {
                shutdown_timer_ms_ += elapsed_ms;
                if (shutdown_timer_ms_ >= POWER_SHUTDOWN_MIN_MS) {
                    // Minimum hold time elapsed
                    if (maintain_timer_ms_ > POWER_MAINTAIN_REPEAT_MS * 2) {
                        // No recent maintain requests - power off
                        state_ = PowerState::PowerOff;
                        echo::category("isobus.power").info("no maintain requests, power off");
                        on_state_change.emit(PowerState::PowerOff);
                        on_power_off.emit();
                    } else {
                        // Transition to maintaining
                        state_ = PowerState::Maintaining;
                        echo::category("isobus.power").info("maintain requests active, extending power");
                        on_state_change.emit(PowerState::Maintaining);
                    }
                }
            } else if (state_ == PowerState::Maintaining) {
                shutdown_timer_ms_ += elapsed_ms;
                maintain_timer_ms_ += elapsed_ms;

                if (shutdown_timer_ms_ >= POWER_MAX_EXTENSION_MS) {
                    // Maximum extension reached
                    state_ = PowerState::PowerOff;
                    echo::category("isobus.power").warn("max power extension reached, forcing off");
                    on_state_change.emit(PowerState::PowerOff);
                    on_power_off.emit();
                } else if (maintain_timer_ms_ > POWER_MAINTAIN_REPEAT_MS * 2) {
                    // No recent maintain requests - power off
                    state_ = PowerState::PowerOff;
                    echo::category("isobus.power").info("maintain requests stopped, power off");
                    on_state_change.emit(PowerState::PowerOff);
                    on_power_off.emit();
                }
            }
        }

        void update_cf(u32 elapsed_ms) {
            if (!requesting_power_)
                return;

            request_timer_ms_ += elapsed_ms;
            if (request_timer_ms_ >= POWER_MAINTAIN_REPEAT_MS) {
                request_timer_ms_ = 0;
                MaintainPowerData mpd;
                mpd.key_switch = KeySwitchState::NotAvailable;
                mpd.maintain_request = MaintainPowerRequest::ECURequest;
                net_.send(PGN_MAINTAIN_POWER, mpd.encode(), cf_);
                echo::category("isobus.power").trace("sent maintain power request");
            }
        }

        void handle_message(const Message &msg) {
            if (msg.data.size() < 2)
                return;
            MaintainPowerData mpd = MaintainPowerData::decode(msg.data);
            mpd.timestamp_us = msg.timestamp_us;

            if (is_tecu_) {
                // TECU: track incoming maintain power requests from CFs
                if (mpd.maintain_request == MaintainPowerRequest::ECURequest) {
                    maintain_timer_ms_ = 0; // Reset maintain timeout
                    echo::category("isobus.power").trace("maintain request from SA=", msg.source);
                }
            } else {
                // CF: track key switch state from TECU
                if (mpd.key_switch == KeySwitchState::Off && state_ == PowerState::Running) {
                    state_ = PowerState::ShutdownPending;
                    on_state_change.emit(PowerState::ShutdownPending);
                    echo::category("isobus.power").info("key-off from TECU, shutdown pending");
                } else if (mpd.key_switch == KeySwitchState::NotOff && state_ != PowerState::Running) {
                    state_ = PowerState::Running;
                    on_state_change.emit(PowerState::Running);
                    echo::category("isobus.power").info("key-on from TECU, power restored");
                }
            }
        }
    };
} // namespace agrobus::j1939
