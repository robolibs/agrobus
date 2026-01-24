#pragma once

#include "ddop.hpp"
#include "objects.hpp"
#include <agrobus/net/constants.hpp>
#include <agrobus/net/error.hpp>
#include <agrobus/net/event.hpp>
#include <agrobus/net/internal_cf.hpp>
#include <agrobus/net/message.hpp>
#include <agrobus/net/network_manager.hpp>
#include <agrobus/net/state_machine.hpp>
#include <datapod/datapod.hpp>
#include <echo/echo.hpp>
#include <functional>

namespace agrobus::isobus::tc {
    using namespace agrobus::net;

    // ─── TC Client Config ────────────────────────────────────────────────────────
    struct TCClientConfig {
        u32 timeout_ms = 6000;

        TCClientConfig &timeout(u32 ms) {
            timeout_ms = ms;
            return *this;
        }
    };

    // ─── TC Client state ─────────────────────────────────────────────────────────
    enum class TCState {
        Disconnected,
        WaitForStartup,
        WaitForServerStatus,
        SendWorkingSetMaster,
        RequestVersion,
        WaitForVersion,
        ProcessDDOP,
        TransferDDOP,
        WaitForPoolResponse,
        ActivatePool,
        WaitForActivation,
        Connected
    };

    // ─── TC command types ────────────────────────────────────────────────────────
    namespace tc_cmd {
        inline constexpr u8 VERSION_REQUEST = 0x00;
        inline constexpr u8 VERSION_RESPONSE = 0x10;
        inline constexpr u8 STRUCTURE_LABEL = 0x01;
        inline constexpr u8 LOCALIZATION_LABEL = 0x21;
        inline constexpr u8 REQUEST_OBJECT_POOL = 0x41;
        inline constexpr u8 OBJECT_POOL_TRANSFER = 0x11;
        inline constexpr u8 OBJECT_POOL_RESPONSE = 0x12;
        inline constexpr u8 ACTIVATE_POOL = 0x22;
        inline constexpr u8 ACTIVATE_RESPONSE = 0x23;
        inline constexpr u8 DELETE_POOL = 0x32;
        inline constexpr u8 PROCESS_DATA = 0x03;
        inline constexpr u8 SET_VALUE = 0x24;
        inline constexpr u8 REQUEST_VALUE = 0x04;
        inline constexpr u8 VALUE_RESPONSE = 0x05;
        inline constexpr u8 TC_STATUS = 0xFE;
    } // namespace tc_cmd

    // ─── Task Controller Client ──────────────────────────────────────────────────
    class TaskControllerClient {
        IsoNet &net_;
        InternalCF *cf_;
        TCClientConfig config_;
        StateMachine<TCState> state_{TCState::Disconnected};
        DDOP ddop_;
        u32 timer_ms_ = 0;
        u8 tc_address_ = NULL_ADDRESS;
        u8 tc_version_ = 0;
        u8 num_booms_ = 0;
        u8 num_sections_ = 0;

        // Callbacks
        using ValueCallback = std::function<Result<i32>(ElementNumber, DDI)>;
        using CommandCallback = std::function<Result<void>(ElementNumber, DDI, i32)>;
        ValueCallback value_callback_;
        CommandCallback command_callback_;

      public:
        TaskControllerClient(IsoNet &net, InternalCF *cf, TCClientConfig config = {})
            : net_(net), cf_(cf), config_(config) {}

        void set_ddop(DDOP pool) { ddop_ = std::move(pool); }
        const DDOP &ddop() const noexcept { return ddop_; }

        Result<void> connect() {
            auto validation = ddop_.validate();
            if (!validation) {
                echo::category("isobus.tc.client").error("DDOP validation failed");
                return Result<void>::err(Error::invalid_state("DDOP validation failed"));
            }

            state_.transition(TCState::WaitForServerStatus);
            echo::category("isobus.tc.client").debug("state: ", static_cast<u8>(state_.state()));
            timer_ms_ = 0;

            net_.register_pgn_callback(PGN_TC_TO_ECU, [this](const Message &msg) { handle_tc_message(msg); });

            echo::category("isobus.tc.client").info("TC client connecting...");
            return {};
        }

        Result<void> disconnect() {
            state_.transition(TCState::Disconnected);
            echo::category("isobus.tc.client").debug("state: ", static_cast<u8>(state_.state()));
            echo::category("isobus.tc.client").info("TC client disconnected");
            return {};
        }

        TCState state() const noexcept { return state_.state(); }

        void on_value_request(ValueCallback cb) { value_callback_ = std::move(cb); }
        void on_value_command(CommandCallback cb) { command_callback_ = std::move(cb); }

        void update(u32 elapsed_ms) {
            timer_ms_ += elapsed_ms;

            switch (state_.state()) {
            case TCState::WaitForServerStatus:
                if (timer_ms_ >= config_.timeout_ms) {
                    echo::category("isobus.tc.client").warn("TC server not found");
                    state_.transition(TCState::Disconnected);
                }
                break;

            case TCState::SendWorkingSetMaster: {
                dp::Vector<u8> data(8, 0xFF);
                data[0] = 1; // Number of members
                net_.send(PGN_WORKING_SET_MASTER, data, cf_);
                state_.transition(TCState::RequestVersion);
                timer_ms_ = 0;
                break;
            }

            case TCState::RequestVersion: {
                dp::Vector<u8> data(8, 0xFF);
                data[0] = tc_cmd::VERSION_REQUEST;
                net_.send(PGN_ECU_TO_TC, data, cf_);
                state_.transition(TCState::WaitForVersion);
                timer_ms_ = 0;
                break;
            }

            case TCState::WaitForVersion:
                if (timer_ms_ >= config_.timeout_ms) {
                    echo::category("isobus.tc.client").warn("Version response timeout");
                    state_.transition(TCState::Disconnected);
                }
                break;

            case TCState::TransferDDOP: {
                auto pool_result = ddop_.serialize();
                if (pool_result) {
                    auto &pool_data = pool_result.value();
                    // Send DDOP via IsoNet (auto-selects TP/ETP based on size)
                    ControlFunction tc_cf;
                    tc_cf.address = tc_address_;
                    auto send_result = net_.send(PGN_ECU_TO_TC, pool_data, cf_, &tc_cf);
                    if (!send_result.is_ok()) {
                        echo::category("isobus.tc.client").error("DDOP transfer failed: transport error");
                        state_.transition(TCState::Disconnected);
                        break;
                    }
                    state_.transition(TCState::WaitForPoolResponse);
                    timer_ms_ = 0;
                    echo::category("isobus.tc.client")
                        .info("DDOP transferred: ", pool_data.size(), " bytes via transport");
                }
                break;
            }

            case TCState::ActivatePool: {
                dp::Vector<u8> data(8, 0xFF);
                data[0] = tc_cmd::ACTIVATE_POOL;
                net_.send(PGN_ECU_TO_TC, data, cf_);
                state_.transition(TCState::WaitForActivation);
                timer_ms_ = 0;
                break;
            }

            case TCState::WaitForPoolResponse:
            case TCState::WaitForActivation:
                if (timer_ms_ >= config_.timeout_ms) {
                    echo::category("isobus.tc.client").warn("TC response timeout");
                    state_.transition(TCState::Disconnected);
                }
                break;

            default:
                break;
            }
        }

        // Events
        Event<TCState> on_state_change;

      private:
        void handle_tc_message(const Message &msg) {
            if (msg.data.size() < 1)
                return;
            u8 cmd = msg.data[0];

            switch (cmd) {
            case tc_cmd::TC_STATUS:
                handle_tc_status(msg);
                break;
            case tc_cmd::VERSION_RESPONSE:
                handle_version_response(msg);
                break;
            case tc_cmd::OBJECT_POOL_RESPONSE:
                handle_pool_response(msg);
                break;
            case tc_cmd::ACTIVATE_RESPONSE:
                handle_activate_response(msg);
                break;
            case tc_cmd::REQUEST_VALUE:
                handle_value_request(msg);
                break;
            case tc_cmd::SET_VALUE:
                handle_value_command(msg);
                break;
            }
        }

        void handle_tc_status(const Message &msg) {
            tc_address_ = msg.source;
            if (state_.state() == TCState::WaitForServerStatus) {
                echo::category("isobus.tc.client").info("connected to TC at addr=", tc_address_);
                state_.transition(TCState::SendWorkingSetMaster);
                echo::category("isobus.tc.client").debug("state: ", static_cast<u8>(state_.state()));
                timer_ms_ = 0;
            }
        }

        void handle_version_response(const Message &msg) {
            if (msg.data.size() < 5)
                return;
            tc_version_ = msg.data[1];
            num_booms_ = msg.data[2];
            num_sections_ = msg.data[3];
            echo::category("isobus.tc.client")
                .info("TC version=", tc_version_, " booms=", num_booms_, " sections=", num_sections_);
            state_.transition(TCState::TransferDDOP);
            timer_ms_ = 0;
        }

        void handle_pool_response(const Message &msg) {
            if (msg.data.size() < 2)
                return;
            bool success = (msg.data[1] == 0);
            if (success) {
                echo::category("isobus.tc.client").info("DDOP accepted");
                state_.transition(TCState::ActivatePool);
            } else {
                echo::category("isobus.tc.client").error("DDOP rejected");
                state_.transition(TCState::Disconnected);
            }
            timer_ms_ = 0;
        }

        void handle_activate_response(const Message &msg) {
            if (msg.data.size() < 2)
                return;
            bool success = (msg.data[1] == 0);
            if (success) {
                state_.transition(TCState::Connected);
                echo::category("isobus.tc.client").info("TC connected, pool active");
                on_state_change.emit(TCState::Connected);
            } else {
                echo::category("isobus.tc.client").error("Pool activation failed");
                state_.transition(TCState::Disconnected);
            }
        }

        void handle_value_request(const Message &msg) {
            if (msg.data.size() < 4 || !value_callback_)
                return;
            // Packed format: low nibble = command, element = high nibble byte[0] | byte[1]<<4
            ElementNumber elem = static_cast<u16>((msg.data[0] >> 4) & 0x0F) | (static_cast<u16>(msg.data[1]) << 4);
            DDI ddi = static_cast<u16>(msg.data[2]) | (static_cast<u16>(msg.data[3]) << 8);

            auto result = value_callback_(elem, ddi);
            if (result) {
                dp::Vector<u8> response(8, 0xFF);
                // Pack: VALUE command in low nibble, element high nibble in high nibble
                response[0] = (static_cast<u8>(tc_cmd::VALUE_RESPONSE) & 0x0F) | ((static_cast<u8>(elem) & 0x0F) << 4);
                response[1] = static_cast<u8>((elem >> 4) & 0xFF);
                response[2] = static_cast<u8>(ddi & 0xFF);
                response[3] = static_cast<u8>((ddi >> 8) & 0xFF);
                i32 val = result.value();
                response[4] = static_cast<u8>(val & 0xFF);
                response[5] = static_cast<u8>((val >> 8) & 0xFF);
                response[6] = static_cast<u8>((val >> 16) & 0xFF);
                response[7] = static_cast<u8>((val >> 24) & 0xFF);

                ControlFunction tc_cf;
                tc_cf.address = tc_address_;
                net_.send(PGN_ECU_TO_TC, response, cf_, &tc_cf);
            }
        }

        void handle_value_command(const Message &msg) {
            if (msg.data.size() < 8 || !command_callback_)
                return;
            // Packed format: low nibble = command, element = high nibble byte[0] | byte[1]<<4
            ElementNumber elem = static_cast<u16>((msg.data[0] >> 4) & 0x0F) | (static_cast<u16>(msg.data[1]) << 4);
            DDI ddi = static_cast<u16>(msg.data[2]) | (static_cast<u16>(msg.data[3]) << 8);
            i32 value = static_cast<i32>(msg.data[4]) | (static_cast<i32>(msg.data[5]) << 8) |
                        (static_cast<i32>(msg.data[6]) << 16) | (static_cast<i32>(msg.data[7]) << 24);
            command_callback_(elem, ddi, value);
        }
    };

} // namespace agrobus::isobus::tc
