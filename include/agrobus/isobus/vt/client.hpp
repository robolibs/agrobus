#pragma once

#include "commands.hpp"
#include "objects.hpp"
#include "working_set.hpp"
#include <agrobus/net/constants.hpp>
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

    // ─── VT Client state ─────────────────────────────────────────────────────────
    enum class VTState {
        Disconnected,
        WaitForVTStatus,
        SendWorkingSetMaster,
        SendGetMemory,
        WaitForMemory,
        UploadPool,
        WaitForPoolStore,
        WaitForPoolActivate,
        Connected
    };

    // ─── VT Version ─────────────────────────────────────────────────────────────
    enum class VTVersion : u8 { Version3 = 3, Version4 = 4, Version5 = 5 };

    // ─── VT Client Config ────────────────────────────────────────────────────────
    struct VTClientConfig {
        u32 timeout_ms = 6000;
        VTVersion preferred_version = VTVersion::Version4;

        VTClientConfig &timeout(u32 ms) {
            timeout_ms = ms;
            return *this;
        }
        VTClientConfig &version(VTVersion v) {
            preferred_version = v;
            return *this;
        }
    };

    // ─── VT Client ───────────────────────────────────────────────────────────────
    class VTClient {
        IsoNet &net_;
        InternalCF *cf_;
        VTClientConfig config_;
        StateMachine<VTState> state_{VTState::Disconnected};
        ObjectPool pool_;
        WorkingSet working_set_;
        u32 timer_ms_ = 0;
        u8 vt_address_ = NULL_ADDRESS;
        u16 vt_version_ = 0;
        dp::String extended_version_label_;
        bool vt_supports_extended_versions_ = false;
        bool is_active_ws_ = false;

      public:
        VTClient(IsoNet &net, InternalCF *cf, VTClientConfig config = {}) : net_(net), cf_(cf), config_(config) {}

        void set_object_pool(ObjectPool pool) { pool_ = std::move(pool); }
        void set_working_set(WorkingSet ws) { working_set_ = std::move(ws); }

        // ─── Active Working Set status ────────────────────────────────────────────
        bool is_active_ws() const noexcept { return is_active_ws_; }

        Result<void> connect() {
            if (pool_.empty()) {
                return Result<void>::err(Error::invalid_state("object pool is empty"));
            }
            state_.transition(VTState::WaitForVTStatus);
            timer_ms_ = 0;

            net_.register_pgn_callback(PGN_VT_TO_ECU, [this](const Message &msg) { handle_vt_message(msg); });

            echo::category("isobus.vt.client").info("VT client connecting...");
            return {};
        }

        Result<void> disconnect() {
            state_.transition(VTState::Disconnected);
            echo::category("isobus.vt.client").info("VT client disconnected");
            return {};
        }

        VTState state() const noexcept { return state_.state(); }

        // ─── VT Commands ─────────────────────────────────────────────────────────
        Result<void> hide_show(ObjectID id, bool visible) {
            if (state_.state() != VTState::Connected)
                return Result<void>::err(Error::not_connected());
            warn_if_not_active_ws();
            dp::Vector<u8> data(8, 0xFF);
            data[0] = vt_cmd::HIDE_SHOW;
            data[1] = static_cast<u8>(id & 0xFF);
            data[2] = static_cast<u8>((id >> 8) & 0xFF);
            data[3] = visible ? 1 : 0;
            return net_.send(PGN_ECU_TO_VT, data, cf_);
        }

        Result<void> enable_disable(ObjectID id, bool enabled) {
            if (state_.state() != VTState::Connected)
                return Result<void>::err(Error::not_connected());
            dp::Vector<u8> data(8, 0xFF);
            data[0] = vt_cmd::ENABLE_DISABLE;
            data[1] = static_cast<u8>(id & 0xFF);
            data[2] = static_cast<u8>((id >> 8) & 0xFF);
            data[3] = enabled ? 1 : 0;
            return net_.send(PGN_ECU_TO_VT, data, cf_);
        }

        Result<void> change_numeric_value(ObjectID id, u32 value) {
            if (state_.state() != VTState::Connected)
                return Result<void>::err(Error::not_connected());
            dp::Vector<u8> data(8, 0xFF);
            data[0] = vt_cmd::CHANGE_NUMERIC_VALUE;
            data[1] = static_cast<u8>(id & 0xFF);
            data[2] = static_cast<u8>((id >> 8) & 0xFF);
            data[3] = 0xFF;
            data[4] = static_cast<u8>(value & 0xFF);
            data[5] = static_cast<u8>((value >> 8) & 0xFF);
            data[6] = static_cast<u8>((value >> 16) & 0xFF);
            data[7] = static_cast<u8>((value >> 24) & 0xFF);
            return net_.send(PGN_ECU_TO_VT, data, cf_);
        }

        Result<void> change_string_value(ObjectID id, dp::String value) {
            if (state_.state() != VTState::Connected)
                return Result<void>::err(Error::not_connected());
            dp::Vector<u8> data;
            data.push_back(vt_cmd::CHANGE_STRING_VALUE);
            data.push_back(static_cast<u8>(id & 0xFF));
            data.push_back(static_cast<u8>((id >> 8) & 0xFF));
            u16 len = static_cast<u16>(value.size());
            data.push_back(static_cast<u8>(len & 0xFF));
            data.push_back(static_cast<u8>((len >> 8) & 0xFF));
            for (char c : value)
                data.push_back(static_cast<u8>(c));
            while (data.size() < 8)
                data.push_back(0xFF);
            return net_.send(PGN_ECU_TO_VT, data, cf_);
        }

        Result<void> change_active_mask(ObjectID working_set_id, ObjectID mask_id) {
            if (state_.state() != VTState::Connected)
                return Result<void>::err(Error::not_connected());
            dp::Vector<u8> data(8, 0xFF);
            data[0] = vt_cmd::CHANGE_ACTIVE_MASK;
            data[1] = static_cast<u8>(working_set_id & 0xFF);
            data[2] = static_cast<u8>((working_set_id >> 8) & 0xFF);
            data[3] = static_cast<u8>(mask_id & 0xFF);
            data[4] = static_cast<u8>((mask_id >> 8) & 0xFF);
            return net_.send(PGN_ECU_TO_VT, data, cf_);
        }

        // ─── Macro support (ISO 11783-6 Annex J) ────────────────────────────────
        struct VTMacro {
            ObjectID macro_id = 0;
            dp::Vector<dp::Vector<u8>> commands; // Each command is a VT command byte sequence
        };

        Result<void> execute_macro(ObjectID macro_id) {
            if (state_.state() != VTState::Connected)
                return Result<void>::err(Error::not_connected());
            dp::Vector<u8> data(8, 0xFF);
            data[0] = 0xBE; // Execute Macro command
            data[1] = static_cast<u8>(macro_id & 0xFF);
            data[2] = static_cast<u8>((macro_id >> 8) & 0xFF);
            on_macro_executed.emit(macro_id);
            return net_.send(PGN_ECU_TO_VT, data, cf_);
        }

        Result<void> register_macro(VTMacro macro) {
            for (auto &m : macros_) {
                if (m.macro_id == macro.macro_id) {
                    m = std::move(macro);
                    return {};
                }
            }
            macros_.push_back(std::move(macro));
            return {};
        }

        dp::Optional<const VTMacro *> get_macro(ObjectID id) const {
            for (const auto &m : macros_) {
                if (m.macro_id == id)
                    return &m;
            }
            return dp::nullopt;
        }

        const dp::Vector<VTMacro> &macros() const noexcept { return macros_; }

        // ─── Additional VT commands (ISO 11783-6) ────────────────────────────────
        Result<void> change_soft_key_mask(ObjectID data_mask_id, ObjectID sk_mask_id) {
            if (state_.state() != VTState::Connected)
                return Result<void>::err(Error::not_connected());
            dp::Vector<u8> data(8, 0xFF);
            data[0] = vt_cmd::CHANGE_SOFT_KEY_MASK;
            data[1] = 0; // mask type (0 = data mask, 1 = alarm mask)
            data[2] = static_cast<u8>(data_mask_id & 0xFF);
            data[3] = static_cast<u8>((data_mask_id >> 8) & 0xFF);
            data[4] = static_cast<u8>(sk_mask_id & 0xFF);
            data[5] = static_cast<u8>((sk_mask_id >> 8) & 0xFF);
            return net_.send(PGN_ECU_TO_VT, data, cf_);
        }

        Result<void> change_attribute(ObjectID id, u8 attribute_id, u32 value) {
            if (state_.state() != VTState::Connected)
                return Result<void>::err(Error::not_connected());
            dp::Vector<u8> data(8, 0xFF);
            data[0] = vt_cmd::CHANGE_ATTRIBUTE;
            data[1] = static_cast<u8>(id & 0xFF);
            data[2] = static_cast<u8>((id >> 8) & 0xFF);
            data[3] = attribute_id;
            data[4] = static_cast<u8>(value & 0xFF);
            data[5] = static_cast<u8>((value >> 8) & 0xFF);
            data[6] = static_cast<u8>((value >> 16) & 0xFF);
            data[7] = static_cast<u8>((value >> 24) & 0xFF);
            return net_.send(PGN_ECU_TO_VT, data, cf_);
        }

        Result<void> change_size(ObjectID id, u16 width, u16 height) {
            if (state_.state() != VTState::Connected)
                return Result<void>::err(Error::not_connected());
            dp::Vector<u8> data(8, 0xFF);
            data[0] = vt_cmd::CHANGE_SIZE;
            data[1] = static_cast<u8>(id & 0xFF);
            data[2] = static_cast<u8>((id >> 8) & 0xFF);
            data[3] = static_cast<u8>(width & 0xFF);
            data[4] = static_cast<u8>((width >> 8) & 0xFF);
            data[5] = static_cast<u8>(height & 0xFF);
            data[6] = static_cast<u8>((height >> 8) & 0xFF);
            return net_.send(PGN_ECU_TO_VT, data, cf_);
        }

        Result<void> change_child_location(ObjectID parent_id, ObjectID child_id, i16 dx, i16 dy) {
            if (state_.state() != VTState::Connected)
                return Result<void>::err(Error::not_connected());
            dp::Vector<u8> data(8, 0xFF);
            data[0] = vt_cmd::CHANGE_CHILD_LOCATION;
            data[1] = static_cast<u8>(parent_id & 0xFF);
            data[2] = static_cast<u8>((parent_id >> 8) & 0xFF);
            data[3] = static_cast<u8>(child_id & 0xFF);
            data[4] = static_cast<u8>((child_id >> 8) & 0xFF);
            data[5] = static_cast<u8>(static_cast<u16>(dx) & 0xFF);
            data[6] = static_cast<u8>((static_cast<u16>(dx) >> 8) & 0xFF);
            // Note: dy sent as relative offset in byte 7 (per ISO 11783-6)
            data[7] = static_cast<u8>(static_cast<u16>(dy) & 0xFF);
            return net_.send(PGN_ECU_TO_VT, data, cf_);
        }

        Result<void> change_background_colour(ObjectID id, u8 colour) {
            if (state_.state() != VTState::Connected)
                return Result<void>::err(Error::not_connected());
            dp::Vector<u8> data(8, 0xFF);
            data[0] = vt_cmd::CHANGE_BACKGROUND_COLOUR;
            data[1] = static_cast<u8>(id & 0xFF);
            data[2] = static_cast<u8>((id >> 8) & 0xFF);
            data[3] = colour;
            return net_.send(PGN_ECU_TO_VT, data, cf_);
        }

        Result<void> change_list_item(ObjectID list_id, u8 index, ObjectID new_item_id) {
            if (state_.state() != VTState::Connected)
                return Result<void>::err(Error::not_connected());
            dp::Vector<u8> data(8, 0xFF);
            data[0] = vt_cmd::CHANGE_LIST_ITEM;
            data[1] = static_cast<u8>(list_id & 0xFF);
            data[2] = static_cast<u8>((list_id >> 8) & 0xFF);
            data[3] = index;
            data[4] = static_cast<u8>(new_item_id & 0xFF);
            data[5] = static_cast<u8>((new_item_id >> 8) & 0xFF);
            return net_.send(PGN_ECU_TO_VT, data, cf_);
        }

        Result<void> lock_unlock_mask(ObjectID mask_id, bool lock, u16 timeout_ms = 0) {
            if (state_.state() != VTState::Connected)
                return Result<void>::err(Error::not_connected());
            dp::Vector<u8> data(8, 0xFF);
            data[0] = vt_cmd::LOCK_UNLOCK_MASK;
            data[1] = lock ? 0x00 : 0x01;
            data[2] = static_cast<u8>(mask_id & 0xFF);
            data[3] = static_cast<u8>((mask_id >> 8) & 0xFF);
            data[4] = static_cast<u8>(timeout_ms & 0xFF);
            data[5] = static_cast<u8>((timeout_ms >> 8) & 0xFF);
            return net_.send(PGN_ECU_TO_VT, data, cf_);
        }

        Result<void> control_audio_signal(u8 activations, u16 frequency_hz, u16 duration_ms, u16 off_time_ms) {
            if (state_.state() != VTState::Connected)
                return Result<void>::err(Error::not_connected());
            dp::Vector<u8> data(8, 0xFF);
            data[0] = vt_cmd::CONTROL_AUDIO_SIGNAL;
            data[1] = activations;
            data[2] = static_cast<u8>(frequency_hz & 0xFF);
            data[3] = static_cast<u8>((frequency_hz >> 8) & 0xFF);
            data[4] = static_cast<u8>(duration_ms & 0xFF);
            data[5] = static_cast<u8>((duration_ms >> 8) & 0xFF);
            data[6] = static_cast<u8>(off_time_ms & 0xFF);
            data[7] = static_cast<u8>((off_time_ms >> 8) & 0xFF);
            return net_.send(PGN_ECU_TO_VT, data, cf_);
        }

        // ─── Object pool version management (ISO 11783-6 Annex F) ────────────────
        // Store Version: saves the currently loaded pool with a version label
        Result<void> store_version(const dp::String &version_label) {
            if (state_.state() != VTState::Connected)
                return Result<void>::err(Error::not_connected());
            dp::Vector<u8> data(8, 0xFF);
            data[0] = vt_cmd::STORE_POOL;
            // Version label: up to 7 bytes (padded with spaces)
            for (usize i = 0; i < 7; ++i) {
                data[1 + i] = (i < version_label.size()) ? static_cast<u8>(version_label[i]) : 0x20;
            }
            ControlFunction vt_cf;
            vt_cf.address = vt_address_;
            return net_.send(PGN_ECU_TO_VT, data, cf_, &vt_cf);
        }

        // Load Version: requests the VT to load a previously stored pool
        Result<void> load_version(const dp::String &version_label) {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = vt_cmd::LOAD_POOL;
            for (usize i = 0; i < 7; ++i) {
                data[1 + i] = (i < version_label.size()) ? static_cast<u8>(version_label[i]) : 0x20;
            }
            ControlFunction vt_cf;
            vt_cf.address = vt_address_;
            auto result = net_.send(PGN_ECU_TO_VT, data, cf_, &vt_cf);
            if (result.is_ok()) {
                state_.transition(VTState::WaitForPoolActivate);
                timer_ms_ = 0;
                echo::category("isobus.vt.client").info("Loading pool version: ", version_label);
            }
            return result;
        }

        // Delete Version: delete a stored pool version from the VT
        Result<void> delete_version(const dp::String &version_label) {
            if (state_.state() != VTState::Connected)
                return Result<void>::err(Error::not_connected());
            dp::Vector<u8> data(8, 0xFF);
            data[0] = vt_cmd::DELETE_OBJECT_POOL;
            for (usize i = 0; i < 7; ++i) {
                data[1 + i] = (i < version_label.size()) ? static_cast<u8>(version_label[i]) : 0x20;
            }
            ControlFunction vt_cf;
            vt_cf.address = vt_address_;
            return net_.send(PGN_ECU_TO_VT, data, cf_, &vt_cf);
        }

        // Get Versions: request the VT to report which version labels are stored
        Result<void> get_versions() {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = vt_cmd::GET_VERSIONS;
            ControlFunction vt_cf;
            vt_cf.address = vt_address_;
            return net_.send(PGN_ECU_TO_VT, data, cf_, &vt_cf);
        }

        // Legacy delete (backwards compatible)
        Result<void> delete_pool(const dp::String &label) { return delete_version(label); }

        // ─── VT v5 Extended Version Label support (32-byte labels) ───────────────
        Result<void> request_extended_version_label() {
            dp::Vector<u8> data;
            data.push_back(vt_cmd::EXTENDED_GET_VERSIONS);
            data.push_back(vt_cmd::EXTENDED_VERSION_SUBFUNCTION);
            // Pad to 8 bytes
            while (data.size() < 8)
                data.push_back(0xFF);
            ControlFunction vt_cf;
            vt_cf.address = vt_address_;
            echo::category("isobus.vt.client").debug("Requesting extended version labels (v5)");
            return net_.send(PGN_ECU_TO_VT, data, cf_, &vt_cf);
        }

        Result<void> send_extended_store_version(dp::String extended_label) {
            if (state_.state() != VTState::Connected)
                return Result<void>::err(Error::not_connected());
            dp::Vector<u8> data;
            data.push_back(vt_cmd::EXTENDED_STORE_VERSION);
            data.push_back(vt_cmd::EXTENDED_VERSION_SUBFUNCTION);
            // Encode the 32-byte extended label (pad with spaces)
            for (usize i = 0; i < vt_cmd::EXTENDED_VERSION_LABEL_SIZE; ++i) {
                data.push_back((i < extended_label.size()) ? static_cast<u8>(extended_label[i]) : 0x20);
            }
            // Pad remaining to fill CAN frame
            while (data.size() < 8)
                data.push_back(0xFF);
            extended_version_label_ = std::move(extended_label);
            ControlFunction vt_cf;
            vt_cf.address = vt_address_;
            echo::category("isobus.vt.client").info("Storing extended version label: ", extended_version_label_);
            return net_.send(PGN_ECU_TO_VT, data, cf_, &vt_cf);
        }

        Result<void> send_extended_load_version(const dp::String &extended_label) {
            dp::Vector<u8> data;
            data.push_back(vt_cmd::EXTENDED_LOAD_VERSION);
            data.push_back(vt_cmd::EXTENDED_VERSION_SUBFUNCTION);
            for (usize i = 0; i < vt_cmd::EXTENDED_VERSION_LABEL_SIZE; ++i) {
                data.push_back((i < extended_label.size()) ? static_cast<u8>(extended_label[i]) : 0x20);
            }
            while (data.size() < 8)
                data.push_back(0xFF);
            ControlFunction vt_cf;
            vt_cf.address = vt_address_;
            auto result = net_.send(PGN_ECU_TO_VT, data, cf_, &vt_cf);
            if (result.is_ok()) {
                state_.transition(VTState::WaitForPoolActivate);
                timer_ms_ = 0;
                echo::category("isobus.vt.client").info("Loading extended version: ", extended_label);
            }
            return result;
        }

        Result<void> send_extended_delete_version(const dp::String &extended_label) {
            if (state_.state() != VTState::Connected)
                return Result<void>::err(Error::not_connected());
            dp::Vector<u8> data;
            data.push_back(vt_cmd::EXTENDED_DELETE_VERSION);
            data.push_back(vt_cmd::EXTENDED_VERSION_SUBFUNCTION);
            for (usize i = 0; i < vt_cmd::EXTENDED_VERSION_LABEL_SIZE; ++i) {
                data.push_back((i < extended_label.size()) ? static_cast<u8>(extended_label[i]) : 0x20);
            }
            while (data.size() < 8)
                data.push_back(0xFF);
            ControlFunction vt_cf;
            vt_cf.address = vt_address_;
            return net_.send(PGN_ECU_TO_VT, data, cf_, &vt_cf);
        }

        bool vt_supports_extended_versions() const noexcept { return vt_supports_extended_versions_; }
        const dp::String &extended_version_label() const noexcept { return extended_version_label_; }

        void set_vt_version_preference(VTVersion version) { vt_version_ = static_cast<u16>(version); }
        u16 get_vt_version() const noexcept { return vt_version_; }

        void update(u32 elapsed_ms) {
            timer_ms_ += elapsed_ms;

            switch (state_.state()) {
            case VTState::WaitForVTStatus:
                if (timer_ms_ >= config_.timeout_ms) {
                    echo::category("isobus.vt.client").warn("VT not found");
                    state_.transition(VTState::Disconnected);
                }
                break;

            case VTState::SendWorkingSetMaster: {
                dp::Vector<u8> data(8, 0xFF);
                data[0] = 1; // Number of members
                net_.send(PGN_WORKING_SET_MASTER, data, cf_);
                state_.transition(VTState::SendGetMemory);
                timer_ms_ = 0;
                break;
            }

            case VTState::SendGetMemory: {
                // ISO 11783-6 F.33: Get Memory command
                // [0] = 0xC0 (Get Memory)
                // [1..4] = required memory in bytes (LE u32)
                // [5..7] = 0xFF reserved
                dp::Vector<u8> data(8, 0xFF);
                data[0] = vt_cmd::GET_MEMORY;
                auto pool_data = pool_.serialize();
                u32 pool_size = pool_data ? static_cast<u32>(pool_data.value().size()) : 0;
                data[1] = static_cast<u8>(pool_size & 0xFF);
                data[2] = static_cast<u8>((pool_size >> 8) & 0xFF);
                data[3] = static_cast<u8>((pool_size >> 16) & 0xFF);
                data[4] = static_cast<u8>((pool_size >> 24) & 0xFF);

                ControlFunction vt_cf;
                vt_cf.address = vt_address_;
                net_.send(PGN_ECU_TO_VT, data, cf_, &vt_cf);
                state_.transition(VTState::WaitForMemory);
                timer_ms_ = 0;
                echo::category("isobus.vt.client").debug("Get Memory: need ", pool_size, " bytes");
                break;
            }

            case VTState::WaitForMemory:
            case VTState::WaitForPoolStore:
            case VTState::WaitForPoolActivate:
                if (timer_ms_ >= config_.timeout_ms) {
                    echo::category("isobus.vt.client").warn("VT response timeout");
                    state_.transition(VTState::Disconnected);
                }
                break;

            default:
                break;
            }
        }

        // ─── Events ──────────────────────────────────────────────────────────────
        Event<ObjectID, ActivationCode> on_soft_key;
        Event<ObjectID, ActivationCode> on_button;
        Event<ObjectID, u32> on_numeric_value_change;
        Event<ObjectID, dp::String> on_string_value_change;
        Event<VTState> on_state_change;
        Event<ObjectID> on_macro_executed;
        Event<u8> on_pool_error;                                     // error code from VT
        Event<dp::Vector<dp::String>> on_versions_received;          // stored version labels
        Event<bool, u8> on_store_version_response;                   // (success, error_code)
        Event<bool, u8> on_load_version_response;                    // (success, error_code)
        Event<dp::Vector<dp::String>> on_extended_versions_received; // 32-byte labels
        Event<bool, u8> on_extended_store_response;                  // (success, error_code)
        Event<bool, u8> on_extended_load_response;                   // (success, error_code)
        Event<bool> on_active_ws_status;                             // true if this client is active WS

      private:
        dp::Vector<VTMacro> macros_;

        void warn_if_not_active_ws() {
            if (!is_active_ws_ && state_.state() == VTState::Connected) {
                echo::category("isobus.vt.client").warn("Sending command while not the active working set");
            }
        }

        void handle_vt_message(const Message &msg) {
            if (msg.data.size() < 1)
                return;
            u8 func = msg.data[0];

            switch (func) {
            case vt_cmd::VT_STATUS:
                handle_vt_status(msg);
                break;
            case vt_cmd::GET_MEMORY_RESPONSE:
                handle_get_memory_response(msg);
                break;
            case vt_cmd::END_OF_POOL:
                handle_end_of_pool_response(msg);
                break;
            case vt_cmd::SOFT_KEY_ACTIVATION:
                handle_soft_key(msg);
                break;
            case vt_cmd::BUTTON_ACTIVATION:
                handle_button(msg);
                break;
            case vt_cmd::NUMERIC_VALUE_CHANGE:
                handle_numeric_change(msg);
                break;
            case vt_cmd::STRING_VALUE_CHANGE:
                handle_string_change(msg);
                break;
            case vt_cmd::STORE_POOL:
                handle_store_version_response(msg);
                break;
            case vt_cmd::LOAD_POOL:
                handle_load_version_response(msg);
                break;
            case vt_cmd::GET_VERSIONS_RESPONSE:
                handle_get_versions_response(msg);
                break;
            case vt_cmd::VT_ESC:
                handle_vt_esc(msg);
                break;
            case vt_cmd::EXTENDED_GET_VERSIONS_RESPONSE:
                handle_extended_version_response(msg);
                break;
            }
        }

        void handle_vt_status(const Message &msg) {
            vt_address_ = msg.source;
            // VT Status byte 6 contains the VT version (ISO 11783-6)
            if (msg.data.size() >= 7) {
                u8 reported_version = msg.data[6];
                if (reported_version > 0) {
                    vt_version_ = reported_version;
                }
            }
            // VT Status byte 1 contains the active WS master address
            if (msg.data.size() >= 2 && cf_) {
                Address active_ws_addr = msg.data[1];
                bool was_active = is_active_ws_;
                is_active_ws_ = (active_ws_addr == cf_->address() && cf_->cf().address_valid());
                if (was_active != is_active_ws_) {
                    on_active_ws_status.emit(is_active_ws_);
                    if (is_active_ws_) {
                        echo::category("isobus.vt.client").info("This client is now the active working set");
                    } else {
                        echo::category("isobus.vt.client").info("This client is no longer the active working set");
                    }
                }
            }
            if (state_.state() == VTState::WaitForVTStatus) {
                echo::category("isobus.vt.client").info("VT found at addr=", vt_address_, " version=", vt_version_);
                state_.transition(VTState::SendWorkingSetMaster);
                echo::category("isobus.vt.client").debug("state: ", static_cast<u8>(state_.state()));
                timer_ms_ = 0;
            }
        }

        void handle_get_memory_response(const Message &msg) {
            if (msg.data.size() < 2)
                return;
            bool enough_memory = (msg.data[1] == 0);
            if (enough_memory) {
                state_.transition(VTState::UploadPool);
                echo::category("isobus.vt.client").info("VT has enough memory, uploading pool");
                upload_pool();
            } else {
                echo::category("isobus.vt.client").error("VT: insufficient memory");
                state_.transition(VTState::Disconnected);
            }
        }

        void upload_pool() {
            auto pool_data = pool_.serialize();
            if (!pool_data.is_ok() || pool_data.value().empty()) {
                echo::category("isobus.vt.client").error("Failed to serialize object pool");
                state_.transition(VTState::Disconnected);
                return;
            }

            // ISO 11783-6 F.39: Object Pool Transfer
            // The pool data is prepended with the Object Pool Transfer command byte (0x11)
            // and sent as a multi-frame message via TP/ETP transport.
            dp::Vector<u8> transfer_data;
            transfer_data.reserve(1 + pool_data.value().size());
            transfer_data.push_back(vt_cmd::OBJECT_POOL_TRANSFER);
            transfer_data.insert(transfer_data.end(), pool_data.value().begin(), pool_data.value().end());

            ControlFunction vt_cf;
            vt_cf.address = vt_address_;
            auto result = net_.send(PGN_ECU_TO_VT, transfer_data, cf_, &vt_cf);
            if (!result.is_ok()) {
                echo::category("isobus.vt.client").error("Pool upload failed: transport error");
                state_.transition(VTState::Disconnected);
                return;
            }

            echo::category("isobus.vt.client").info("Pool uploaded: ", pool_data.value().size(), " bytes");

            // Send End of Object Pool Transfer
            send_end_of_pool();
            state_.transition(VTState::WaitForPoolActivate);
            timer_ms_ = 0;
        }

        void send_end_of_pool() {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = vt_cmd::END_OF_POOL;
            ControlFunction vt_cf;
            vt_cf.address = vt_address_;
            net_.send(PGN_ECU_TO_VT, data, cf_, &vt_cf);
        }

        void handle_end_of_pool_response(const Message &msg) {
            if (state_.state() != VTState::WaitForPoolActivate)
                return;
            if (msg.data.size() < 2)
                return;

            bool success = (msg.data[1] == 0);
            if (success) {
                state_.transition(VTState::Connected);
                echo::category("isobus.vt.client").debug("state: ", static_cast<u8>(state_.state()));
                echo::category("isobus.vt.client").info("Pool activated successfully");
                on_state_change.emit(VTState::Connected);
            } else {
                u8 error_code = msg.data.size() > 2 ? msg.data[2] : 0xFF;
                echo::category("isobus.vt.client").error("pool upload rejected");
                on_pool_error.emit(error_code);
                state_.transition(VTState::Disconnected);
                echo::category("isobus.vt.client").debug("state: ", static_cast<u8>(state_.state()));
            }
        }

        void handle_soft_key(const Message &msg) {
            if (msg.data.size() < 4)
                return;
            ObjectID key_id = static_cast<u16>(msg.data[1]) | (static_cast<u16>(msg.data[2]) << 8);
            auto code = static_cast<ActivationCode>(msg.data[3]);
            on_soft_key.emit(key_id, code);
        }

        void handle_button(const Message &msg) {
            if (msg.data.size() < 4)
                return;
            ObjectID btn_id = static_cast<u16>(msg.data[1]) | (static_cast<u16>(msg.data[2]) << 8);
            auto code = static_cast<ActivationCode>(msg.data[3]);
            on_button.emit(btn_id, code);
        }

        void handle_numeric_change(const Message &msg) {
            if (msg.data.size() < 7)
                return;
            ObjectID id = static_cast<u16>(msg.data[1]) | (static_cast<u16>(msg.data[2]) << 8);
            u32 value = msg.get_u32_le(3);
            on_numeric_value_change.emit(id, value);
        }

        void handle_string_change(const Message &msg) {
            if (msg.data.size() < 5)
                return;
            ObjectID id = static_cast<u16>(msg.data[1]) | (static_cast<u16>(msg.data[2]) << 8);
            u16 len = msg.get_u16_le(3);
            dp::String str;
            for (u16 i = 0; i < len && static_cast<usize>(5 + i) < msg.data.size(); ++i) {
                str += static_cast<char>(msg.data[5 + i]);
            }
            on_string_value_change.emit(id, std::move(str));
        }

        void handle_store_version_response(const Message &msg) {
            if (msg.data.size() < 2)
                return;
            bool success = (msg.data[1] == 0);
            u8 error_code = msg.data.size() > 2 ? msg.data[2] : 0;
            on_store_version_response.emit(success, error_code);
            if (success) {
                echo::category("isobus.vt.client").info("Store version succeeded");
            } else {
                echo::category("isobus.vt.client").warn("Store version failed: error=", error_code);
            }
        }

        void handle_load_version_response(const Message &msg) {
            if (msg.data.size() < 2)
                return;
            bool success = (msg.data[1] == 0);
            u8 error_code = msg.data.size() > 2 ? msg.data[2] : 0;
            on_load_version_response.emit(success, error_code);
            if (success) {
                state_.transition(VTState::Connected);
                on_state_change.emit(VTState::Connected);
                echo::category("isobus.vt.client").info("Load version succeeded");
            } else {
                state_.transition(VTState::Disconnected);
                echo::category("isobus.vt.client").warn("Load version failed: error=", error_code);
            }
        }

        void handle_get_versions_response(const Message &msg) {
            // Response contains number of versions + 7-byte version labels
            if (msg.data.size() < 2)
                return;
            u8 num_versions = msg.data[1];
            dp::Vector<dp::String> labels;
            usize offset = 2;
            for (u8 i = 0; i < num_versions && offset + 7 <= msg.data.size(); ++i) {
                dp::String label;
                for (usize j = 0; j < 7; ++j) {
                    char c = static_cast<char>(msg.data[offset + j]);
                    if (c != ' ' && c != '\0')
                        label += c;
                }
                labels.push_back(std::move(label));
                offset += 7;
            }
            on_versions_received.emit(std::move(labels));
            echo::category("isobus.vt.client").debug("received ", num_versions, " stored versions");
        }

        void handle_vt_esc(const Message &msg) {
            // VT ESC message: VT is reporting an error/escape condition
            if (msg.data.size() < 4)
                return;
            ObjectID escaped_id = static_cast<u16>(msg.data[1]) | (static_cast<u16>(msg.data[2]) << 8);
            u8 error_code = msg.data[3];
            echo::category("isobus.vt.client").warn("VT ESC: obj=", escaped_id, " err=", error_code);
            on_pool_error.emit(error_code);
        }

        void handle_extended_version_response(const Message &msg) {
            // Extended Get Versions Response: contains 32-byte version labels
            if (msg.data.size() < 2)
                return;

            // Check for sub-function marker indicating extended response
            if (msg.data[1] == vt_cmd::EXTENDED_VERSION_SUBFUNCTION) {
                vt_supports_extended_versions_ = true;
                u8 num_versions = msg.data.size() > 2 ? msg.data[2] : 0;
                dp::Vector<dp::String> labels;
                usize offset = 3;
                for (u8 i = 0; i < num_versions && offset + vt_cmd::EXTENDED_VERSION_LABEL_SIZE <= msg.data.size();
                     ++i) {
                    dp::String label;
                    for (usize j = 0; j < vt_cmd::EXTENDED_VERSION_LABEL_SIZE; ++j) {
                        char c = static_cast<char>(msg.data[offset + j]);
                        if (c != ' ' && c != '\0')
                            label += c;
                    }
                    labels.push_back(std::move(label));
                    offset += vt_cmd::EXTENDED_VERSION_LABEL_SIZE;
                }
                on_extended_versions_received.emit(std::move(labels));
                echo::category("isobus.vt.client").debug("received ", num_versions, " extended versions");
            } else {
                // Extended store/load/delete response
                bool success = (msg.data[1] == 0);
                u8 error_code = msg.data.size() > 2 ? msg.data[2] : 0;
                // Determine which response based on state
                if (state_.state() == VTState::WaitForPoolActivate) {
                    on_extended_load_response.emit(success, error_code);
                    if (success) {
                        state_.transition(VTState::Connected);
                        on_state_change.emit(VTState::Connected);
                        echo::category("isobus.vt.client").info("Extended load version succeeded");
                    } else {
                        // Fallback to classic version label
                        echo::category("isobus.vt.client").warn("Extended load failed, falling back to classic");
                        state_.transition(VTState::Disconnected);
                    }
                } else {
                    on_extended_store_response.emit(success, error_code);
                    if (success) {
                        echo::category("isobus.vt.client").info("Extended store version succeeded");
                    } else {
                        echo::category("isobus.vt.client").warn("Extended store failed: err=", error_code);
                    }
                }
            }
        }

        // ─── V5 negotiation: try extended label first, fallback to classic ──────
        void negotiate_version_label(const dp::String &label) {
            if (vt_version_ >= static_cast<u16>(VTVersion::Version5)) {
                // VT v5+: attempt extended version label first
                echo::category("isobus.vt.client").info("VT v5 detected, trying extended version label");
                send_extended_load_version(label);
            } else {
                // Classic: use 7-byte label
                load_version(label.substr(0, vt_cmd::CLASSIC_VERSION_LABEL_SIZE));
            }
        }
    };

} // namespace agrobus::isobus::vt
