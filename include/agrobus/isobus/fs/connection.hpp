#pragma once

#include <agrobus/net/types.hpp>
#include <datapod/datapod.hpp>

namespace agrobus::isobus::fs {
    using namespace agrobus::net;

    // ─── Connection maintenance constants (ISO 11783-13) ────────────────────────
    inline constexpr u32 FS_CLIENT_TIMEOUT_MS = 6000;       // 6 second timeout
    inline constexpr u32 FS_STATUS_IDLE_INTERVAL_MS = 2000; // 2s idle
    inline constexpr u32 FS_STATUS_BUSY_INTERVAL_MS = 200;  // 200ms busy
    inline constexpr u8 FS_MAX_STATUS_BURST_PER_SEC = 5;    // max on-change bursts

    // ─── Client connection state ────────────────────────────────────────────────
    struct ClientConnection {
        Address client_address = 0xFF;
        u32 last_activity_ms = 0;
        bool connected = false;
        u8 open_file_count = 0;

        // Check if the connection has timed out
        bool is_timed_out(u32 current_time_ms) const noexcept {
            if (!connected)
                return false;
            return (current_time_ms - last_activity_ms) >= FS_CLIENT_TIMEOUT_MS;
        }

        // Update activity timestamp
        void touch(u32 current_time_ms) noexcept { last_activity_ms = current_time_ms; }

        // Reset the connection
        void reset() noexcept {
            connected = false;
            open_file_count = 0;
            last_activity_ms = 0;
        }
    };

    // ─── Connection manager for file server ─────────────────────────────────────
    class ConnectionManager {
        dp::Vector<ClientConnection> connections_;
        u8 max_clients_;
        u32 elapsed_total_ms_ = 0;
        u32 status_timer_ms_ = 0;
        u32 burst_count_ = 0;
        u32 burst_window_ms_ = 0;
        bool has_active_clients_ = false;

      public:
        explicit ConnectionManager(u8 max_clients = 4) : max_clients_(max_clients) {}

        // Register or update a client connection
        bool connect(Address client, u32 current_time_ms) {
            // Check if already connected
            for (auto &conn : connections_) {
                if (conn.client_address == client) {
                    conn.connected = true;
                    conn.touch(current_time_ms);
                    return true;
                }
            }

            // Check capacity
            if (connections_.size() >= max_clients_) {
                return false;
            }

            ClientConnection conn;
            conn.client_address = client;
            conn.connected = true;
            conn.last_activity_ms = current_time_ms;
            connections_.push_back(conn);
            has_active_clients_ = true;
            return true;
        }

        // Disconnect a client
        void disconnect(Address client) {
            for (auto it = connections_.begin(); it != connections_.end(); ++it) {
                if (it->client_address == client) {
                    connections_.erase(it);
                    update_active_state();
                    return;
                }
            }
        }

        // Record activity from a client
        void record_activity(Address client, u32 current_time_ms) {
            for (auto &conn : connections_) {
                if (conn.client_address == client) {
                    conn.touch(current_time_ms);
                    return;
                }
            }
        }

        // Update timers and check for timeouts
        dp::Vector<Address> update(u32 elapsed_ms) {
            elapsed_total_ms_ += elapsed_ms;
            burst_window_ms_ += elapsed_ms;

            // Reset burst counter every second
            if (burst_window_ms_ >= 1000) {
                burst_window_ms_ -= 1000;
                burst_count_ = 0;
            }

            // Check for timed-out connections
            dp::Vector<Address> timed_out;
            for (auto it = connections_.begin(); it != connections_.end();) {
                if (it->is_timed_out(elapsed_total_ms_)) {
                    timed_out.push_back(it->client_address);
                    it = connections_.erase(it);
                } else {
                    ++it;
                }
            }

            update_active_state();
            return timed_out;
        }

        // Get the appropriate status interval based on server load
        u32 current_status_interval() const noexcept {
            return has_active_clients_ ? FS_STATUS_BUSY_INTERVAL_MS : FS_STATUS_IDLE_INTERVAL_MS;
        }

        // Check if a status burst is allowed (rate limiting)
        bool can_send_status_burst() noexcept {
            if (burst_count_ < FS_MAX_STATUS_BURST_PER_SEC) {
                ++burst_count_;
                return true;
            }
            return false;
        }

        // Check if status should be sent based on timer
        bool should_send_status(u32 elapsed_ms) {
            status_timer_ms_ += elapsed_ms;
            u32 interval = current_status_interval();
            if (status_timer_ms_ >= interval) {
                status_timer_ms_ -= interval;
                return true;
            }
            return false;
        }

        // Accessors
        const dp::Vector<ClientConnection> &connections() const noexcept { return connections_; }
        bool has_active_clients() const noexcept { return has_active_clients_; }
        u8 active_client_count() const noexcept { return static_cast<u8>(connections_.size()); }
        u8 max_clients() const noexcept { return max_clients_; }
        u32 elapsed_total() const noexcept { return elapsed_total_ms_; }

        // Get connection for a specific client
        dp::Optional<ClientConnection> get_connection(Address client) const {
            for (const auto &conn : connections_) {
                if (conn.client_address == client) {
                    return conn;
                }
            }
            return dp::nullopt;
        }

        // Increment open file count for a client
        void increment_open_files(Address client) {
            for (auto &conn : connections_) {
                if (conn.client_address == client) {
                    ++conn.open_file_count;
                    return;
                }
            }
        }

        // Decrement open file count for a client
        void decrement_open_files(Address client) {
            for (auto &conn : connections_) {
                if (conn.client_address == client && conn.open_file_count > 0) {
                    --conn.open_file_count;
                    return;
                }
            }
        }

      private:
        void update_active_state() noexcept {
            has_active_clients_ = false;
            for (const auto &conn : connections_) {
                if (conn.connected) {
                    has_active_clients_ = true;
                    return;
                }
            }
        }
    };

} // namespace agrobus::isobus::fs
