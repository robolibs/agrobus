#pragma once

#include "commands.hpp"
#include "objects.hpp"
#include <agrobus/net/constants.hpp>
#include <agrobus/net/event.hpp>
#include <agrobus/net/message.hpp>
#include <agrobus/net/network_manager.hpp>
#include <agrobus/net/types.hpp>
#include <datapod/datapod.hpp>
#include <echo/echo.hpp>

namespace agrobus::isobus::vt {
    using namespace agrobus::net;

    // ─── Tracked attribute entry ──────────────────────────────────────────────────
    struct TrackedAttribute {
        ObjectID object_id = 0;
        u8 attribute_id = 0;
        u32 value = 0;
    };

    // ─── Alarm Priority Levels ──────────────────────────────────────────────────
    enum class AlarmPriority : u8 {
        Critical = 0,   // Highest priority
        Warning = 1,    // Medium priority
        Information = 2 // Lowest priority
    };

    // ─── Active Alarm Entry ────────────────────────────────────────────────────
    struct AlarmEntry {
        ObjectID alarm_mask_id = 0xFFFF;
        AlarmPriority priority = AlarmPriority::Information;
        u32 activation_timestamp_ms = 0;

        bool operator<(const AlarmEntry &other) const {
            // Higher priority (lower number) comes first
            if (priority != other.priority)
                return static_cast<u8>(priority) < static_cast<u8>(other.priority);
            // If same priority, older alarm comes first
            return activation_timestamp_ms < other.activation_timestamp_ms;
        }
    };

    // ─── VT Client State Tracker ──────────────────────────────────────────────────
    // Maintains a local mirror of VT state by observing VT-to-ECU messages.
    // Tracks: active masks, numeric/string values, visibility, enable states,
    // and soft key mask assignments.
    //
    // Usage:
    //   VTClientStateTracker tracker(nm);
    //   tracker.initialize();
    //   // ... later ...
    //   auto mask = tracker.active_data_mask();
    //   auto val = tracker.numeric_value(obj_id);
    class VTClientStateTracker {
        IsoNet &net_;
        ObjectID active_data_mask_ = 0xFFFF;
        ObjectID active_soft_key_mask_ = 0xFFFF;
        ObjectID active_alarm_mask_ = 0xFFFF;

        dp::Map<ObjectID, u32> numeric_values_;
        dp::Map<ObjectID, dp::String> string_values_;
        dp::Map<ObjectID, bool> visibility_;
        dp::Map<ObjectID, bool> enable_state_;
        dp::Map<ObjectID, ObjectID> soft_key_mask_assignments_; // data mask -> soft key mask

        // ─── Alarm Priority Stack ─────────────────────────────────────────────────
        dp::Vector<AlarmEntry> active_alarms_;              // Sorted by priority (highest first)
        dp::Map<ObjectID, AlarmPriority> alarm_priorities_; // Cached priorities from pool

        u8 vt_busy_code_ = 0;
        u8 vt_function_code_ = 0xFF;
        Address vt_address_ = NULL_ADDRESS;

      public:
        explicit VTClientStateTracker(IsoNet &net) : net_(net) {}

        Result<void> initialize() {
            net_.register_pgn_callback(PGN_VT_TO_ECU, [this](const Message &msg) { handle_vt_message(msg); });
            echo::category("isobus.vt.tracker").debug("state tracker initialized");
            return {};
        }

        // ─── Accessors ────────────────────────────────────────────────────────────
        ObjectID active_data_mask() const noexcept { return active_data_mask_; }
        ObjectID active_soft_key_mask() const noexcept { return active_soft_key_mask_; }
        ObjectID active_alarm_mask() const noexcept { return active_alarm_mask_; }
        Address vt_address() const noexcept { return vt_address_; }
        u8 vt_busy_code() const noexcept { return vt_busy_code_; }

        dp::Optional<u32> numeric_value(ObjectID id) const {
            auto it = numeric_values_.find(id);
            if (it != numeric_values_.end())
                return it->second;
            return dp::nullopt;
        }

        dp::Optional<dp::String> string_value(ObjectID id) const {
            auto it = string_values_.find(id);
            if (it != string_values_.end())
                return it->second;
            return dp::nullopt;
        }

        dp::Optional<bool> is_visible(ObjectID id) const {
            auto it = visibility_.find(id);
            if (it != visibility_.end())
                return it->second;
            return dp::nullopt;
        }

        dp::Optional<bool> is_enabled(ObjectID id) const {
            auto it = enable_state_.find(id);
            if (it != enable_state_.end())
                return it->second;
            return dp::nullopt;
        }

        dp::Optional<ObjectID> soft_key_mask_for(ObjectID data_mask_id) const {
            auto it = soft_key_mask_assignments_.find(data_mask_id);
            if (it != soft_key_mask_assignments_.end())
                return it->second;
            return dp::nullopt;
        }

        // ─── Alarm Priority Stack Management ──────────────────────────────────────
        // Register an alarm mask's priority (typically done during pool upload)
        void register_alarm_priority(ObjectID alarm_mask_id, AlarmPriority priority) {
            alarm_priorities_[alarm_mask_id] = priority;
        }

        // Activate an alarm and add it to the priority stack
        void activate_alarm(ObjectID alarm_mask_id, u32 timestamp_ms = 0) {
            // Check if already active
            for (const auto &alarm : active_alarms_) {
                if (alarm.alarm_mask_id == alarm_mask_id)
                    return; // Already active
            }

            // Get priority (default to Information if not registered)
            AlarmPriority priority = AlarmPriority::Information;
            auto it = alarm_priorities_.find(alarm_mask_id);
            if (it != alarm_priorities_.end()) {
                priority = it->second;
            }

            // Create alarm entry
            AlarmEntry entry;
            entry.alarm_mask_id = alarm_mask_id;
            entry.priority = priority;
            entry.activation_timestamp_ms = timestamp_ms;

            // Insert in priority order
            active_alarms_.push_back(entry);
            sort_alarm_stack();

            // Update active alarm mask to highest priority
            if (!active_alarms_.empty()) {
                active_alarm_mask_ = active_alarms_[0].alarm_mask_id;
                on_alarm_activated.emit(alarm_mask_id, priority);
                echo::category("isobus.vt.tracker")
                    .info("alarm activated: id=", alarm_mask_id, " priority=", static_cast<u32>(priority));
            }
        }

        // Deactivate (acknowledge) the highest priority alarm
        void acknowledge_alarm() {
            if (active_alarms_.empty())
                return;

            ObjectID deactivated_id = active_alarms_[0].alarm_mask_id;
            active_alarms_.erase(active_alarms_.begin());

            // Update active alarm mask to next highest priority (or none)
            if (!active_alarms_.empty()) {
                active_alarm_mask_ = active_alarms_[0].alarm_mask_id;
            } else {
                active_alarm_mask_ = 0xFFFF;
            }

            on_alarm_deactivated.emit(deactivated_id);
            echo::category("isobus.vt.tracker").info("alarm acknowledged: id=", deactivated_id);
        }

        // Deactivate a specific alarm by ID
        void deactivate_alarm(ObjectID alarm_mask_id) {
            auto it = active_alarms_.begin();
            while (it != active_alarms_.end()) {
                if (it->alarm_mask_id == alarm_mask_id) {
                    active_alarms_.erase(it);
                    on_alarm_deactivated.emit(alarm_mask_id);
                    echo::category("isobus.vt.tracker").info("alarm deactivated: id=", alarm_mask_id);
                    break;
                }
                ++it;
            }

            // Update active alarm mask
            if (!active_alarms_.empty()) {
                active_alarm_mask_ = active_alarms_[0].alarm_mask_id;
            } else {
                active_alarm_mask_ = 0xFFFF;
            }
        }

        // Get the current alarm stack (highest priority first)
        const dp::Vector<AlarmEntry> &active_alarms() const noexcept { return active_alarms_; }

        // Get the highest priority active alarm
        dp::Optional<AlarmEntry> highest_priority_alarm() const {
            if (active_alarms_.empty())
                return dp::nullopt;
            return active_alarms_[0];
        }

        // Check if a specific alarm is active
        bool is_alarm_active(ObjectID alarm_mask_id) const {
            for (const auto &alarm : active_alarms_) {
                if (alarm.alarm_mask_id == alarm_mask_id)
                    return true;
            }
            return false;
        }

        // ─── Manual state injection (for testing or initial sync) ─────────────────
        void set_numeric_value(ObjectID id, u32 value) { numeric_values_[id] = value; }
        void set_string_value(ObjectID id, dp::String value) { string_values_[id] = std::move(value); }
        void set_visibility(ObjectID id, bool visible) { visibility_[id] = visible; }
        void set_enable_state(ObjectID id, bool enabled) { enable_state_[id] = enabled; }

        void reset() {
            active_data_mask_ = 0xFFFF;
            active_soft_key_mask_ = 0xFFFF;
            active_alarm_mask_ = 0xFFFF;
            numeric_values_.clear();
            string_values_.clear();
            visibility_.clear();
            enable_state_.clear();
            soft_key_mask_assignments_.clear();
            active_alarms_.clear();
            alarm_priorities_.clear();
            vt_busy_code_ = 0;
            vt_address_ = NULL_ADDRESS;
        }

        // ─── Events ──────────────────────────────────────────────────────────────
        Event<ObjectID> on_active_mask_changed;
        Event<ObjectID, u32> on_numeric_value_changed;
        Event<ObjectID, const dp::String &> on_string_value_changed;
        Event<ObjectID, bool> on_visibility_changed;
        Event<ObjectID, bool> on_enable_state_changed;
        Event<ObjectID, AlarmPriority> on_alarm_activated;
        Event<ObjectID> on_alarm_deactivated;

      private:
        void handle_vt_message(const Message &msg) {
            if (msg.data.size() < 1)
                return;
            vt_address_ = msg.source;

            u8 func = msg.data[0];
            switch (func) {
            case vt_cmd::VT_STATUS:
                handle_vt_status(msg);
                break;
            case vt_cmd::NUMERIC_VALUE_CHANGE:
                handle_numeric_change(msg);
                break;
            case vt_cmd::STRING_VALUE_CHANGE:
                handle_string_change(msg);
                break;
            case vt_cmd::HIDE_SHOW:
                handle_hide_show(msg);
                break;
            case vt_cmd::ENABLE_DISABLE:
                handle_enable_disable(msg);
                break;
            case vt_cmd::CHANGE_ACTIVE_MASK:
                handle_change_active_mask(msg);
                break;
            default:
                break;
            }
        }

        void handle_vt_status(const Message &msg) {
            if (msg.data.size() < 8)
                return;

            // VT Status message format (ISO 11783-6):
            // [0] = 0xFE (VT_STATUS function)
            // [1] = active working set master address
            // [2-3] = active data mask object ID (LE)
            // [4-5] = active soft key mask object ID (LE)
            // [6] = VT busy code
            // [7] = VT function code
            ObjectID new_data_mask = static_cast<u16>(msg.data[2]) | (static_cast<u16>(msg.data[3]) << 8);
            ObjectID new_sk_mask = static_cast<u16>(msg.data[4]) | (static_cast<u16>(msg.data[5]) << 8);
            vt_busy_code_ = msg.data[6];
            vt_function_code_ = msg.data[7];

            if (new_data_mask != active_data_mask_) {
                active_data_mask_ = new_data_mask;
                on_active_mask_changed.emit(active_data_mask_);
                echo::category("isobus.vt.tracker").debug("active data mask: ", active_data_mask_);
            }
            if (new_sk_mask != active_soft_key_mask_) {
                active_soft_key_mask_ = new_sk_mask;
            }
        }

        void handle_numeric_change(const Message &msg) {
            if (msg.data.size() < 8)
                return;
            ObjectID id = static_cast<u16>(msg.data[1]) | (static_cast<u16>(msg.data[2]) << 8);
            u32 value = static_cast<u32>(msg.data[4]) | (static_cast<u32>(msg.data[5]) << 8) |
                        (static_cast<u32>(msg.data[6]) << 16) | (static_cast<u32>(msg.data[7]) << 24);
            numeric_values_[id] = value;
            on_numeric_value_changed.emit(id, value);
            echo::category("isobus.vt.tracker").trace("numeric: id=", id, " val=", value);
        }

        void handle_string_change(const Message &msg) {
            if (msg.data.size() < 5)
                return;
            ObjectID id = static_cast<u16>(msg.data[1]) | (static_cast<u16>(msg.data[2]) << 8);
            u16 len = static_cast<u16>(msg.data[3]) | (static_cast<u16>(msg.data[4]) << 8);
            dp::String str;
            for (u16 i = 0; i < len && static_cast<usize>(5 + i) < msg.data.size(); ++i) {
                str += static_cast<char>(msg.data[5 + i]);
            }
            string_values_[id] = str;
            on_string_value_changed.emit(id, str);
            echo::category("isobus.vt.tracker").trace("string: id=", id, " val=", str);
        }

        void handle_hide_show(const Message &msg) {
            if (msg.data.size() < 4)
                return;
            ObjectID id = static_cast<u16>(msg.data[1]) | (static_cast<u16>(msg.data[2]) << 8);
            bool visible = (msg.data[3] != 0);
            visibility_[id] = visible;
            on_visibility_changed.emit(id, visible);
            echo::category("isobus.vt.tracker").trace("visibility: id=", id, " vis=", visible);
        }

        void handle_enable_disable(const Message &msg) {
            if (msg.data.size() < 4)
                return;
            ObjectID id = static_cast<u16>(msg.data[1]) | (static_cast<u16>(msg.data[2]) << 8);
            bool enabled = (msg.data[3] != 0);
            enable_state_[id] = enabled;
            on_enable_state_changed.emit(id, enabled);
            echo::category("isobus.vt.tracker").trace("enable: id=", id, " en=", enabled);
        }

        void handle_change_active_mask(const Message &msg) {
            if (msg.data.size() < 5)
                return;
            // Response to change active mask contains the new mask ID
            ObjectID new_mask = static_cast<u16>(msg.data[3]) | (static_cast<u16>(msg.data[4]) << 8);
            if (new_mask != active_data_mask_) {
                active_data_mask_ = new_mask;
                on_active_mask_changed.emit(active_data_mask_);
                echo::category("isobus.vt.tracker").debug("active mask changed to: ", active_data_mask_);
            }
        }

        void sort_alarm_stack() {
            // Sort alarms by priority (highest first), then by timestamp (oldest first)
            std::sort(active_alarms_.begin(), active_alarms_.end());
        }
    };

} // namespace agrobus::isobus::vt
