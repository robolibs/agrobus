#pragma once

#include "error_codes.hpp"
#include "types.hpp"
#include <agrobus/net/constants.hpp>
#include <agrobus/net/error.hpp>
#include <agrobus/net/event.hpp>
#include <agrobus/net/internal_cf.hpp>
#include <agrobus/net/message.hpp>
#include <agrobus/net/network_manager.hpp>
#include <agrobus/net/state_machine.hpp>
#include <datapod/datapod.hpp>
#include <echo/echo.hpp>

namespace agrobus::isobus::fs {
    using namespace agrobus::net;

    // ═════════════════════════════════════════════════════════════════════════════
    // Enhanced File Server with full TAN support and idempotency (ISO 11783-13)
    // ═════════════════════════════════════════════════════════════════════════════

    // ─── Per-client connection state ─────────────────────────────────────────────
    struct ClientConnection {
        Address client_address;
        u32 last_ccm_timestamp_ms = 0;
        dp::String current_directory = "\\";
        dp::Vector<FileHandle> open_handles;

        // TAN cache for idempotency (ISO 11783-13 Section 7.2.2)
        dp::Map<TAN, TANResponse> tan_cache;

        bool is_connected(u32 current_time_ms, u32 timeout_ms) const {
            return (current_time_ms - last_ccm_timestamp_ms) <= timeout_ms;
        }

        void update_ccm(u32 current_time_ms) { last_ccm_timestamp_ms = current_time_ms; }
    };

    // ─── Open file state ─────────────────────────────────────────────────────────
    struct OpenFile {
        FileHandle handle = INVALID_FILE_HANDLE;
        Address owner = NULL_ADDRESS;
        dp::String path;
        dp::Vector<u8> *data = nullptr; // Pointer to actual file data
        u32 position = 0;
        OpenFlags flags = OpenFlags::Read;
        bool is_directory = false;
    };

    // ─── File Server Configuration ───────────────────────────────────────────────
    struct FileServerConfig {
        u32 status_broadcast_interval_ms = 2000;
        u32 busy_status_interval_ms = 200;
        u32 ccm_timeout_ms = 6000;        // 6 seconds without CCM = disconnect
        u32 tan_cache_timeout_ms = 10000; // TAN cache entry lifetime
        u8 max_open_files_per_client = 8;
        u8 max_open_files_total = 32;

        FileServerConfig &status_interval(u32 ms) {
            status_broadcast_interval_ms = ms;
            return *this;
        }

        FileServerConfig &busy_interval(u32 ms) {
            busy_status_interval_ms = ms;
            return *this;
        }

        FileServerConfig &ccm_timeout(u32 ms) {
            ccm_timeout_ms = ms;
            return *this;
        }

        FileServerConfig &max_files_per_client(u8 n) {
            max_open_files_per_client = n;
            return *this;
        }

        FileServerConfig &max_files_total(u8 n) {
            max_open_files_total = n;
            return *this;
        }
    };

    // ─── Enhanced File Server ────────────────────────────────────────────────────
    class FileServerEnhanced {
        IsoNet &net_;
        InternalCF *cf_;
        FileServerConfig config_;

        // File system state
        dp::Map<dp::String, dp::Vector<u8>> files_;      // path -> data
        dp::Map<dp::String, FileAttributes> file_attrs_; // path -> attributes
        dp::Vector<dp::String> directories_;             // List of directories
        dp::Vector<OpenFile> open_files_;
        FileHandle next_handle_ = 1;

        // Client connections
        dp::Map<Address, ClientConnection> clients_;

        // Server state
        bool busy_ = false;
        u32 status_timer_ms_ = 0;
        u32 current_time_ms_ = 0;

        // Volume info
        StateMachine<VolumeState> volume_state_{VolumeState::Present};
        dp::String volume_name_ = "ISOBUS";
        u32 volume_removal_timer_ms_ = 0;
        u32 volume_max_removal_time_ms_ = 10000; // Max 10s for removal prep
        dp::Vector<Address> volume_maintain_requests_;

        // Properties
        FileServerProperties properties_;

      public:
        FileServerEnhanced(IsoNet &net, InternalCF *cf, FileServerConfig config = {})
            : net_(net), cf_(cf), config_(config) {

            // Initialize properties
            properties_.version_number = 1;
            properties_.max_simultaneous_files = config_.max_open_files_total;
            properties_.supports_directories = true;
            properties_.supports_volume_management = true;
            properties_.supports_file_attributes = true;
            properties_.supports_move_file = true;
            properties_.supports_delete_file = true;

            // Initialize root directory
            directories_.push_back("\\");
        }

        // ─── Initialization ──────────────────────────────────────────────────────
        Result<void> initialize() {
            if (!cf_) {
                return Result<void>::err(Error::invalid_state("control function not set"));
            }

            // Register for client requests
            net_.register_pgn_callback(PGN_FILE_CLIENT_TO_SERVER,
                                       [this](const Message &msg) { handle_client_message(msg); });

            echo::category("isobus.fs.server").info("Enhanced file server initialized");
            return {};
        }

        // ─── File Management ─────────────────────────────────────────────────────
        Result<void> add_file(dp::String path, dp::Vector<u8> data, FileAttributes attrs = FileAttributes::None) {
            files_[path] = std::move(data);
            file_attrs_[path] = attrs;
            echo::category("isobus.fs.server").debug("File added: ", path);
            return {};
        }

        Result<void> remove_file(const dp::String &path) {
            if (files_.erase(path) > 0) {
                file_attrs_.erase(path);
                echo::category("isobus.fs.server").debug("File removed: ", path);
                return {};
            }
            return Result<void>::err(Error::invalid_state("file not found"));
        }

        // ─── Directory Management ────────────────────────────────────────────────
        Result<void> add_directory(dp::String path) {
            if (!path.empty() && path.back() != '\\') {
                path += '\\';
            }
            directories_.push_back(path);
            echo::category("isobus.fs.server").debug("Directory added: ", path);
            return {};
        }

        bool directory_exists(const dp::String &path) const {
            for (const auto &dir : directories_) {
                if (dir == path)
                    return true;
            }
            return false;
        }

        // List all files in a directory (supports wildcards)
        dp::Vector<FileEntry> list_directory(const dp::String &path, const dp::String &pattern = "*") {
            dp::Vector<FileEntry> entries;

            // List files
            for (const auto &[file_path, data] : files_) {
                // Check if file is in the specified directory
                if (file_path.find(path) == 0) {
                    // Extract filename from path
                    auto filename = file_path.substr(path.size());

                    // Check wildcard match
                    if (pattern != "*" && !wildcard_match(filename, pattern)) {
                        continue;
                    }

                    FileEntry entry;
                    entry.name = filename;
                    entry.size = data.size();
                    entry.attributes = file_attrs_.count(file_path) ? file_attrs_.at(file_path) : FileAttributes::None;
                    entry.date = pack_dos_date(2025, 1, 1);
                    entry.time = pack_dos_time(12, 0, 0);
                    entries.push_back(entry);
                }
            }

            // List subdirectories
            for (const auto &dir : directories_) {
                if (dir == path)
                    continue; // Skip self

                if (dir.find(path) == 0 && dir.size() > path.size()) {
                    auto subdir = dir.substr(path.size());

                    // Only include immediate subdirectories
                    auto next_slash = subdir.find('\\');
                    if (next_slash != dp::String::npos && next_slash < subdir.size() - 1) {
                        continue; // This is a nested subdirectory
                    }

                    FileEntry entry;
                    entry.name = subdir;
                    entry.size = 0;
                    entry.attributes = FileAttributes::Directory;
                    entry.date = pack_dos_date(2025, 1, 1);
                    entry.time = pack_dos_time(12, 0, 0);
                    entries.push_back(entry);
                }
            }

            return entries;
        }

        // ─── Server Properties ───────────────────────────────────────────────────
        FileServerProperties get_properties() const { return properties_; }

        void set_properties(FileServerProperties props) { properties_ = props; }

        // ─── Volume Management ───────────────────────────────────────────────────
        VolumeState get_volume_state() const { return volume_state_.state(); }

        // Initiate volume removal sequence (ISO 11783-13 Section 7.7)
        Result<void> prepare_volume_for_removal() {
            auto current = volume_state_.state();

            if (current == VolumeState::Present || current == VolumeState::InUse) {
                volume_state_.transition(VolumeState::PreparingForRemoval);
                volume_removal_timer_ms_ = 0;
                volume_maintain_requests_.clear();

                echo::category("isobus.fs.server")
                    .info("Volume removal initiated, max time: ", volume_max_removal_time_ms_ / 1000, "s");

                // Broadcast volume status change
                broadcast_volume_status();

                on_volume_preparing_for_removal.emit();
                return {};
            }

            return Result<void>::err(Error::invalid_state("volume not in removable state"));
        }

        // Client requests to maintain volume power during removal
        void receive_volume_maintain_request(Address client) {
            if (volume_state_.state() != VolumeState::PreparingForRemoval) {
                return;
            }

            // Add or update maintain request
            bool found = false;
            for (auto addr : volume_maintain_requests_) {
                if (addr == client) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                volume_maintain_requests_.push_back(client);
                echo::category("isobus.fs.server").debug("Volume maintain request from client ", client);
            }
        }

        // Client finished with volume
        void clear_volume_maintain_request(Address client) {
            volume_maintain_requests_.erase(
                std::remove(volume_maintain_requests_.begin(), volume_maintain_requests_.end(), client),
                volume_maintain_requests_.end());
        }

        // Force volume to removed state
        Result<void> set_volume_removed() {
            volume_state_.transition(VolumeState::Removed);
            echo::category("isobus.fs.server").warn("Volume set to REMOVED state");

            // Close all open files
            for (auto &open_file : open_files_) {
                echo::category("isobus.fs.server")
                    .debug("Force-closing file handle ", static_cast<u32>(open_file.handle), " due to volume removal");
            }
            open_files_.clear();

            broadcast_volume_status();
            on_volume_removed.emit();

            return {};
        }

        // Reinsert volume (transition from Removed back to Present)
        Result<void> reinsert_volume() {
            if (volume_state_.state() != VolumeState::Removed) {
                return Result<void>::err(Error::invalid_state("volume not in removed state"));
            }

            volume_state_.transition(VolumeState::Present);
            echo::category("isobus.fs.server").info("Volume reinserted, state: PRESENT");

            broadcast_volume_status();
            on_volume_present.emit();

            return {};
        }

        // ─── Busy State Control ──────────────────────────────────────────────────
        void set_busy(bool busy) { busy_ = busy; }

        bool is_busy() const { return busy_; }

        // ─── Update Loop ─────────────────────────────────────────────────────────
        void update(u32 elapsed_ms) {
            current_time_ms_ += elapsed_ms;

            // Update volume state machine
            update_volume_state_machine(elapsed_ms);

            // Clean up expired TAN cache entries
            cleanup_expired_tan_cache();

            // Check for disconnected clients
            cleanup_disconnected_clients();

            // Send status broadcast
            status_timer_ms_ += elapsed_ms;
            u32 interval = busy_ ? config_.busy_status_interval_ms : config_.status_broadcast_interval_ms;
            if (status_timer_ms_ >= interval) {
                status_timer_ms_ = 0;
                broadcast_status();
            }
        }

        // ─── Events ──────────────────────────────────────────────────────────────
        Event<Address> on_client_connected;
        Event<Address> on_client_disconnected;
        Event<Address, dp::String> on_file_opened;
        Event<Address, FileHandle> on_file_closed;
        Event<> on_volume_preparing_for_removal;
        Event<> on_volume_removed;
        Event<> on_volume_present;

      private:
        // ─── Message Handling ────────────────────────────────────────────────────
        void handle_client_message(const Message &msg) {
            if (msg.data.size() < 2) {
                echo::category("isobus.fs.server").warn("Malformed message from ", msg.source);
                return;
            }

            u8 function = msg.data[0];
            TAN tan = msg.data[1];

            // Ensure client connection exists
            auto &client = clients_[msg.source];
            client.client_address = msg.source;

            // Check for CCM (Client Connection Maintenance)
            if (function == 0xFF) { // Special CCM function code
                handle_ccm(msg.source, tan);
                return;
            }

            // Check TAN cache for idempotency
            if (client.tan_cache.count(tan) > 0) {
                // TAN match: resend cached response, don't re-execute
                auto &cached = client.tan_cache[tan];
                echo::category("isobus.fs.server")
                    .debug("TAN cache hit for client ", msg.source, " TAN=", static_cast<u32>(tan));
                send_response(msg.source, cached.response_data);
                return;
            }

            // Execute function and cache response
            auto response = execute_function(msg.source, function, tan, msg.data);

            // Cache the response for this TAN
            TANResponse tan_response;
            tan_response.tan = tan;
            tan_response.response_data = response;
            tan_response.timestamp_ms = current_time_ms_;
            client.tan_cache[tan] = tan_response;

            // Send response
            send_response(msg.source, response);
        }

        // ─── CCM Handling ────────────────────────────────────────────────────────
        void handle_ccm(Address client, TAN tan) {
            auto &conn = clients_[client];
            bool was_connected = conn.is_connected(current_time_ms_, config_.ccm_timeout_ms);
            conn.update_ccm(current_time_ms_);

            if (!was_connected) {
                echo::category("isobus.fs.server").info("Client connected: ", client);
                on_client_connected.emit(client);
            }

            echo::category("isobus.fs.server").trace("CCM from client ", client, " TAN=", static_cast<u32>(tan));
        }

        // ─── Function Execution ──────────────────────────────────────────────────
        dp::Vector<u8> execute_function(Address client, u8 function_code, TAN tan, const dp::Vector<u8> &request) {
            auto function = static_cast<FSFunction>(function_code);

            switch (function) {
            case FSFunction::OpenFile:
                return handle_open_file(client, tan, request);

            case FSFunction::CloseFile:
                return handle_close_file(client, tan, request);

            case FSFunction::ReadFile:
                return handle_read_file(client, tan, request);

            case FSFunction::WriteFile:
                return handle_write_file(client, tan, request);

            case FSFunction::SeekFile:
                return handle_seek_file(client, tan, request);

            case FSFunction::GetFileServerProperties:
                return handle_get_properties(client, tan);

            case FSFunction::FileServerStatus:
                return handle_get_status(client, tan);

            case FSFunction::GetCurrentDirectory:
                return handle_get_current_directory(client, tan);

            case FSFunction::ChangeDirectory:
                return handle_change_directory(client, tan, request);

            default:
                echo::category("isobus.fs.server").warn("Unsupported function: ", static_cast<u32>(function_code));
                return encode_error_response(function_code, tan, FSError::NotSupported);
            }
        }

        // ─── Open File ───────────────────────────────────────────────────────────
        dp::Vector<u8> handle_open_file(Address client, TAN tan, const dp::Vector<u8> &request) {
            if (request.size() < 4) {
                return encode_error_response(static_cast<u8>(FSFunction::OpenFile), tan, FSError::MalformedRequest);
            }

            u8 path_len = request[2];
            OpenFlags flags = static_cast<OpenFlags>(request[3]);

            if (request.size() < 4 + path_len) {
                return encode_error_response(static_cast<u8>(FSFunction::OpenFile), tan, FSError::MalformedRequest);
            }

            dp::String path(reinterpret_cast<const char *>(request.data() + 4), path_len);

            // Check if opening a directory
            auto access_mode = get_access_mode(flags);
            bool is_dir_listing = (access_mode == OpenFlags::OpenDir);

            // Check client file limit
            auto &conn = clients_[client];
            if (conn.open_handles.size() >= config_.max_open_files_per_client) {
                return encode_error_response(static_cast<u8>(FSFunction::OpenFile), tan, FSError::TooManyOpen);
            }

            // Check total file limit
            if (open_files_.size() >= config_.max_open_files_total) {
                return encode_error_response(static_cast<u8>(FSFunction::OpenFile), tan, FSError::MaxHandles);
            }

            // Handle directory listing vs file open
            if (is_dir_listing) {
                // Check if directory exists
                dp::String dir_path = path;
                if (!dir_path.empty() && dir_path.back() != '\\') {
                    dir_path += '\\';
                }

                if (!directory_exists(dir_path)) {
                    return encode_error_response(static_cast<u8>(FSFunction::OpenFile), tan, FSError::NotFound);
                }
            } else {
                // Check if file exists
                if (files_.find(path) == files_.end()) {
                    if (!has_flag(flags, OpenFlags::Create)) {
                        return encode_error_response(static_cast<u8>(FSFunction::OpenFile), tan, FSError::NotFound);
                    }

                    // Create new file
                    files_[path] = dp::Vector<u8>();
                    file_attrs_[path] = FileAttributes::None;
                }
            }

            // Allocate handle
            FileHandle handle = allocate_handle();
            if (handle == INVALID_FILE_HANDLE) {
                return encode_error_response(static_cast<u8>(FSFunction::OpenFile), tan, FSError::MaxHandles);
            }

            // Create open file entry
            OpenFile open_file;
            open_file.handle = handle;
            open_file.owner = client;
            open_file.path = path;
            open_file.data = is_dir_listing ? nullptr : &files_[path];
            open_file.position = 0;
            open_file.flags = flags;
            open_file.is_directory = is_dir_listing;
            open_files_.push_back(open_file);

            conn.open_handles.push_back(handle);

            echo::category("isobus.fs.server")
                .debug("File opened: ", path, " handle=", static_cast<u32>(handle), " client=", client);

            on_file_opened.emit(client, path);

            // Encode success response
            dp::Vector<u8> response(8, 0xFF);
            response[0] = static_cast<u8>(FSFunction::OpenFile);
            response[1] = tan;
            response[2] = static_cast<u8>(FSError::Success);
            response[3] = handle;
            return response;
        }

        // ─── Close File ──────────────────────────────────────────────────────────
        dp::Vector<u8> handle_close_file(Address client, TAN tan, const dp::Vector<u8> &request) {
            if (request.size() < 3) {
                return encode_error_response(static_cast<u8>(FSFunction::CloseFile), tan, FSError::MalformedRequest);
            }

            FileHandle handle = request[2];

            // Find open file
            for (auto it = open_files_.begin(); it != open_files_.end(); ++it) {
                if (it->handle == handle && it->owner == client) {
                    // Remove from client's handle list
                    auto &conn = clients_[client];
                    conn.open_handles.erase(std::remove(conn.open_handles.begin(), conn.open_handles.end(), handle),
                                            conn.open_handles.end());

                    echo::category("isobus.fs.server")
                        .debug("File closed: ", it->path, " handle=", static_cast<u32>(handle));

                    on_file_closed.emit(client, handle);

                    open_files_.erase(it);

                    // Success response
                    dp::Vector<u8> response(8, 0xFF);
                    response[0] = static_cast<u8>(FSFunction::CloseFile);
                    response[1] = tan;
                    response[2] = static_cast<u8>(FSError::Success);
                    return response;
                }
            }

            return encode_error_response(static_cast<u8>(FSFunction::CloseFile), tan, FSError::InvalidHandle);
        }

        // ─── Read File ───────────────────────────────────────────────────────────
        dp::Vector<u8> handle_read_file(Address client, TAN tan, const dp::Vector<u8> &request) {
            if (request.size() < 4) {
                return encode_error_response(static_cast<u8>(FSFunction::ReadFile), tan, FSError::MalformedRequest);
            }

            FileHandle handle = request[2];
            u8 count = request[3]; // Number of bytes to read

            // Find open file
            for (auto &open_file : open_files_) {
                if (open_file.handle == handle && open_file.owner == client) {
                    if (open_file.data == nullptr) {
                        return encode_error_response(static_cast<u8>(FSFunction::ReadFile), tan,
                                                     FSError::InvalidHandle);
                    }

                    // Check EOF
                    if (open_file.position >= open_file.data->size()) {
                        return encode_error_response(static_cast<u8>(FSFunction::ReadFile), tan, FSError::EndOfFile);
                    }

                    // Read data
                    u32 available = open_file.data->size() - open_file.position;
                    u32 to_read = std::min(static_cast<u32>(count), available);

                    dp::Vector<u8> response(8, 0xFF);
                    response[0] = static_cast<u8>(FSFunction::ReadFile);
                    response[1] = tan;
                    response[2] = static_cast<u8>(FSError::Success);
                    response[3] = static_cast<u8>(to_read);

                    // Append data (may trigger TP if > 8 bytes total)
                    for (u32 i = 0; i < to_read && response.size() < 8; ++i) {
                        response[4 + i] = (*open_file.data)[open_file.position + i];
                    }

                    open_file.position += to_read;

                    echo::category("isobus.fs.server")
                        .trace("Read ", to_read, " bytes from handle ", static_cast<u32>(handle));

                    return response;
                }
            }

            return encode_error_response(static_cast<u8>(FSFunction::ReadFile), tan, FSError::InvalidHandle);
        }

        // ─── Write File ──────────────────────────────────────────────────────────
        dp::Vector<u8> handle_write_file(Address client, TAN tan, const dp::Vector<u8> &request) {
            if (request.size() < 4) {
                return encode_error_response(static_cast<u8>(FSFunction::WriteFile), tan, FSError::MalformedRequest);
            }

            FileHandle handle = request[2];
            u8 count = request[3];

            if (request.size() < 4 + count) {
                return encode_error_response(static_cast<u8>(FSFunction::WriteFile), tan, FSError::MalformedRequest);
            }

            // Find open file
            for (auto &open_file : open_files_) {
                if (open_file.handle == handle && open_file.owner == client) {
                    if (open_file.data == nullptr) {
                        return encode_error_response(static_cast<u8>(FSFunction::WriteFile), tan,
                                                     FSError::InvalidHandle);
                    }

                    // Check write permissions
                    auto mode = get_access_mode(open_file.flags);
                    if (mode != OpenFlags::Write && mode != OpenFlags::ReadWrite) {
                        return encode_error_response(static_cast<u8>(FSFunction::WriteFile), tan,
                                                     FSError::InvalidAccess);
                    }

                    // Write data
                    auto &data = *open_file.data;
                    if (open_file.position + count > data.size()) {
                        data.resize(open_file.position + count);
                    }

                    for (u8 i = 0; i < count; ++i) {
                        data[open_file.position + i] = request[4 + i];
                    }

                    open_file.position += count;

                    echo::category("isobus.fs.server")
                        .trace("Wrote ", static_cast<u32>(count), " bytes to handle ", static_cast<u32>(handle));

                    // Success response
                    dp::Vector<u8> response(8, 0xFF);
                    response[0] = static_cast<u8>(FSFunction::WriteFile);
                    response[1] = tan;
                    response[2] = static_cast<u8>(FSError::Success);
                    response[3] = count; // Bytes written
                    return response;
                }
            }

            return encode_error_response(static_cast<u8>(FSFunction::WriteFile), tan, FSError::InvalidHandle);
        }

        // ─── Seek File ───────────────────────────────────────────────────────────
        dp::Vector<u8> handle_seek_file(Address client, TAN tan, const dp::Vector<u8> &request) {
            if (request.size() < 7) {
                return encode_error_response(static_cast<u8>(FSFunction::SeekFile), tan, FSError::MalformedRequest);
            }

            FileHandle handle = request[2];
            u32 position = static_cast<u32>(request[3]) | (static_cast<u32>(request[4]) << 8) |
                           (static_cast<u32>(request[5]) << 16) | (static_cast<u32>(request[6]) << 24);

            // Find open file
            for (auto &open_file : open_files_) {
                if (open_file.handle == handle && open_file.owner == client) {
                    if (open_file.data == nullptr) {
                        return encode_error_response(static_cast<u8>(FSFunction::SeekFile), tan,
                                                     FSError::InvalidHandle);
                    }

                    open_file.position = position;

                    echo::category("isobus.fs.server")
                        .trace("Seek handle ", static_cast<u32>(handle), " to position ", position);

                    // Success response
                    dp::Vector<u8> response(8, 0xFF);
                    response[0] = static_cast<u8>(FSFunction::SeekFile);
                    response[1] = tan;
                    response[2] = static_cast<u8>(FSError::Success);
                    return response;
                }
            }

            return encode_error_response(static_cast<u8>(FSFunction::SeekFile), tan, FSError::InvalidHandle);
        }

        // ─── Get Properties ──────────────────────────────────────────────────────
        dp::Vector<u8> handle_get_properties(Address client, TAN tan) {
            auto props_data = properties_.encode();

            dp::Vector<u8> response(8, 0xFF);
            response[0] = static_cast<u8>(FSFunction::GetFileServerProperties);
            response[1] = tan;
            response[2] = static_cast<u8>(FSError::Success);

            // Copy properties data
            for (usize i = 0; i < props_data.size() && i + 3 < response.size(); ++i) {
                response[3 + i] = props_data[i];
            }

            return response;
        }

        // ─── Get Status ──────────────────────────────────────────────────────────
        dp::Vector<u8> handle_get_status(Address client, TAN tan) {
            FileServerStatus status;
            status.busy = busy_;
            status.number_of_open_files = static_cast<u8>(open_files_.size());

            auto status_data = status.encode();

            dp::Vector<u8> response(8, 0xFF);
            response[0] = static_cast<u8>(FSFunction::FileServerStatus);
            response[1] = tan;
            response[2] = static_cast<u8>(FSError::Success);

            for (usize i = 0; i < status_data.size() && i + 3 < response.size(); ++i) {
                response[3 + i] = status_data[i];
            }

            return response;
        }

        // ─── Get Current Directory ───────────────────────────────────────────────
        dp::Vector<u8> handle_get_current_directory(Address client, TAN tan) {
            auto &conn = clients_[client];
            const auto &cwd = conn.current_directory;

            dp::Vector<u8> response(8, 0xFF);
            response[0] = static_cast<u8>(FSFunction::GetCurrentDirectory);
            response[1] = tan;
            response[2] = static_cast<u8>(FSError::Success);
            response[3] = static_cast<u8>(cwd.size());

            // Append path (will trigger TP if needed)
            for (usize i = 0; i < cwd.size() && i + 4 < 8; ++i) {
                response[4 + i] = cwd[i];
            }

            echo::category("isobus.fs.server").trace("Get current directory for client ", client, ": ", cwd);

            return response;
        }

        // ─── Change Directory ────────────────────────────────────────────────────
        dp::Vector<u8> handle_change_directory(Address client, TAN tan, const dp::Vector<u8> &request) {
            if (request.size() < 3) {
                return encode_error_response(static_cast<u8>(FSFunction::ChangeDirectory), tan,
                                             FSError::MalformedRequest);
            }

            u8 path_len = request[2];
            if (request.size() < 3 + path_len) {
                return encode_error_response(static_cast<u8>(FSFunction::ChangeDirectory), tan,
                                             FSError::MalformedRequest);
            }

            dp::String path(reinterpret_cast<const char *>(request.data() + 3), path_len);

            auto &conn = clients_[client];

            // Handle special paths
            if (path == "..") {
                // Go to parent directory
                auto &cwd = conn.current_directory;
                if (cwd != "\\") {
                    // Remove last directory component
                    auto last_slash = cwd.rfind('\\', cwd.size() - 2);
                    if (last_slash != dp::String::npos) {
                        cwd = cwd.substr(0, last_slash + 1);
                    } else {
                        cwd = "\\";
                    }
                }
            } else if (path == ".") {
                // Stay in current directory (no-op)
            } else if (path.empty() || path == "\\") {
                // Go to root
                conn.current_directory = "\\";
            } else {
                // Absolute or relative path
                dp::String target_path;
                if (is_absolute_path(path)) {
                    target_path = path;
                } else {
                    // Relative to current directory
                    target_path = conn.current_directory;
                    if (!target_path.empty() && target_path.back() != '\\') {
                        target_path += '\\';
                    }
                    target_path += path;
                }

                // Ensure trailing slash
                if (!target_path.empty() && target_path.back() != '\\') {
                    target_path += '\\';
                }

                // Check if directory exists
                if (!directory_exists(target_path)) {
                    return encode_error_response(static_cast<u8>(FSFunction::ChangeDirectory), tan, FSError::NotFound);
                }

                conn.current_directory = target_path;
            }

            echo::category("isobus.fs.server")
                .debug("Change directory for client ", client, " to: ", conn.current_directory);

            // Success response
            dp::Vector<u8> response(8, 0xFF);
            response[0] = static_cast<u8>(FSFunction::ChangeDirectory);
            response[1] = tan;
            response[2] = static_cast<u8>(FSError::Success);
            return response;
        }

        // ─── Helper Functions ────────────────────────────────────────────────────
        bool wildcard_match(const dp::String &str, const dp::String &pattern) {
            // Simple wildcard matching: * matches any sequence, ? matches single char
            usize s = 0, p = 0;
            usize star_idx = dp::String::npos, match_idx = 0;

            while (s < str.size()) {
                if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == str[s])) {
                    ++s;
                    ++p;
                } else if (p < pattern.size() && pattern[p] == '*') {
                    star_idx = p;
                    match_idx = s;
                    ++p;
                } else if (star_idx != dp::String::npos) {
                    p = star_idx + 1;
                    ++match_idx;
                    s = match_idx;
                } else {
                    return false;
                }
            }

            while (p < pattern.size() && pattern[p] == '*') {
                ++p;
            }

            return p == pattern.size();
        }

        dp::Vector<u8> encode_error_response(u8 function, TAN tan, FSError error) {
            dp::Vector<u8> response(8, 0xFF);
            response[0] = function;
            response[1] = tan;
            response[2] = static_cast<u8>(error);

            echo::category("isobus.fs.server")
                .debug("Error response: function=", static_cast<u32>(function), " TAN=", static_cast<u32>(tan),
                       " error=", fs_error_to_string(error));

            return response;
        }

        void send_response(Address client, const dp::Vector<u8> &data) {
            // TODO: Look up client ControlFunction from address
            net_.send(PGN_FILE_SERVER_TO_CLIENT, data, cf_);
        }

        void broadcast_status() {
            FileServerStatus status;
            status.busy = busy_;
            status.number_of_open_files = static_cast<u8>(open_files_.size());

            auto data = status.encode();
            net_.send(PGN_FILE_SERVER_TO_CLIENT, data, cf_);

            echo::category("isobus.fs.server").trace("Status broadcast: busy=", busy_);
        }

        FileHandle allocate_handle() {
            // Simple sequential allocation with wrap-around
            for (u8 i = 0; i < 255; ++i) {
                FileHandle candidate = next_handle_++;
                if (next_handle_ == 0 || next_handle_ == INVALID_FILE_HANDLE) {
                    next_handle_ = 1;
                }

                // Check if handle is in use
                bool in_use = false;
                for (const auto &f : open_files_) {
                    if (f.handle == candidate) {
                        in_use = true;
                        break;
                    }
                }

                if (!in_use && candidate != INVALID_FILE_HANDLE && candidate != RESERVED_FILE_HANDLE_0) {
                    return candidate;
                }
            }

            return INVALID_FILE_HANDLE;
        }

        void cleanup_expired_tan_cache() {
            for (auto &[addr, client] : clients_) {
                for (auto it = client.tan_cache.begin(); it != client.tan_cache.end();) {
                    if (it->second.is_expired(current_time_ms_, config_.tan_cache_timeout_ms)) {
                        client.tan_cache.erase(it->first);
                        it = client.tan_cache.begin(); // Restart iteration
                    } else {
                        ++it;
                    }
                }
            }
        }

        void cleanup_disconnected_clients() {
            dp::Vector<Address> to_remove;

            for (auto &[addr, client] : clients_) {
                if (!client.is_connected(current_time_ms_, config_.ccm_timeout_ms)) {
                    // Close all client files
                    for (auto it = open_files_.begin(); it != open_files_.end();) {
                        if (it->owner == addr) {
                            echo::category("isobus.fs.server")
                                .debug("Auto-closing file handle ", static_cast<u32>(it->handle),
                                       " for disconnected client ", addr);
                            it = open_files_.erase(it);
                        } else {
                            ++it;
                        }
                    }

                    to_remove.push_back(addr);
                }
            }

            for (Address addr : to_remove) {
                echo::category("isobus.fs.server").info("Client disconnected: ", addr);
                on_client_disconnected.emit(addr);
                clients_.erase(addr);
            }
        }

        // ─── Volume State Machine Update ─────────────────────────────────────────
        void update_volume_state_machine(u32 elapsed_ms) {
            auto current = volume_state_.state();

            switch (current) {
            case VolumeState::Present:
                // Check if any files are open
                if (!open_files_.empty()) {
                    volume_state_.transition(VolumeState::InUse);
                    echo::category("isobus.fs.server").debug("Volume state: PRESENT -> IN_USE");
                }
                break;

            case VolumeState::InUse:
                // Check if all files are closed
                if (open_files_.empty()) {
                    volume_state_.transition(VolumeState::Present);
                    echo::category("isobus.fs.server").debug("Volume state: IN_USE -> PRESENT");
                }
                break;

            case VolumeState::PreparingForRemoval: {
                volume_removal_timer_ms_ += elapsed_ms;

                // Check conditions for completion
                bool all_files_closed = open_files_.empty();
                bool no_maintain_requests = volume_maintain_requests_.empty();
                bool timeout_expired = volume_removal_timer_ms_ >= volume_max_removal_time_ms_;

                if ((all_files_closed && no_maintain_requests) || timeout_expired) {
                    volume_state_.transition(VolumeState::Removed);

                    if (timeout_expired) {
                        echo::category("isobus.fs.server")
                            .warn("Volume removal timeout expired, forcing REMOVED state");
                    } else {
                        echo::category("isobus.fs.server").info("Volume removal complete, state: REMOVED");
                    }

                    // Force close any remaining files
                    if (!open_files_.empty()) {
                        echo::category("isobus.fs.server")
                            .warn("Force-closing ", open_files_.size(), " remaining files");
                        open_files_.clear();
                    }

                    broadcast_volume_status();
                    on_volume_removed.emit();
                }
                break;
            }

            case VolumeState::Removed:
                // Waiting for reinsertion
                break;
            }
        }

        // ─── Volume Status Broadcast ─────────────────────────────────────────────
        void broadcast_volume_status() {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = static_cast<u8>(FSFunction::VolumeStatus);
            data[1] = 0xFF; // No TAN for broadcasts
            data[2] = static_cast<u8>(volume_state_.state());
            data[3] = static_cast<u8>(open_files_.size());

            // Broadcast to all clients (0xFF = global)
            net_.send(PGN_FILE_SERVER_TO_CLIENT, data, cf_);

            echo::category("isobus.fs.server")
                .trace("Volume status broadcast: state=", static_cast<u32>(volume_state_.state()),
                       " open_files=", open_files_.size());
        }
    };

} // namespace agrobus::isobus::fs
