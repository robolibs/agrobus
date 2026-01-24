#pragma once

#include <agrobus/net/constants.hpp>
#include <agrobus/net/error.hpp>
#include <agrobus/net/event.hpp>
#include <agrobus/net/internal_cf.hpp>
#include <agrobus/net/message.hpp>
#include <agrobus/net/network_manager.hpp>
#include <agrobus/net/types.hpp>
#include <datapod/datapod.hpp>
#include <echo/echo.hpp>

namespace agrobus::isobus {
    using namespace agrobus::net;

    // ─── ISO 11783-11 Auxiliary Functions ────────────────────────────────────────

    enum class AuxFunctionType : u8 {
        Type0 = 0, // On/Off (boolean)
        Type1 = 1, // Variable speed (analog)
        Type2 = 2  // Variable position (analog)
    };

    enum class AuxFunctionState : u8 { Off = 0, On = 1, Variable = 2 };

    // ─── AUX-O (Old-style) ───────────────────────────────────────────────────────
    struct AuxOFunction {
        u8 function_number = 0;
        AuxFunctionType type = AuxFunctionType::Type0;
        AuxFunctionState state = AuxFunctionState::Off;
        u16 setpoint = 0; // For Type1/2: 0-10000 (0.0%-100.0%)
    };

    // ─── AUX-N (New-style, ISO 11783-6 Annex G) ─────────────────────────────────
    struct AuxNFunction {
        u8 function_number = 0;
        AuxFunctionType type = AuxFunctionType::Type0;
        AuxFunctionState state = AuxFunctionState::Off;
        u16 setpoint = 0; // For Type1/2: 0-65535
    };

    // ─── AUX Config ──────────────────────────────────────────────────────────────
    struct AuxConfig {
        bool auto_send_status = false;
        u32 status_interval_ms = 100;

        AuxConfig &auto_send(bool enable) {
            auto_send_status = enable;
            return *this;
        }
        AuxConfig &interval(u32 ms) {
            status_interval_ms = ms;
            return *this;
        }
    };

    // ─── AUX-O Interface ─────────────────────────────────────────────────────────
    class AuxOInterface {
        IsoNet &net_;
        InternalCF *cf_;
        AuxConfig config_;
        dp::Vector<AuxOFunction> functions_;

      public:
        AuxOInterface(IsoNet &net, InternalCF *cf, AuxConfig config = {}) : net_(net), cf_(cf), config_(config) {}

        Result<void> initialize() {
            if (!cf_) {
                return Result<void>::err(Error::invalid_state("control function not set"));
            }
            net_.register_pgn_callback(PGN_AUX_INPUT_STATUS, [this](const Message &msg) { handle_input(msg); });
            echo::category("isobus.protocol.aux_o").debug("initialized");
            return {};
        }

        Result<void> add_function(u8 number, AuxFunctionType type) {
            for (auto &f : functions_) {
                if (f.function_number == number) {
                    f.type = type;
                    return {};
                }
            }
            functions_.push_back({number, type, AuxFunctionState::Off, 0});
            return {};
        }

        void set_function(u8 number, AuxFunctionType type, u16 setpoint) {
            for (auto &f : functions_) {
                if (f.function_number == number) {
                    f.type = type;
                    auto old_setpoint = f.setpoint;
                    f.setpoint = setpoint;
                    f.state = (setpoint > 0) ? AuxFunctionState::On : AuxFunctionState::Off;
                    if (f.type == AuxFunctionType::Type1 || f.type == AuxFunctionType::Type2) {
                        f.state = AuxFunctionState::Variable;
                    }
                    if (old_setpoint != setpoint) {
                        on_function_changed.emit(f);
                    }
                    return;
                }
            }
            AuxOFunction func{number, type, AuxFunctionState::Off, setpoint};
            if (setpoint > 0) {
                func.state = AuxFunctionState::On;
            }
            functions_.push_back(func);
            on_function_changed.emit(functions_.back());
        }

        dp::Optional<AuxOFunction> get_function(u8 number) const {
            for (const auto &f : functions_) {
                if (f.function_number == number)
                    return f;
            }
            return dp::nullopt;
        }

        const dp::Vector<AuxOFunction> &functions() const noexcept { return functions_; }

        Result<void> send_status(u8 function_number) {
            echo::category("isobus.protocol.aux_o").debug("sending status for func=", function_number);
            auto func = get_function(function_number);
            if (!func.has_value()) {
                return Result<void>::err(Error::invalid_state("function not found"));
            }

            dp::Vector<u8> data(8, 0xFF);
            data[0] = func->function_number;
            data[1] = static_cast<u8>(func->type);
            data[2] = static_cast<u8>(func->state);
            data[3] = static_cast<u8>(func->setpoint & 0xFF);
            data[4] = static_cast<u8>((func->setpoint >> 8) & 0xFF);
            return net_.send(PGN_AUX_INPUT_STATUS, data, cf_, nullptr, Priority::Default);
        }

        // Events
        Event<const AuxOFunction &> on_function_changed;

        void update(u32 /*elapsed_ms*/) {}

      private:
        void handle_input(const Message &msg) {
            if (msg.data.size() < 5)
                return;

            u8 func_num = msg.data[0];
            auto type = static_cast<AuxFunctionType>(msg.data[1]);
            u16 setpoint = static_cast<u16>(msg.data[3]) | (static_cast<u16>(msg.data[4]) << 8);

            set_function(func_num, type, setpoint);
            echo::category("isobus.protocol.aux_o").trace("AUX-O input: func=", func_num, " setpoint=", setpoint);
        }
    };

    // ─── AUX-N Interface ─────────────────────────────────────────────────────────
    class AuxNInterface {
        IsoNet &net_;
        InternalCF *cf_;
        AuxConfig config_;
        dp::Vector<AuxNFunction> functions_;

      public:
        AuxNInterface(IsoNet &net, InternalCF *cf, AuxConfig config = {}) : net_(net), cf_(cf), config_(config) {}

        Result<void> initialize() {
            if (!cf_) {
                return Result<void>::err(Error::invalid_state("control function not set"));
            }
            net_.register_pgn_callback(PGN_AUX_INPUT_TYPE2, [this](const Message &msg) { handle_input(msg); });
            echo::category("isobus.protocol.aux_n").debug("initialized");
            return {};
        }

        Result<void> add_function(u8 number, AuxFunctionType type) {
            for (auto &f : functions_) {
                if (f.function_number == number) {
                    f.type = type;
                    return {};
                }
            }
            functions_.push_back({number, type, AuxFunctionState::Off, 0});
            return {};
        }

        void set_function(u8 number, AuxFunctionType type, u16 setpoint) {
            for (auto &f : functions_) {
                if (f.function_number == number) {
                    f.type = type;
                    auto old_setpoint = f.setpoint;
                    f.setpoint = setpoint;
                    if (type == AuxFunctionType::Type0) {
                        f.state = (setpoint > 0) ? AuxFunctionState::On : AuxFunctionState::Off;
                    } else {
                        f.state = AuxFunctionState::Variable;
                    }
                    if (old_setpoint != setpoint) {
                        on_function_changed.emit(f);
                    }
                    return;
                }
            }
            AuxFunctionState state = AuxFunctionState::Off;
            if (type == AuxFunctionType::Type0) {
                state = (setpoint > 0) ? AuxFunctionState::On : AuxFunctionState::Off;
            } else {
                state = AuxFunctionState::Variable;
            }
            functions_.push_back({number, type, state, setpoint});
            on_function_changed.emit(functions_.back());
        }

        dp::Optional<AuxNFunction> get_function(u8 number) const {
            for (const auto &f : functions_) {
                if (f.function_number == number)
                    return f;
            }
            return dp::nullopt;
        }

        const dp::Vector<AuxNFunction> &functions() const noexcept { return functions_; }

        Result<void> send_status(u8 function_number) {
            echo::category("isobus.protocol.aux_n").debug("sending status for func=", function_number);
            auto func = get_function(function_number);
            if (!func.has_value()) {
                return Result<void>::err(Error::invalid_state("function not found"));
            }

            dp::Vector<u8> data(8, 0xFF);
            data[0] = func->function_number;
            data[1] = static_cast<u8>(func->type);
            data[2] = static_cast<u8>(func->state);
            data[3] = static_cast<u8>(func->setpoint & 0xFF);
            data[4] = static_cast<u8>((func->setpoint >> 8) & 0xFF);
            return net_.send(PGN_AUX_INPUT_TYPE2, data, cf_, nullptr, Priority::Default);
        }

        // Events
        Event<const AuxNFunction &> on_function_changed;

        void update(u32 /*elapsed_ms*/) {}

      private:
        void handle_input(const Message &msg) {
            if (msg.data.size() < 5)
                return;

            u8 func_num = msg.data[0];
            auto type = static_cast<AuxFunctionType>(msg.data[1]);
            u16 setpoint = static_cast<u16>(msg.data[3]) | (static_cast<u16>(msg.data[4]) << 8);

            set_function(func_num, type, setpoint);
            echo::category("isobus.protocol.aux_n").trace("AUX-N input: func=", func_num, " setpoint=", setpoint);
        }
    };
} // namespace agrobus::isobus
