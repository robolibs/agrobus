#pragma once

#include "error_codes.hpp"
#include "types.hpp"
#include <agrobus/net/constants.hpp>
#include <agrobus/net/error.hpp>
#include <agrobus/net/event.hpp>
#include <agrobus/net/internal_cf.hpp>
#include <agrobus/net/message.hpp>
#include <agrobus/net/network_manager.hpp>
#include <datapod/datapod.hpp>
#include <echo/echo.hpp>

namespace agrobus::isobus::fs {
    using namespace agrobus::net;

    // ═════════════════════════════════════════════════════════════════════════════
    // File Client (ISO 11783-13)
    // Client-side implementation for accessing file servers
    // ═════════════════════════════════════════════════════════════════════════════

    // ─── Pending Request ─────────────────────────────────────────────────────────
    struct PendingRequest {
        TAN tan;
        FSFunction function;
        u32 timestamp_ms = 0;
        dp::Vector<u8> request_data;
        std::function<void(const dp::Vector<u8> &)> callback;

        bool is_expired(u32 current_time_ms, u32 timeout_ms) const {
            return (current_time_ms - timestamp_ms) > timeout_ms;
        }
    };

    // ─── File Client State ───────────────────────────────────────────────────────
    enum class ClientState { Disconnected, WaitingForStatus, Connected, Error };

    // ─── Open File Info ──────────────────────────────────────────────────────────
    struct OpenFileInfo {
        FileHandle handle = INVALID_FILE_HANDLE;
        dp::String path;
        OpenFlags flags = OpenFlags::Read;
        u32 position = 0;
        u32 size = 0;
    };

    // ─── File Client Configuration ───────────────────────────────────────────────
    struct FileClientConfig {
        u32 ccm_interval_ms = 2000;          // Send CCM every 2s
        u32 request_timeout_ms = 6000;       // Request timeout (6s)
        u32 server_status_timeout_ms = 6000; // Server status timeout (6s)
        u32 retry_delay_ms = 500;            // Delay before retry
        u8 max_retries_ = 3;                 // Max request retries

        FileClientConfig &ccm_interval(u32 ms) {
            ccm_interval_ms = ms;
            return *this;
        }

        FileClientConfig &request_timeout(u32 ms) {
            request_timeout_ms = ms;
            return *this;
        }

        FileClientConfig &max_retries(u8 n) {
            max_retries_ = n;
            return *this;
        }
    };

    // ─── File Client ─────────────────────────────────────────────────────────────
    class FileClient {
        IsoNet &net_;
        InternalCF *cf_;
        Address server_address_ = NULL_ADDRESS;
        FileClientConfig config_;

        // Connection state
        ClientState state_ = ClientState::Disconnected;
        u32 ccm_timer_ms_ = 0;
        u32 server_status_timer_ms_ = 0;
        u32 current_time_ms_ = 0;

        // TAN management
        TAN next_tan_ = 0;
        dp::Map<TAN, PendingRequest> pending_requests_;

        // Server properties
        dp::Optional<FileServerProperties> server_properties_;
        dp::Optional<FileServerStatus> server_status_;

        // Open files
        dp::Map<FileHandle, OpenFileInfo> open_files_;

        // Current directory
        dp::String current_directory_ = "\\";

      public:
        FileClient(IsoNet &net, InternalCF *cf, FileClientConfig config = {}) : net_(net), cf_(cf), config_(config) {}

        // ─── Initialization ──────────────────────────────────────────────────────
        Result<void> initialize() {
            if (!cf_) {
                return Result<void>::err(Error::invalid_state("control function not set"));
            }

            // Register for server responses
            net_.register_pgn_callback(PGN_FILE_SERVER_TO_CLIENT,
                                       [this](const Message &msg) { handle_server_response(msg); });

            echo::category("isobus.fs.client").info("File client initialized");
            return {};
        }

        // ─── Connection Management ───────────────────────────────────────────────
        Result<void> connect_to_server(Address server) {
            if (state_ != ClientState::Disconnected) {
                return Result<void>::err(Error::invalid_state("already connecting or connected"));
            }

            server_address_ = server;
            state_ = ClientState::WaitingForStatus;
            ccm_timer_ms_ = 0;
            server_status_timer_ms_ = 0;

            echo::category("isobus.fs.client").info("Connecting to file server: ", server);

            // Request server properties
            request_server_properties();

            return {};
        }

        void disconnect() {
            // Close all open files
            dp::Vector<FileHandle> handles_to_close;
            for (const auto &[handle, info] : open_files_) {
                handles_to_close.push_back(handle);
            }

            for (FileHandle handle : handles_to_close) {
                close_file(handle);
            }

            state_ = ClientState::Disconnected;
            server_address_ = NULL_ADDRESS;
            pending_requests_.clear();
            open_files_.clear();
            current_directory_ = "\\";

            echo::category("isobus.fs.client").info("Disconnected from file server");
            on_disconnected.emit();
        }

        bool is_connected() const { return state_ == ClientState::Connected; }

        ClientState get_state() const { return state_; }

        // ─── File Operations ─────────────────────────────────────────────────────
        void open_file(const dp::String &path, OpenFlags flags, std::function<void(Result<FileHandle>)> callback) {

            if (!is_connected()) {
                callback(Result<FileHandle>::err(Error::invalid_state("not connected")));
                return;
            }

            // Build request
            dp::Vector<u8> request(8, 0xFF);
            request[0] = static_cast<u8>(FSFunction::OpenFile);
            TAN tan = allocate_tan();
            request[1] = tan;
            request[2] = static_cast<u8>(path.size());
            request[3] = static_cast<u8>(flags);

            // Add path (may require TP for long paths)
            for (usize i = 0; i < path.size() && i + 4 < 8; ++i) {
                request[4 + i] = path[i];
            }

            // Send request and register callback
            send_request(tan, FSFunction::OpenFile, request,
                         [this, path, flags, callback](const dp::Vector<u8> &response) {
                             handle_open_response(path, flags, response, callback);
                         });

            echo::category("isobus.fs.client").debug("Open file request: ", path);
        }

        void close_file(FileHandle handle) {
            if (!open_files_.count(handle)) {
                echo::category("isobus.fs.client").warn("Close: invalid handle ", static_cast<u32>(handle));
                return;
            }

            // Build request
            dp::Vector<u8> request(8, 0xFF);
            request[0] = static_cast<u8>(FSFunction::CloseFile);
            TAN tan = allocate_tan();
            request[1] = tan;
            request[2] = handle;

            send_request(tan, FSFunction::CloseFile, request,
                         [this, handle](const dp::Vector<u8> &response) { handle_close_response(handle, response); });

            echo::category("isobus.fs.client").debug("Close file request: handle=", static_cast<u32>(handle));
        }

        void read_file(FileHandle handle, u8 count, std::function<void(Result<dp::Vector<u8>>)> callback) {

            if (!open_files_.count(handle)) {
                callback(Result<dp::Vector<u8>>::err(Error::invalid_state("invalid handle")));
                return;
            }

            // Build request
            dp::Vector<u8> request(8, 0xFF);
            request[0] = static_cast<u8>(FSFunction::ReadFile);
            TAN tan = allocate_tan();
            request[1] = tan;
            request[2] = handle;
            request[3] = count;

            send_request(tan, FSFunction::ReadFile, request, [this, handle, callback](const dp::Vector<u8> &response) {
                handle_read_response(handle, response, callback);
            });

            echo::category("isobus.fs.client")
                .trace("Read file request: handle=", static_cast<u32>(handle), " count=", static_cast<u32>(count));
        }

        void write_file(FileHandle handle, const dp::Vector<u8> &data, std::function<void(Result<u8>)> callback) {

            if (!open_files_.count(handle)) {
                callback(Result<u8>::err(Error::invalid_state("invalid handle")));
                return;
            }

            // Build request
            dp::Vector<u8> request(8, 0xFF);
            request[0] = static_cast<u8>(FSFunction::WriteFile);
            TAN tan = allocate_tan();
            request[1] = tan;
            request[2] = handle;
            request[3] = static_cast<u8>(data.size());

            // Add data
            for (usize i = 0; i < data.size() && i + 4 < 8; ++i) {
                request[4 + i] = data[i];
            }

            send_request(tan, FSFunction::WriteFile, request, [this, handle, callback](const dp::Vector<u8> &response) {
                handle_write_response(handle, response, callback);
            });

            echo::category("isobus.fs.client")
                .trace("Write file request: handle=", static_cast<u32>(handle), " bytes=", data.size());
        }

        void seek_file(FileHandle handle, u32 position, std::function<void(Result<void>)> callback) {

            if (!open_files_.count(handle)) {
                callback(Result<void>::err(Error::invalid_state("invalid handle")));
                return;
            }

            // Build request
            dp::Vector<u8> request(8, 0xFF);
            request[0] = static_cast<u8>(FSFunction::SeekFile);
            TAN tan = allocate_tan();
            request[1] = tan;
            request[2] = handle;
            request[3] = static_cast<u8>(position & 0xFF);
            request[4] = static_cast<u8>((position >> 8) & 0xFF);
            request[5] = static_cast<u8>((position >> 16) & 0xFF);
            request[6] = static_cast<u8>((position >> 24) & 0xFF);

            send_request(tan, FSFunction::SeekFile, request,
                         [this, handle, position, callback](const dp::Vector<u8> &response) {
                             handle_seek_response(handle, position, response, callback);
                         });

            echo::category("isobus.fs.client")
                .trace("Seek file request: handle=", static_cast<u32>(handle), " pos=", position);
        }

        // ─── Directory Operations ────────────────────────────────────────────────
        void get_current_directory(std::function<void(Result<dp::String>)> callback) {
            if (!is_connected()) {
                callback(Result<dp::String>::err(Error::invalid_state("not connected")));
                return;
            }

            dp::Vector<u8> request(8, 0xFF);
            request[0] = static_cast<u8>(FSFunction::GetCurrentDirectory);
            TAN tan = allocate_tan();
            request[1] = tan;

            send_request(tan, FSFunction::GetCurrentDirectory, request,
                         [this, callback](const dp::Vector<u8> &response) {
                             handle_get_directory_response(response, callback);
                         });
        }

        void change_directory(const dp::String &path, std::function<void(Result<void>)> callback) {

            if (!is_connected()) {
                callback(Result<void>::err(Error::invalid_state("not connected")));
                return;
            }

            dp::Vector<u8> request(8, 0xFF);
            request[0] = static_cast<u8>(FSFunction::ChangeDirectory);
            TAN tan = allocate_tan();
            request[1] = tan;
            request[2] = static_cast<u8>(path.size());

            // Add path
            for (usize i = 0; i < path.size() && i + 3 < 8; ++i) {
                request[3 + i] = path[i];
            }

            send_request(tan, FSFunction::ChangeDirectory, request,
                         [this, path, callback](const dp::Vector<u8> &response) {
                             handle_change_directory_response(path, response, callback);
                         });

            echo::category("isobus.fs.client").debug("Change directory request: ", path);
        }

        // ─── Query Operations ────────────────────────────────────────────────────
        void request_server_properties() {
            dp::Vector<u8> request(8, 0xFF);
            request[0] = static_cast<u8>(FSFunction::GetFileServerProperties);
            TAN tan = allocate_tan();
            request[1] = tan;

            send_request(tan, FSFunction::GetFileServerProperties, request,
                         [this](const dp::Vector<u8> &response) { handle_properties_response(response); });
        }

        dp::Optional<FileServerProperties> get_server_properties() const { return server_properties_; }

        dp::Optional<FileServerStatus> get_server_status() const { return server_status_; }

        // ─── Events ──────────────────────────────────────────────────────────────
        Event<> on_connected;
        Event<> on_disconnected;
        Event<FSError> on_error;
        Event<FileHandle, dp::String> on_file_opened;
        Event<FileHandle> on_file_closed;

        // ─── Update Loop ─────────────────────────────────────────────────────────
        void update(u32 elapsed_ms) {
            current_time_ms_ += elapsed_ms;

            // Send CCM (Client Connection Maintenance) if connected
            if (is_connected()) {
                ccm_timer_ms_ += elapsed_ms;
                if (ccm_timer_ms_ >= config_.ccm_interval_ms) {
                    ccm_timer_ms_ = 0;
                    send_ccm();
                }
            }

            // Check server status timeout
            if (state_ == ClientState::WaitingForStatus || state_ == ClientState::Connected) {
                server_status_timer_ms_ += elapsed_ms;
                if (server_status_timer_ms_ >= config_.server_status_timeout_ms) {
                    echo::category("isobus.fs.client").warn("Server status timeout, disconnecting");
                    disconnect();
                }
            }

            // Check for expired requests and retry
            check_expired_requests();
        }

      private:
        // ─── Message Handling ────────────────────────────────────────────────────
        void handle_server_response(const Message &msg) {
            if (msg.source != server_address_ && server_address_ != NULL_ADDRESS) {
                return; // Ignore messages from other servers
            }

            if (msg.data.size() < 2) {
                return;
            }

            u8 function = msg.data[0];
            TAN tan = msg.data[1];

            // Reset server status timeout
            server_status_timer_ms_ = 0;

            // Check if this is a status broadcast (TAN = 0xFF)
            if (tan == 0xFF) {
                handle_status_broadcast(msg.data);
                return;
            }

            // Find pending request
            auto it = pending_requests_.find(tan);
            if (it != pending_requests_.end()) {
                // Call the callback
                if (it->second.callback) {
                    it->second.callback(msg.data);
                }

                // Remove pending request
                pending_requests_.erase(it);
            }
        }

        void handle_status_broadcast(const dp::Vector<u8> &data) {
            if (data.size() < 3)
                return;

            FSFunction function = static_cast<FSFunction>(data[0]);

            if (function == FSFunction::FileServerStatus) {
                server_status_ = FileServerStatus::decode(data);
                echo::category("isobus.fs.client")
                    .trace("Server status: busy=", server_status_->busy,
                           " open_files=", static_cast<u32>(server_status_->number_of_open_files));
            } else if (function == FSFunction::VolumeStatus) {
                VolumeState vol_state = static_cast<VolumeState>(data[2]);
                echo::category("isobus.fs.client").info("Volume status: ", static_cast<u32>(vol_state));
            }
        }

        // ─── Response Handlers ───────────────────────────────────────────────────
        void handle_properties_response(const dp::Vector<u8> &response) {
            if (response.size() < 3)
                return;

            FSError error = static_cast<FSError>(response[2]);
            if (error != FSError::Success) {
                echo::category("isobus.fs.client").error("Failed to get server properties");
                return;
            }

            server_properties_ = FileServerProperties::decode(response);

            if (state_ == ClientState::WaitingForStatus) {
                state_ = ClientState::Connected;
                echo::category("isobus.fs.client").info("Connected to file server");
                on_connected.emit();
            }
        }

        void handle_open_response(const dp::String &path, OpenFlags flags, const dp::Vector<u8> &response,
                                  std::function<void(Result<FileHandle>)> callback) {

            if (response.size() < 4) {
                callback(Result<FileHandle>::err(Error::invalid_state("malformed response")));
                return;
            }

            FSError error = static_cast<FSError>(response[2]);
            if (error != FSError::Success) {
                echo::category("isobus.fs.client").error("Open failed: ", fs_error_to_string(error));
                callback(Result<FileHandle>::err(Error::invalid_state(fs_error_to_string(error))));
                on_error.emit(error);
                return;
            }

            FileHandle handle = response[3];

            // Track open file
            OpenFileInfo info;
            info.handle = handle;
            info.path = path;
            info.flags = flags;
            info.position = 0;
            open_files_[handle] = info;

            echo::category("isobus.fs.client").info("File opened: ", path, " handle=", static_cast<u32>(handle));
            on_file_opened.emit(handle, path);

            callback(Result<FileHandle>::ok(handle));
        }

        void handle_close_response(FileHandle handle, const dp::Vector<u8> &response) {
            open_files_.erase(handle);
            echo::category("isobus.fs.client").debug("File closed: handle=", static_cast<u32>(handle));
            on_file_closed.emit(handle);
        }

        void handle_read_response(FileHandle handle, const dp::Vector<u8> &response,
                                  std::function<void(Result<dp::Vector<u8>>)> callback) {

            if (response.size() < 4) {
                callback(Result<dp::Vector<u8>>::err(Error::invalid_state("malformed response")));
                return;
            }

            FSError error = static_cast<FSError>(response[2]);
            if (error != FSError::Success) {
                if (error == FSError::EndOfFile) {
                    // EOF is not an error, return empty data
                    callback(Result<dp::Vector<u8>>::ok(dp::Vector<u8>()));
                } else {
                    echo::category("isobus.fs.client").error("Read failed: ", fs_error_to_string(error));
                    callback(Result<dp::Vector<u8>>::err(Error::invalid_state(fs_error_to_string(error))));
                    on_error.emit(error);
                }
                return;
            }

            u8 count = response[3];
            dp::Vector<u8> data;

            for (u8 i = 0; i < count && i + 4 < response.size(); ++i) {
                data.push_back(response[4 + i]);
            }

            // Update position
            if (open_files_.count(handle)) {
                open_files_[handle].position += count;
            }

            callback(Result<dp::Vector<u8>>::ok(data));
        }

        void handle_write_response(FileHandle handle, const dp::Vector<u8> &response,
                                   std::function<void(Result<u8>)> callback) {

            if (response.size() < 4) {
                callback(Result<u8>::err(Error::invalid_state("malformed response")));
                return;
            }

            FSError error = static_cast<FSError>(response[2]);
            if (error != FSError::Success) {
                echo::category("isobus.fs.client").error("Write failed: ", fs_error_to_string(error));
                callback(Result<u8>::err(Error::invalid_state(fs_error_to_string(error))));
                on_error.emit(error);
                return;
            }

            u8 written = response[3];

            // Update position
            if (open_files_.count(handle)) {
                open_files_[handle].position += written;
            }

            callback(Result<u8>::ok(written));
        }

        void handle_seek_response(FileHandle handle, u32 position, const dp::Vector<u8> &response,
                                  std::function<void(Result<void>)> callback) {

            if (response.size() < 3) {
                callback(Result<void>::err(Error::invalid_state("malformed response")));
                return;
            }

            FSError error = static_cast<FSError>(response[2]);
            if (error != FSError::Success) {
                echo::category("isobus.fs.client").error("Seek failed: ", fs_error_to_string(error));
                callback(Result<void>::err(Error::invalid_state(fs_error_to_string(error))));
                on_error.emit(error);
                return;
            }

            // Update position
            if (open_files_.count(handle)) {
                open_files_[handle].position = position;
            }

            callback(Result<void>::ok());
        }

        void handle_get_directory_response(const dp::Vector<u8> &response,
                                           std::function<void(Result<dp::String>)> callback) {

            if (response.size() < 4) {
                callback(Result<dp::String>::err(Error::invalid_state("malformed response")));
                return;
            }

            FSError error = static_cast<FSError>(response[2]);
            if (error != FSError::Success) {
                callback(Result<dp::String>::err(Error::invalid_state(fs_error_to_string(error))));
                return;
            }

            u8 path_len = response[3];
            dp::String path;

            for (u8 i = 0; i < path_len && i + 4 < response.size(); ++i) {
                path += static_cast<char>(response[4 + i]);
            }

            current_directory_ = path;
            callback(Result<dp::String>::ok(path));
        }

        void handle_change_directory_response(const dp::String &path, const dp::Vector<u8> &response,
                                              std::function<void(Result<void>)> callback) {

            if (response.size() < 3) {
                callback(Result<void>::err(Error::invalid_state("malformed response")));
                return;
            }

            FSError error = static_cast<FSError>(response[2]);
            if (error != FSError::Success) {
                echo::category("isobus.fs.client").error("Change directory failed: ", fs_error_to_string(error));
                callback(Result<void>::err(Error::invalid_state(fs_error_to_string(error))));
                on_error.emit(error);
                return;
            }

            current_directory_ = path;
            callback(Result<void>::ok());
        }

        // ─── Helper Functions ────────────────────────────────────────────────────
        TAN allocate_tan() {
            TAN tan = next_tan_++;
            if (next_tan_ == INVALID_TAN) {
                next_tan_ = 0; // Wrap around
            }
            return tan;
        }

        void send_request(TAN tan, FSFunction function, const dp::Vector<u8> &request,
                          std::function<void(const dp::Vector<u8> &)> callback) {

            // Store pending request
            PendingRequest pending;
            pending.tan = tan;
            pending.function = function;
            pending.timestamp_ms = current_time_ms_;
            pending.request_data = request;
            pending.callback = callback;
            pending_requests_[tan] = pending;

            // Send to server (broadcast to all file servers)
            net_.send(PGN_FILE_CLIENT_TO_SERVER, request, cf_);
        }

        void send_ccm() {
            CCMMessage ccm;
            ccm.version = 1;
            ccm.tan = allocate_tan();

            auto data = ccm.encode();
            data[0] = 0xFF; // Special CCM function code

            net_.send(PGN_FILE_CLIENT_TO_SERVER, data, cf_);

            echo::category("isobus.fs.client").trace("CCM sent, TAN=", static_cast<u32>(ccm.tan));
        }

        void check_expired_requests() {
            dp::Vector<TAN> expired;

            for (const auto &[tan, req] : pending_requests_) {
                if (req.is_expired(current_time_ms_, config_.request_timeout_ms)) {
                    expired.push_back(tan);
                }
            }

            for (TAN tan : expired) {
                echo::category("isobus.fs.client").warn("Request timeout: TAN=", static_cast<u32>(tan));

                // TODO: Implement retry logic
                pending_requests_.erase(tan);
            }
        }
    };

} // namespace agrobus::isobus::fs
