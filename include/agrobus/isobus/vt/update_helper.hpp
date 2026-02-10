#pragma once

#include "client.hpp"
#include "objects.hpp"
#include "state_tracker.hpp"
#include <agrobus/net/error.hpp>
#include <agrobus/net/types.hpp>
#include <datapod/datapod.hpp>
#include <echo/echo.hpp>

namespace agrobus::isobus::vt {
    using namespace agrobus::net;

    // ─── VT Client Update Helper ──────────────────────────────────────────────────
    // Provides safe, ergonomic wrappers for VT client updates.
    // Uses the state tracker to avoid redundant sends and validate operations.
    //
    // Features:
    // - Skips sends when the value hasn't changed (reduces bus traffic)
    // - Validates object IDs against the pool before sending
    // - Provides batched update support via begin_batch/end_batch
    // - Type-safe numeric value helpers (scaled, clamped)
    //
    // Usage:
    //   VTClientUpdateHelper helper(client, tracker, pool);
    //   helper.set_numeric_value(gauge_id, 42);        // only sends if changed
    //   helper.set_string_value(label_id, "Hello");    // only sends if changed
    //   helper.show(icon_id);                          // only sends if hidden
    class VTClientUpdateHelper {
        VTClient &client_;
        VTClientStateTracker &tracker_;
        const ObjectPool *pool_;
        bool batch_mode_ = false;

        struct PendingUpdate {
            enum class Type : u8 { Numeric, String, Visibility, Enable, ActiveMask };
            Type type;
            ObjectID id;
            u32 numeric_val = 0;
            dp::String string_val;
            bool bool_val = false;
            ObjectID mask_id = 0;
        };
        dp::Vector<PendingUpdate> pending_;

      public:
        VTClientUpdateHelper(VTClient &client, VTClientStateTracker &tracker, const ObjectPool *pool = nullptr)
            : client_(client), tracker_(tracker), pool_(pool) {}

        // ─── Configuration ────────────────────────────────────────────────────────
        VTClientUpdateHelper &with_pool(const ObjectPool *pool) {
            pool_ = pool;
            return *this;
        }

        // ─── Numeric value updates ───────────────────────────────────────────────
        Result<void> set_numeric_value(ObjectID id, u32 value) {
            auto current = tracker_.numeric_value(id);
            if (current && *current == value) {
                echo::category("isobus.vt.helper").trace("skip numeric: id=", id, " unchanged");
                return {};
            }

            if (batch_mode_) {
                pending_.push_back({PendingUpdate::Type::Numeric, id, value, {}, false, 0});
                return {};
            }

            auto result = client_.change_numeric_value(id, value);
            if (result.is_ok()) {
                tracker_.set_numeric_value(id, value);
            }
            return result;
        }

        // Scaled numeric: applies scale factor before sending
        Result<void> set_numeric_scaled(ObjectID id, f64 value, f64 scale, f64 offset = 0.0) {
            u32 raw = static_cast<u32>((value + offset) * scale);
            return set_numeric_value(id, raw);
        }

        // Clamped numeric: clamps value to [min, max] before sending
        Result<void> set_numeric_clamped(ObjectID id, u32 value, u32 min_val, u32 max_val) {
            if (value < min_val)
                value = min_val;
            if (value > max_val)
                value = max_val;
            return set_numeric_value(id, value);
        }

        // ─── String value updates ────────────────────────────────────────────────
        Result<void> set_string_value(ObjectID id, dp::String value) {
            auto current = tracker_.string_value(id);
            if (current && *current == value) {
                echo::category("isobus.vt.helper").trace("skip string: id=", id, " unchanged");
                return {};
            }

            if (batch_mode_) {
                pending_.push_back({PendingUpdate::Type::String, id, 0, std::move(value), false, 0});
                return {};
            }

            auto result = client_.change_string_value(id, value);
            if (result.is_ok()) {
                tracker_.set_string_value(id, std::move(value));
            }
            return result;
        }

        // ─── Visibility updates ──────────────────────────────────────────────────
        Result<void> show(ObjectID id) { return set_visibility(id, true); }
        Result<void> hide(ObjectID id) { return set_visibility(id, false); }

        Result<void> set_visibility(ObjectID id, bool visible) {
            auto current = tracker_.is_visible(id);
            if (current && *current == visible) {
                echo::category("isobus.vt.helper").trace("skip visibility: id=", id, " unchanged");
                return {};
            }

            if (batch_mode_) {
                pending_.push_back({PendingUpdate::Type::Visibility, id, 0, {}, visible, 0});
                return {};
            }

            auto result = client_.hide_show(id, visible);
            if (result.is_ok()) {
                tracker_.set_visibility(id, visible);
            }
            return result;
        }

        // ─── Enable/disable updates ─────────────────────────────────────────────
        Result<void> enable(ObjectID id) { return set_enable(id, true); }
        Result<void> disable(ObjectID id) { return set_enable(id, false); }

        Result<void> set_enable(ObjectID id, bool enabled) {
            auto current = tracker_.is_enabled(id);
            if (current && *current == enabled) {
                echo::category("isobus.vt.helper").trace("skip enable: id=", id, " unchanged");
                return {};
            }

            if (batch_mode_) {
                pending_.push_back({PendingUpdate::Type::Enable, id, 0, {}, enabled, 0});
                return {};
            }

            auto result = client_.enable_disable(id, enabled);
            if (result.is_ok()) {
                tracker_.set_enable_state(id, enabled);
            }
            return result;
        }

        // ─── Active mask ─────────────────────────────────────────────────────────
        Result<void> change_active_mask(ObjectID working_set_id, ObjectID mask_id) {
            if (tracker_.active_data_mask() == mask_id) {
                echo::category("isobus.vt.helper").trace("skip mask: already active");
                return {};
            }

            if (pool_) {
                auto obj = pool_->find(mask_id);
                if (!obj) {
                    return Result<void>::err(Error::invalid_state("mask object not in pool"));
                }
                auto *mask_obj = *obj;
                if (mask_obj->type != ObjectType::DataMask && mask_obj->type != ObjectType::AlarmMask) {
                    return Result<void>::err(Error::invalid_state("object is not a mask type"));
                }
            }

            if (batch_mode_) {
                pending_.push_back({PendingUpdate::Type::ActiveMask, working_set_id, 0, {}, false, mask_id});
                return {};
            }

            return client_.change_active_mask(working_set_id, mask_id);
        }

        // ─── Batch mode ──────────────────────────────────────────────────────────
        // Queues updates and sends them all at once when end_batch() is called.
        void begin_batch() {
            batch_mode_ = true;
            pending_.clear();
        }

        Result<void> end_batch() {
            batch_mode_ = false;
            Result<void> last_error;

            for (auto &update : pending_) {
                Result<void> result;
                switch (update.type) {
                case PendingUpdate::Type::Numeric:
                    result = client_.change_numeric_value(update.id, update.numeric_val);
                    if (result.is_ok())
                        tracker_.set_numeric_value(update.id, update.numeric_val);
                    break;
                case PendingUpdate::Type::String:
                    result = client_.change_string_value(update.id, update.string_val);
                    if (result.is_ok())
                        tracker_.set_string_value(update.id, std::move(update.string_val));
                    break;
                case PendingUpdate::Type::Visibility:
                    result = client_.hide_show(update.id, update.bool_val);
                    if (result.is_ok())
                        tracker_.set_visibility(update.id, update.bool_val);
                    break;
                case PendingUpdate::Type::Enable:
                    result = client_.enable_disable(update.id, update.bool_val);
                    if (result.is_ok())
                        tracker_.set_enable_state(update.id, update.bool_val);
                    break;
                case PendingUpdate::Type::ActiveMask:
                    result = client_.change_active_mask(update.id, update.mask_id);
                    break;
                }
                if (!result.is_ok()) {
                    last_error = result;
                    echo::category("isobus.vt.helper").warn("batch update failed for id=", update.id);
                }
            }
            pending_.clear();

            if (last_error.is_err())
                return last_error;
            return {};
        }

        // Discard pending batch without sending
        void cancel_batch() {
            batch_mode_ = false;
            pending_.clear();
        }

        usize pending_count() const noexcept { return pending_.size(); }
        bool is_batching() const noexcept { return batch_mode_; }
    };

} // namespace agrobus::isobus::vt
