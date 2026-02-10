#pragma once

#include "commands.hpp"
#include "objects.hpp"
#include "server_working_set.hpp"
#include "working_set.hpp"
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

namespace agrobus::isobus::vt {
    using namespace agrobus::net;

    // ─── VT Server state ─────────────────────────────────────────────────────────
    enum class VTServerState { Disconnected, WaitForClientStatus, SendWorkingSetMaster, WaitForPoolUpload, Connected };

    // ─── VT Server status message timing ─────────────────────────────────────────
    inline constexpr u32 VT_STATUS_INTERVAL_MS = 1000;

    // ─── VT Server configuration ──────────────────────────────────────────────
    struct VTServerConfig {
        u16 screen_width = 480;
        u16 screen_height = 480;
        u16 vt_version = 5;

        VTServerConfig &width(u16 w) {
            screen_width = w;
            return *this;
        }
        VTServerConfig &height(u16 h) {
            screen_height = h;
            return *this;
        }
        VTServerConfig &version(u16 v) {
            vt_version = v;
            return *this;
        }
        VTServerConfig &screen(u16 w, u16 h) {
            screen_width = w;
            screen_height = h;
            return *this;
        }
    };

    // ─── ISO 11783-6 Virtual Terminal Server ─────────────────────────────────────
    class VTServer {
        IsoNet &net_;
        InternalCF *cf_;
        StateMachine<VTServerState> state_{VTServerState::Disconnected};
        dp::Vector<ServerWorkingSet> clients_;
        u32 status_timer_ms_ = 0;
        u16 vt_version_;
        u16 screen_width_;
        u16 screen_height_;
        Address active_working_set_ = NULL_ADDRESS;

      public:
        VTServer(IsoNet &net, InternalCF *cf, VTServerConfig config = {})
            : net_(net), cf_(cf), vt_version_(config.vt_version), screen_width_(config.screen_width),
              screen_height_(config.screen_height) {}

        Result<void> start() {
            state_.transition(VTServerState::WaitForClientStatus);
            net_.register_pgn_callback(PGN_ECU_TO_VT, [this](const Message &msg) { handle_ecu_message(msg); });
            echo::category("isobus.vt.server").info("VT Server started");
            return {};
        }

        Result<void> stop() {
            state_.transition(VTServerState::Disconnected);
            // Save all versions to disk before stopping
            save_all_versions();
            clients_.clear();
            echo::category("isobus.vt.server").info("VT Server stopped");
            return {};
        }

        VTServerState state() const noexcept { return state_.state(); }
        u16 screen_width() const noexcept { return screen_width_; }
        u16 screen_height() const noexcept { return screen_height_; }

        // ─── Persistent storage management ────────────────────────────────────────

        // Set storage directory for all clients
        void set_storage_path(const dp::String &path) {
            for (auto &client : clients_) {
                client.set_storage_path(path);
            }
        }

        // Load all versions from disk for all clients
        u32 load_all_versions() {
            u32 total_loaded = 0;
            for (auto &client : clients_) {
                total_loaded += client.load_all_versions_from_disk();
            }
            if (total_loaded > 0) {
                echo::category("isobus.vt.server").info("Loaded ", total_loaded, " versions from disk");
            }
            return total_loaded;
        }

        // Save all versions to disk for all clients
        u32 save_all_versions() {
            u32 total_saved = 0;
            for (auto &client : clients_) {
                total_saved += client.save_all_versions_to_disk();
            }
            if (total_saved > 0) {
                echo::category("isobus.vt.server").debug("Saved ", total_saved, " versions to disk");
            }
            return total_saved;
        }

        // Cleanup expired versions for all clients
        u32 cleanup_expired_versions(u32 max_age_days = 30) {
            u32 total_deleted = 0;
            for (auto &client : clients_) {
                total_deleted += client.cleanup_expired_versions(max_age_days);
            }
            if (total_deleted > 0) {
                echo::category("isobus.vt.server").info("Cleaned up ", total_deleted, " expired versions");
            }
            return total_deleted;
        }

        // ─── Active Working Set management ────────────────────────────────────────
        Address active_working_set() const noexcept { return active_working_set_; }

        void set_active_working_set(Address addr) {
            Address old_addr = active_working_set_;
            if (old_addr == addr)
                return;
            active_working_set_ = addr;
            on_active_ws_changed.emit(old_addr, addr);
            echo::category("isobus.vt.server").info("Active working set changed: ", old_addr, " -> ", addr);
        }

        // ─── Send messages to VT clients ─────────────────────────────────────────
        Result<void> send_button_activation(KeyActivationCode code, ObjectID object_id, ObjectID parent_id,
                                            u8 key_number, ControlFunction *dest) {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = vt_cmd::BUTTON_ACTIVATION;
            data[1] = static_cast<u8>(code);
            data[2] = static_cast<u8>(object_id & 0xFF);
            data[3] = static_cast<u8>((object_id >> 8) & 0xFF);
            data[4] = static_cast<u8>(parent_id & 0xFF);
            data[5] = static_cast<u8>((parent_id >> 8) & 0xFF);
            data[6] = key_number;
            return net_.send(PGN_VT_TO_ECU, data, cf_, dest, Priority::High);
        }

        Result<void> send_soft_key_activation(KeyActivationCode code, ObjectID object_id, ObjectID parent_id,
                                              u8 key_number, ControlFunction *dest) {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = vt_cmd::SOFT_KEY_ACTIVATION;
            data[1] = static_cast<u8>(code);
            data[2] = static_cast<u8>(object_id & 0xFF);
            data[3] = static_cast<u8>((object_id >> 8) & 0xFF);
            data[4] = static_cast<u8>(parent_id & 0xFF);
            data[5] = static_cast<u8>((parent_id >> 8) & 0xFF);
            data[6] = key_number;
            return net_.send(PGN_VT_TO_ECU, data, cf_, dest, Priority::High);
        }

        Result<void> send_change_numeric_value(ObjectID object_id, u32 value, ControlFunction *dest) {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = vt_cmd::CHANGE_NUMERIC_VALUE;
            data[1] = static_cast<u8>(object_id & 0xFF);
            data[2] = static_cast<u8>((object_id >> 8) & 0xFF);
            data[3] = 0xFF;
            data[4] = static_cast<u8>(value & 0xFF);
            data[5] = static_cast<u8>((value >> 8) & 0xFF);
            data[6] = static_cast<u8>((value >> 16) & 0xFF);
            data[7] = static_cast<u8>((value >> 24) & 0xFF);
            return net_.send(PGN_VT_TO_ECU, data, cf_, dest, Priority::Default);
        }

        Result<void> send_change_string_value(ObjectID object_id, const dp::String &value, ControlFunction *dest) {
            dp::Vector<u8> data;
            data.push_back(vt_cmd::CHANGE_STRING_VALUE);
            data.push_back(static_cast<u8>(object_id & 0xFF));
            data.push_back(static_cast<u8>((object_id >> 8) & 0xFF));
            u16 len = static_cast<u16>(value.size());
            data.push_back(static_cast<u8>(len & 0xFF));
            data.push_back(static_cast<u8>((len >> 8) & 0xFF));
            for (auto c : value)
                data.push_back(static_cast<u8>(c));
            return net_.send(PGN_VT_TO_ECU, data, cf_, dest, Priority::Default);
        }

        Result<void> send_select_input_object(ObjectID object_id, bool selected, bool open_for_input,
                                              ControlFunction *dest) {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = 0x05; // Select input object
            data[1] = static_cast<u8>(object_id & 0xFF);
            data[2] = static_cast<u8>((object_id >> 8) & 0xFF);
            data[3] = selected ? 1 : 0;
            data[4] = open_for_input ? 1 : 0;
            return net_.send(PGN_VT_TO_ECU, data, cf_, dest, Priority::Default);
        }

        const dp::Vector<ServerWorkingSet> &clients() const noexcept { return clients_; }

        // ─── Events ──────────────────────────────────────────────────────────────
        Event<ObjectID, u8> on_button_activation;
        Event<ObjectID, u32> on_numeric_value_change;
        Event<ObjectID, dp::String> on_string_value_change;
        Event<ObjectID, bool, bool> on_input_object_selected;
        Event<ObjectID, u8> on_soft_key_activation;
        Event<VTServerState> on_state_change;
        Event<Address> on_client_connected;
        Event<Address> on_client_disconnected;
        Event<Address, Address> on_active_ws_changed; // (old_addr, new_addr)

        // ─── Update loop ─────────────────────────────────────────────────────────
        void update(u32 elapsed_ms) {
            if (state_.state() == VTServerState::Disconnected)
                return;

            status_timer_ms_ += elapsed_ms;
            if (status_timer_ms_ >= VT_STATUS_INTERVAL_MS) {
                status_timer_ms_ -= VT_STATUS_INTERVAL_MS;
                send_vt_status();
            }
        }

      private:
        void send_vt_status() {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = vt_cmd::VT_STATUS;
            // Active working set master address
            data[1] = active_working_set_;
            // Data/alarm mask object IDs
            data[2] = 0x00;
            // Active WS address in bytes 3-4 (ISO 11783-6)
            data[3] = static_cast<u8>(active_working_set_ & 0xFF);
            data[4] = 0x00;
            data[5] = 0x00;
            // VT busy codes + function
            data[6] = 0x00;
            data[7] = 0x00;
            net_.send(PGN_VT_TO_ECU, data, cf_, nullptr, Priority::Default);
        }

        void handle_ecu_message(const Message &msg) {
            if (msg.data.empty())
                return;

            u8 function = msg.data[0];

            switch (function) {
            case vt_cmd::GET_MEMORY:
                handle_get_memory(msg);
                break;
            case vt_cmd::OBJECT_POOL_TRANSFER:
                handle_object_pool_transfer(msg);
                break;
            case vt_cmd::STORE_POOL:
                handle_store_version(msg);
                break;
            case vt_cmd::LOAD_POOL:
                handle_load_version(msg);
                break;
            case vt_cmd::DELETE_OBJECT_POOL:
                handle_delete_version(msg);
                break;
            case vt_cmd::GET_VERSIONS:
                handle_get_versions(msg);
                break;
            case vt_cmd::END_OF_POOL:
                handle_end_of_pool(msg);
                break;
            case vt_cmd::POOL_ACTIVATE:
                handle_pool_activate(msg);
                break;
            case vt_cmd::CHANGE_NUMERIC_VALUE:
                handle_numeric_value_change(msg);
                break;
            case vt_cmd::CHANGE_STRING_VALUE:
                handle_string_value_change(msg);
                break;
            default:
                echo::category("isobus.vt.server")
                    .trace("Unhandled ECU->VT function: 0x", function, " from ", msg.source);
                break;
            }
        }

        void send_to_client(const dp::Vector<u8> &data, Address client_addr) {
            ControlFunction dest_cf;
            dest_cf.address = client_addr;
            net_.send(PGN_VT_TO_ECU, data, cf_, &dest_cf, Priority::Default);
        }

        void handle_get_memory(const Message &msg) {
            // Track client
            ensure_client(msg.source);

            // Respond with memory available (addressed to requester)
            dp::Vector<u8> data(8, 0xFF);
            data[0] = vt_cmd::GET_MEMORY_RESPONSE;
            data[1] = vt_version_ & 0xFF;
            data[2] = 0x00; // Enough memory (0 = sufficient)
            send_to_client(data, msg.source);

            if (state_.state() == VTServerState::WaitForClientStatus) {
                state_.transition(VTServerState::WaitForPoolUpload);
                on_state_change.emit(VTServerState::WaitForPoolUpload);
            }

            echo::category("isobus.vt.server").debug("Get memory request from ", msg.source);
        }

        void handle_object_pool_transfer(const Message &msg) {
            // ISO 11783-6 F.39: Object Pool Transfer
            // [0] = 0x11 (Object Pool Transfer command)
            // [1..N] = pool data bytes
            auto *client = find_client(msg.source);
            if (!client) {
                ensure_client(msg.source);
                client = find_client(msg.source);
            }
            if (!client)
                return;

            // Strip the command byte and parse the pool data
            if (msg.data.size() < 2) {
                echo::category("isobus.vt.server").error("Object Pool Transfer too short from ", msg.source);
                return;
            }
            dp::Vector<u8> pool_data(msg.data.begin() + 1, msg.data.end());

            auto result = ObjectPool::deserialize(pool_data);
            if (result.is_ok()) {
                client->pool = std::move(result.value());
                client->pool_uploaded = true;
                echo::category("isobus.vt.server")
                    .info("Pool received from addr=", msg.source, ": ", client->pool.size(), " objects");
            } else {
                echo::category("isobus.vt.server")
                    .error("Pool deserialization failed from ", msg.source, ": ", result.error().message);
            }
        }

        void handle_store_version(const Message &msg) {
            // Store Version command: save current pool with version label
            auto *client = find_client(msg.source);
            if (!client) {
                ensure_client(msg.source);
                client = find_client(msg.source);
            }

            dp::Vector<u8> response(8, 0xFF);
            response[0] = vt_cmd::STORE_POOL;

            if (!client || !client->pool_uploaded || client->pool.empty()) {
                response[1] = 0x01; // error: no pool
                send_to_client(response, msg.source);
                return;
            }

            // Extract 7-byte version label
            dp::String label;
            for (usize i = 1; i < 8 && i < msg.data.size(); ++i) {
                char c = static_cast<char>(msg.data[i]);
                if (c != ' ' && c != '\0')
                    label += c;
            }

            if (client->store_version(label)) {
                response[1] = 0x00; // success
                echo::category("isobus.vt.server").info("Stored version '", label, "' for addr=", msg.source);
            } else {
                response[1] = 0x02; // error: storage failure
            }
            send_to_client(response, msg.source);
        }

        void handle_load_version(const Message &msg) {
            // Load Version command: load a previously stored pool
            auto *client = find_client(msg.source);
            if (!client) {
                ensure_client(msg.source);
                client = find_client(msg.source);
            }

            dp::Vector<u8> response(8, 0xFF);
            response[0] = vt_cmd::LOAD_POOL;

            // Extract version label
            dp::String label;
            for (usize i = 1; i < 8 && i < msg.data.size(); ++i) {
                char c = static_cast<char>(msg.data[i]);
                if (c != ' ' && c != '\0')
                    label += c;
            }

            if (client && client->load_version(label)) {
                response[1] = 0x00; // success
                if (state_.state() != VTServerState::Connected) {
                    state_.transition(VTServerState::Connected);
                    on_state_change.emit(VTServerState::Connected);
                }
                on_client_connected.emit(msg.source);
                echo::category("isobus.vt.server").info("Loaded version '", label, "' for addr=", msg.source);
            } else {
                response[1] = 0x01; // error: version not found
                echo::category("isobus.vt.server").warn("Version '", label, "' not found for addr=", msg.source);
            }
            send_to_client(response, msg.source);
        }

        void handle_delete_version(const Message &msg) {
            auto *client = find_client(msg.source);

            dp::Vector<u8> response(8, 0xFF);
            response[0] = vt_cmd::DELETE_OBJECT_POOL;

            dp::String label;
            for (usize i = 1; i < 8 && i < msg.data.size(); ++i) {
                char c = static_cast<char>(msg.data[i]);
                if (c != ' ' && c != '\0')
                    label += c;
            }

            if (client && client->delete_version(label)) {
                response[1] = 0x00;
                echo::category("isobus.vt.server").info("Deleted version '", label, "'");
            } else {
                response[1] = 0x01;
            }
            send_to_client(response, msg.source);
        }

        void handle_get_versions(const Message &msg) {
            auto *client = find_client(msg.source);

            dp::Vector<u8> response;
            response.push_back(vt_cmd::GET_VERSIONS_RESPONSE);

            if (client && !client->stored_versions.empty()) {
                response.push_back(static_cast<u8>(client->stored_versions.size()));
                for (const auto &ver : client->stored_versions) {
                    // Each label is exactly 7 bytes (space-padded)
                    for (usize i = 0; i < 7; ++i) {
                        response.push_back((i < ver.label.size()) ? static_cast<u8>(ver.label[i]) : 0x20);
                    }
                }
            } else {
                response.push_back(0); // no versions stored
            }
            // Pad to minimum 8 bytes
            while (response.size() < 8)
                response.push_back(0xFF);

            send_to_client(response, msg.source);
        }

        void handle_end_of_pool(const Message &msg) {
            auto *client = find_client(msg.source);
            if (!client)
                return;

            // Send END_OF_POOL response
            dp::Vector<u8> data(8, 0xFF);
            data[0] = vt_cmd::END_OF_POOL;
            if (client->pool_uploaded && !client->pool.empty()) {
                data[1] = 0x00; // No errors
                echo::category("isobus.vt.server")
                    .info("Pool upload complete from ", msg.source, ": ", client->pool.size(), " objects");
            } else {
                data[1] = 0x01; // Error: pool not received or empty
                data[2] = 0x02; // Error code: other error
                echo::category("isobus.vt.server").error("Pool upload failed from ", msg.source);
            }
            send_to_client(data, msg.source);
        }

        void handle_pool_activate(const Message &msg) {
            auto *client = find_client(msg.source);
            if (!client)
                return;

            // Send activation response
            dp::Vector<u8> data(8, 0xFF);
            data[0] = vt_cmd::POOL_ACTIVATE;

            if (client->pool_uploaded && !client->pool.empty()) {
                client->pool_activated = true;
                data[1] = 0x00; // No errors

                if (state_.state() != VTServerState::Connected) {
                    state_.transition(VTServerState::Connected);
                    on_state_change.emit(VTServerState::Connected);
                }
                // Set as active working set if none is currently active
                if (active_working_set_ == NULL_ADDRESS) {
                    set_active_working_set(msg.source);
                }
                on_client_connected.emit(msg.source);
                echo::category("isobus.vt.server").info("pool activated for addr=", msg.source);
            } else {
                data[1] = 0x01; // Error: no pool uploaded
                echo::category("isobus.vt.server").error("Pool activate failed: no pool from ", msg.source);
            }
            send_to_client(data, msg.source);
        }

        void handle_numeric_value_change(const Message &msg) {
            if (msg.data.size() < 8)
                return;
            ObjectID obj_id = static_cast<u16>(msg.data[1]) | (static_cast<u16>(msg.data[2]) << 8);
            u32 value = static_cast<u32>(msg.data[4]) | (static_cast<u32>(msg.data[5]) << 8) |
                        (static_cast<u32>(msg.data[6]) << 16) | (static_cast<u32>(msg.data[7]) << 24);
            on_numeric_value_change.emit(obj_id, value);
        }

        void handle_string_value_change(const Message &msg) {
            if (msg.data.size() < 5)
                return;
            ObjectID obj_id = static_cast<u16>(msg.data[1]) | (static_cast<u16>(msg.data[2]) << 8);
            u16 len = static_cast<u16>(msg.data[3]) | (static_cast<u16>(msg.data[4]) << 8);
            dp::String value;
            for (u16 i = 0; i < len && static_cast<usize>(5 + i) < msg.data.size(); ++i) {
                value += static_cast<char>(msg.data[5 + i]);
            }
            on_string_value_change.emit(obj_id, value);
        }

        void ensure_client(Address addr) {
            for (auto &c : clients_) {
                if (c.client_address == addr)
                    return;
            }
            clients_.push_back({addr, {}, {}, false, false, 0, {}});
        }

        ServerWorkingSet *find_client(Address addr) {
            for (auto &c : clients_) {
                if (c.client_address == addr)
                    return &c;
            }
            return nullptr;
        }
    };

} // namespace agrobus::isobus::vt
