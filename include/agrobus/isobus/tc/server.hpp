#pragma once

#include "ddop.hpp"
#include "objects.hpp"
#include "server_options.hpp"
#include <agrobus/net/constants.hpp>
#include <agrobus/net/control_function.hpp>
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

    // ─── TC Server client tracking ───────────────────────────────────────────────
    struct TCClientInfo {
        Address address = NULL_ADDRESS;
        DDOP ddop;
        bool pool_activated = false;
        u32 last_status_ms = 0;
    };

    // ─── TC Status broadcast interval ────────────────────────────────────────────
    inline constexpr u32 TC_STATUS_INTERVAL_MS = 2000;

    // ─── TC Server Config ────────────────────────────────────────────────────────
    struct TCServerConfig {
        u8 tc_number = 0;
        u8 tc_version = 4;
        u8 num_booms = 0;
        u8 num_sections = 0;
        u8 num_channels = 0;
        u8 server_options = 0;

        TCServerConfig &number(u8 n) {
            tc_number = n;
            return *this;
        }
        TCServerConfig &version(u8 v) {
            tc_version = v;
            return *this;
        }
        TCServerConfig &booms(u8 b) {
            num_booms = b;
            return *this;
        }
        TCServerConfig &sections(u8 s) {
            num_sections = s;
            return *this;
        }
        TCServerConfig &channels(u8 c) {
            num_channels = c;
            return *this;
        }
        TCServerConfig &options(u8 o) {
            server_options = o;
            return *this;
        }
    };

    // ─── ISO 11783-10 Task Controller Server ─────────────────────────────────────
    class TaskControllerServer {
        IsoNet &net_;
        InternalCF *cf_;
        StateMachine<TCServerState> state_{TCServerState::Disconnected};
        dp::Vector<TCClientInfo> clients_;
        u8 server_options_ = 0;
        u32 status_timer_ms_ = 0;
        u8 tc_number_ = 0;
        u8 tc_version_ = 4;
        u8 num_booms_ = 0;
        u8 num_sections_ = 0;
        u8 num_channels_ = 0;

        // Callbacks
        using ValueRequestCallback = std::function<Result<i32>(ElementNumber, DDI, TCClientInfo *)>;
        using ValueCallback =
            std::function<Result<ProcessDataAcknowledgeErrorCodes>(ElementNumber, DDI, i32, TCClientInfo *)>;
        using PeerControlCallback =
            std::function<Result<void>(ElementNumber src_element, DDI src_ddi, ElementNumber dst_element, DDI dst_ddi)>;

        dp::Optional<ValueRequestCallback> value_request_cb_;
        dp::Optional<ValueCallback> value_cb_;
        dp::Optional<PeerControlCallback> peer_control_cb_;

      public:
        TaskControllerServer(IsoNet &net, InternalCF *cf, TCServerConfig config = {})
            : net_(net), cf_(cf), server_options_(config.server_options), tc_number_(config.tc_number),
              tc_version_(config.tc_version), num_booms_(config.num_booms), num_sections_(config.num_sections),
              num_channels_(config.num_channels) {}

        Result<void> start() {
            state_.transition(TCServerState::WaitForClients);
            net_.register_pgn_callback(PGN_ECU_TO_TC, [this](const Message &msg) { handle_client_message(msg); });
            echo::category("isobus.tc.server").info("TC Server started");
            return {};
        }

        Result<void> stop() {
            state_.transition(TCServerState::Disconnected);
            clients_.clear();
            echo::category("isobus.tc.server").info("TC Server stopped");
            return {};
        }

        TCServerState state() const noexcept { return state_.state(); }

        // ─── Process data callbacks ──────────────────────────────────────────────
        void on_value_request(ValueRequestCallback cb) { value_request_cb_ = std::move(cb); }
        void on_value_received(ValueCallback cb) { value_cb_ = std::move(cb); }
        void on_peer_control_assignment(PeerControlCallback cb) { peer_control_cb_ = std::move(cb); }

        // ─── Pool management ─────────────────────────────────────────────────────
        Result<ObjectPoolActivationError> activate_pool(TCClientInfo &client) {
            if (client.ddop.devices().empty()) {
                return Result<ObjectPoolActivationError>::ok(ObjectPoolActivationError::ThereAreErrorsInTheDDOP);
            }
            client.pool_activated = true;
            echo::category("isobus.tc.server").info("Pool activated for client ", client.address);
            return Result<ObjectPoolActivationError>::ok(ObjectPoolActivationError::NoErrors);
        }

        Result<void> send_request_value(ElementNumber element, DDI ddi, ControlFunction *dest) {
            dp::Vector<u8> data(8, 0xFF);
            data[0] =
                (static_cast<u8>(ProcessDataCommands::RequestValue) & 0x0F) | ((static_cast<u8>(element) & 0x0F) << 4);
            data[1] = static_cast<u8>((element >> 4) & 0xFF);
            data[2] = static_cast<u8>(ddi & 0xFF);
            data[3] = static_cast<u8>((ddi >> 8) & 0xFF);
            return net_.send(PGN_TC_TO_ECU, data, cf_, dest, Priority::Default);
        }

        Result<void> send_set_value(ElementNumber element, DDI ddi, i32 value, ControlFunction *dest) {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = (static_cast<u8>(ProcessDataCommands::Value) & 0x0F) | ((static_cast<u8>(element) & 0x0F) << 4);
            data[1] = static_cast<u8>((element >> 4) & 0xFF);
            data[2] = static_cast<u8>(ddi & 0xFF);
            data[3] = static_cast<u8>((ddi >> 8) & 0xFF);
            data[4] = static_cast<u8>(value & 0xFF);
            data[5] = static_cast<u8>((value >> 8) & 0xFF);
            data[6] = static_cast<u8>((value >> 16) & 0xFF);
            data[7] = static_cast<u8>((value >> 24) & 0xFF);
            return net_.send(PGN_TC_TO_ECU, data, cf_, dest, Priority::Default);
        }

        const dp::Vector<TCClientInfo> &clients() const noexcept { return clients_; }

        // ─── Events ──────────────────────────────────────────────────────────────
        Event<TCServerState> on_state_change;
        Event<Address> on_client_connected;
        Event<Address> on_client_disconnected;
        Event<ObjectPoolActivationError> on_pool_activation_error;

        // ─── Update loop ─────────────────────────────────────────────────────────
        void update(u32 elapsed_ms) {
            if (state_.state() == TCServerState::Disconnected)
                return;

            status_timer_ms_ += elapsed_ms;
            if (status_timer_ms_ >= TC_STATUS_INTERVAL_MS) {
                status_timer_ms_ -= TC_STATUS_INTERVAL_MS;
                send_tc_status();
            }
        }

      private:
        void send_tc_status() {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = static_cast<u8>(ProcessDataCommands::Status);
            data[1] = tc_number_;
            // Status byte: bit 0 = task active, bit 1 = busy
            data[2] = 0x00;
            // TC version/options
            data[3] = tc_version_;
            data[4] = server_options_;
            data[5] = num_booms_;
            data[6] = num_sections_;
            data[7] = num_channels_;
            net_.send(PGN_TC_TO_ECU, data, cf_, nullptr, Priority::Default);
        }

        void handle_client_message(const Message &msg) {
            if (msg.data.empty())
                return;

            u8 cmd = msg.data[0] & 0x0F;

            switch (static_cast<ProcessDataCommands>(cmd)) {
            case ProcessDataCommands::TechnicalCapabilities:
                handle_tech_capabilities(msg);
                break;
            case ProcessDataCommands::DeviceDescriptor:
                handle_device_descriptor(msg);
                break;
            case ProcessDataCommands::Value:
                handle_value(msg);
                break;
            case ProcessDataCommands::RequestValue:
                handle_request_value(msg);
                break;
            case ProcessDataCommands::PeerControlAssignment:
                handle_peer_control(msg);
                break;
            default:
                echo::category("isobus.tc.server").trace("Unhandled TC command: ", cmd, " from ", msg.source);
                break;
            }
        }

        void handle_tech_capabilities(const Message &msg) {
            ensure_client(msg.source);

            // Respond with our capabilities (addressed to requester)
            dp::Vector<u8> data(8, 0xFF);
            data[0] = static_cast<u8>(ProcessDataCommands::TechnicalCapabilities);
            data[1] = tc_version_;
            data[2] = num_booms_;
            data[3] = num_sections_;
            data[4] = num_channels_;
            ControlFunction dest_cf;
            dest_cf.address = msg.source;
            net_.send(PGN_TC_TO_ECU, data, cf_, &dest_cf, Priority::Default);

            echo::category("isobus.tc.server").debug("Tech capabilities from client ", msg.source);
        }

        void handle_device_descriptor(const Message & /*msg*/) {
            // DDOP transfer handled by transport protocol
            echo::category("isobus.tc.server").trace("Device descriptor received");
        }

        void handle_value(const Message &msg) {
            if (msg.data.size() < 8)
                return;

            echo::category("isobus.tc.server").debug("process data from addr=", msg.source);

            ElementNumber element = static_cast<u16>((msg.data[0] >> 4) & 0x0F) | (static_cast<u16>(msg.data[1]) << 4);
            DDI ddi = static_cast<u16>(msg.data[2]) | (static_cast<u16>(msg.data[3]) << 8);
            i32 value = static_cast<i32>(msg.data[4]) | (static_cast<i32>(msg.data[5]) << 8) |
                        (static_cast<i32>(msg.data[6]) << 16) | (static_cast<i32>(msg.data[7]) << 24);

            auto *client = find_client(msg.source);
            if (value_cb_.has_value() && client) {
                auto result = (*value_cb_)(element, ddi, value, client);
                (void)result;
            }
        }

        void handle_request_value(const Message &msg) {
            if (msg.data.size() < 4)
                return;

            ElementNumber element = static_cast<u16>((msg.data[0] >> 4) & 0x0F) | (static_cast<u16>(msg.data[1]) << 4);
            DDI ddi = static_cast<u16>(msg.data[2]) | (static_cast<u16>(msg.data[3]) << 8);

            auto *client = find_client(msg.source);
            if (value_request_cb_.has_value() && client) {
                auto result = (*value_request_cb_)(element, ddi, client);
                if (result.is_ok()) {
                    dp::Vector<u8> data(8, 0xFF);
                    data[0] =
                        (static_cast<u8>(ProcessDataCommands::Value) & 0x0F) | ((static_cast<u8>(element) & 0x0F) << 4);
                    data[1] = static_cast<u8>((element >> 4) & 0xFF);
                    data[2] = static_cast<u8>(ddi & 0xFF);
                    data[3] = static_cast<u8>((ddi >> 8) & 0xFF);
                    i32 v = result.value();
                    data[4] = static_cast<u8>(v & 0xFF);
                    data[5] = static_cast<u8>((v >> 8) & 0xFF);
                    data[6] = static_cast<u8>((v >> 16) & 0xFF);
                    data[7] = static_cast<u8>((v >> 24) & 0xFF);
                    // Address response back to requester
                    ControlFunction dest_cf;
                    dest_cf.address = msg.source;
                    net_.send(PGN_TC_TO_ECU, data, cf_, &dest_cf, Priority::Default);
                }
            }
        }

        void handle_peer_control(const Message &msg) {
            if (msg.data.size() < 8)
                return;

            ElementNumber src_element =
                static_cast<u16>((msg.data[0] >> 4) & 0x0F) | (static_cast<u16>(msg.data[1]) << 4);
            DDI src_ddi = static_cast<u16>(msg.data[2]) | (static_cast<u16>(msg.data[3]) << 8);
            ElementNumber dst_element = static_cast<u16>(msg.data[4]) | (static_cast<u16>(msg.data[5]) << 8);
            DDI dst_ddi = static_cast<u16>(msg.data[6]) | (static_cast<u16>(msg.data[7]) << 8);

            echo::category("isobus.tc.server")
                .debug("Peer control: src_elem=", src_element, " src_ddi=", src_ddi, " dst_elem=", dst_element,
                       " dst_ddi=", dst_ddi);

            if (peer_control_cb_.has_value()) {
                auto result = (*peer_control_cb_)(src_element, src_ddi, dst_element, dst_ddi);
                // Send acknowledge back to requester
                dp::Vector<u8> ack(8, 0xFF);
                ack[0] = (static_cast<u8>(ProcessDataCommands::Acknowledge) & 0x0F) |
                         ((static_cast<u8>(src_element) & 0x0F) << 4);
                ack[1] = static_cast<u8>((src_element >> 4) & 0xFF);
                ack[2] = static_cast<u8>(src_ddi & 0xFF);
                ack[3] = static_cast<u8>((src_ddi >> 8) & 0xFF);
                ack[4] = result.is_ok()
                             ? 0x00
                             : static_cast<u8>(ProcessDataAcknowledgeErrorCodes::NoProcessingResourcesAvailable);
                ControlFunction dest_cf;
                dest_cf.address = msg.source;
                net_.send(PGN_TC_TO_ECU, ack, cf_, &dest_cf, Priority::Default);
            }
        }

        void ensure_client(Address addr) {
            for (auto &c : clients_) {
                if (c.address == addr)
                    return;
            }
            clients_.push_back({addr, {}, false, 0});
            echo::category("isobus.tc.server").info("client connected: addr=", addr);
            on_client_connected.emit(addr);

            if (state_.state() == TCServerState::WaitForClients) {
                state_.transition(TCServerState::Active);
                on_state_change.emit(TCServerState::Active);
            }
        }

        TCClientInfo *find_client(Address addr) {
            for (auto &c : clients_) {
                if (c.address == addr)
                    return &c;
            }
            return nullptr;
        }
    };

} // namespace agrobus::isobus::tc
