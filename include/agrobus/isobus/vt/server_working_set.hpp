#pragma once

#include "objects.hpp"
#include "working_set.hpp"
#include <agrobus/net/constants.hpp>
#include <agrobus/net/types.hpp>
#include <datapod/datapod.hpp>

namespace agrobus::isobus::vt {
    using namespace agrobus::net;

    // ─── Stored pool version ──────────────────────────────────────────────────────
    struct StoredPoolVersion {
        dp::String label;         // 7-char version label
        dp::Vector<u8> pool_data; // raw serialized pool data
    };

    // ─── VT Server Working Set ───────────────────────────────────────────────────
    // Tracks a connected client's working set state on the server side.
    struct ServerWorkingSet {
        Address client_address = NULL_ADDRESS;
        ObjectPool pool;
        WorkingSet working_set;
        bool pool_uploaded = false;
        bool pool_activated = false;
        u32 last_status_ms = 0;
        dp::Vector<StoredPoolVersion> stored_versions;

        // Find a stored version by label
        StoredPoolVersion *find_version(const dp::String &label) {
            for (auto &v : stored_versions) {
                if (v.label == label)
                    return &v;
            }
            return nullptr;
        }

        // Store current pool with a version label
        bool store_version(const dp::String &label) {
            if (!pool_uploaded || pool.empty())
                return false;
            auto data = pool.serialize();
            if (!data.is_ok())
                return false;

            // Replace existing or add new
            for (auto &v : stored_versions) {
                if (v.label == label) {
                    v.pool_data = std::move(data.value());
                    return true;
                }
            }
            stored_versions.push_back({label, std::move(data.value())});
            return true;
        }

        // Load a stored version into the active pool
        bool load_version(const dp::String &label) {
            auto *ver = find_version(label);
            if (!ver)
                return false;
            auto result = ObjectPool::deserialize(ver->pool_data);
            if (!result.is_ok())
                return false;
            pool = std::move(result.value());
            pool_uploaded = true;
            pool_activated = true;
            return true;
        }

        // Delete a stored version
        bool delete_version(const dp::String &label) {
            for (auto it = stored_versions.begin(); it != stored_versions.end(); ++it) {
                if (it->label == label) {
                    stored_versions.erase(it);
                    return true;
                }
            }
            return false;
        }
    };

} // namespace agrobus::isobus::vt
