#pragma once

#include "objects.hpp"
#include "working_set.hpp"
#include <agrobus/net/constants.hpp>
#include <agrobus/net/types.hpp>
#include <chrono>
#include <datapod/datapod.hpp>
#include <filesystem>
#include <fstream>

namespace agrobus::isobus::vt {
    using namespace agrobus::net;

    // ─── Stored pool version with metadata ────────────────────────────────────────
    struct StoredPoolVersion {
        dp::String label;         // 7-char version label
        dp::Vector<u8> pool_data; // raw serialized pool data
        u64 timestamp_us = 0;     // creation timestamp (microseconds since epoch)
        u32 size_bytes = 0;       // pool data size in bytes
        u16 vt_version = 0;       // VT version compatibility (3, 4, 5, etc.)
        u8 object_count = 0;      // number of objects in pool

        // Calculate size from pool_data
        void update_metadata(u16 vt_ver = 5) {
            size_bytes = static_cast<u32>(pool_data.size());
            vt_version = vt_ver;

            // Parse object count from pool data if possible
            if (pool_data.size() >= 2) {
                // First 2 bytes typically contain object count
                object_count = pool_data[0] | (pool_data[1] << 8);
            }

            // Set timestamp to current time
            auto now = std::chrono::system_clock::now();
            auto duration = now.time_since_epoch();
            timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
        }

        // Check if version is expired (older than max_age_days)
        bool is_expired(u32 max_age_days) const {
            if (timestamp_us == 0)
                return false; // no timestamp, keep it

            auto now = std::chrono::system_clock::now();
            auto duration = now.time_since_epoch();
            u64 now_us = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();

            u64 age_us = now_us - timestamp_us;
            u64 max_age_us = static_cast<u64>(max_age_days) * 24 * 3600 * 1000000;

            return age_us > max_age_us;
        }
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
        dp::String storage_path = "./vt_storage"; // default storage directory

        // Find a stored version by label
        StoredPoolVersion *find_version(const dp::String &label) {
            for (auto &v : stored_versions) {
                if (v.label == label)
                    return &v;
            }
            return nullptr;
        }

        // Store current pool with a version label (with persistent storage)
        bool store_version(const dp::String &label, u16 vt_ver = 5) {
            if (!pool_uploaded || pool.empty())
                return false;
            auto data = pool.serialize();
            if (!data.is_ok())
                return false;

            StoredPoolVersion ver;
            ver.label = label;
            ver.pool_data = std::move(data.value());
            ver.update_metadata(vt_ver);

            // Save to disk
            if (!save_version_to_disk(ver))
                return false;

            // Replace existing or add new in-memory
            for (auto &v : stored_versions) {
                if (v.label == label) {
                    v = ver;
                    return true;
                }
            }
            stored_versions.push_back(std::move(ver));
            return true;
        }

        // Load a stored version into the active pool (try memory, then disk)
        bool load_version(const dp::String &label) {
            // Try in-memory first
            auto *ver = find_version(label);

            // If not in memory, try loading from disk
            if (!ver) {
                auto disk_ver = load_version_from_disk(label);
                if (disk_ver.has_value()) {
                    // Add to in-memory cache
                    stored_versions.push_back(std::move(disk_ver.value()));
                    ver = &stored_versions.back();
                } else {
                    return false;
                }
            }

            // Deserialize and activate
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
                    // Also delete from persistent storage
                    delete_version_from_disk(label);
                    return true;
                }
            }
            return false;
        }

        // ─── Persistent storage methods ───────────────────────────────────────────

        // Set storage directory path
        void set_storage_path(const dp::String &path) { storage_path = path; }

        // Get storage directory for this client
        std::filesystem::path get_client_storage_dir() const {
            std::filesystem::path base(storage_path.c_str());
            // Create subdirectory per client address
            char addr_str[8];
            snprintf(addr_str, sizeof(addr_str), "%02X", client_address);
            return base / addr_str;
        }

        // Ensure storage directory exists
        bool ensure_storage_dir() {
            try {
                auto dir = get_client_storage_dir();
                std::filesystem::create_directories(dir);
                return true;
            } catch (...) {
                return false;
            }
        }

        // Save version to disk
        bool save_version_to_disk(const StoredPoolVersion &ver) {
            if (!ensure_storage_dir())
                return false;

            try {
                dp::String filename = ver.label + ".vtp";
                auto filepath = get_client_storage_dir() / filename.c_str();
                std::ofstream file(filepath, std::ios::binary);
                if (!file)
                    return false;

                // Write header (version metadata)
                file.write("VTP1", 4); // Magic: VT Pool v1
                file.write(reinterpret_cast<const char *>(&ver.timestamp_us), sizeof(ver.timestamp_us));
                file.write(reinterpret_cast<const char *>(&ver.size_bytes), sizeof(ver.size_bytes));
                file.write(reinterpret_cast<const char *>(&ver.vt_version), sizeof(ver.vt_version));
                file.write(reinterpret_cast<const char *>(&ver.object_count), sizeof(ver.object_count));

                // Write label (7 bytes + null terminator)
                char label_buf[8] = {0};
                for (usize i = 0; i < 7 && i < ver.label.size(); ++i)
                    label_buf[i] = ver.label[i];
                file.write(label_buf, 8);

                // Write pool data
                file.write(reinterpret_cast<const char *>(ver.pool_data.data()), ver.pool_data.size());

                return file.good();
            } catch (...) {
                return false;
            }
        }

        // Load version from disk
        dp::Optional<StoredPoolVersion> load_version_from_disk(const dp::String &label) {
            try {
                dp::String filename = label + ".vtp";
                auto filepath = get_client_storage_dir() / filename.c_str();
                std::ifstream file(filepath, std::ios::binary);
                if (!file)
                    return dp::nullopt;

                StoredPoolVersion ver;

                // Read and verify magic
                char magic[4];
                file.read(magic, 4);
                if (std::string(magic, 4) != "VTP1")
                    return dp::nullopt;

                // Read metadata
                file.read(reinterpret_cast<char *>(&ver.timestamp_us), sizeof(ver.timestamp_us));
                file.read(reinterpret_cast<char *>(&ver.size_bytes), sizeof(ver.size_bytes));
                file.read(reinterpret_cast<char *>(&ver.vt_version), sizeof(ver.vt_version));
                file.read(reinterpret_cast<char *>(&ver.object_count), sizeof(ver.object_count));

                // Read label
                char label_buf[8];
                file.read(label_buf, 8);
                ver.label = dp::String(label_buf);

                // Read pool data
                ver.pool_data.resize(ver.size_bytes);
                file.read(reinterpret_cast<char *>(ver.pool_data.data()), ver.size_bytes);

                if (!file.good())
                    return dp::nullopt;

                return ver;
            } catch (...) {
                return dp::nullopt;
            }
        }

        // Delete version from disk
        bool delete_version_from_disk(const dp::String &label) {
            try {
                dp::String filename = label + ".vtp";
                auto filepath = get_client_storage_dir() / filename.c_str();
                return std::filesystem::remove(filepath);
            } catch (...) {
                return false;
            }
        }

        // Load all versions from disk into memory
        u32 load_all_versions_from_disk() {
            u32 loaded = 0;
            try {
                auto dir = get_client_storage_dir();
                if (!std::filesystem::exists(dir))
                    return 0;

                for (const auto &entry : std::filesystem::directory_iterator(dir)) {
                    if (entry.path().extension() == ".vtp") {
                        auto label = entry.path().stem().string();
                        auto ver = load_version_from_disk(dp::String(label.c_str()));
                        if (ver.has_value()) {
                            // Add to in-memory cache if not already present
                            bool exists = false;
                            for (const auto &v : stored_versions) {
                                if (v.label == label) {
                                    exists = true;
                                    break;
                                }
                            }
                            if (!exists) {
                                stored_versions.push_back(std::move(ver.value()));
                                loaded++;
                            }
                        }
                    }
                }
            } catch (...) {
            }
            return loaded;
        }

        // Clean up expired versions (both in-memory and on-disk)
        u32 cleanup_expired_versions(u32 max_age_days = 30) {
            u32 deleted = 0;

            // Remove expired from in-memory cache
            for (auto it = stored_versions.begin(); it != stored_versions.end();) {
                if (it->is_expired(max_age_days)) {
                    delete_version_from_disk(it->label);
                    it = stored_versions.erase(it);
                    deleted++;
                } else {
                    ++it;
                }
            }

            return deleted;
        }

        // Save all in-memory versions to disk
        u32 save_all_versions_to_disk() {
            u32 saved = 0;
            for (const auto &ver : stored_versions) {
                if (save_version_to_disk(ver))
                    saved++;
            }
            return saved;
        }
    };

} // namespace agrobus::isobus::vt
