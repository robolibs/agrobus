#pragma once

#include <agrobus/net/constants.hpp>
#include <agrobus/net/control_function.hpp>
#include <agrobus/net/error.hpp>
#include <agrobus/net/event.hpp>
#include <agrobus/net/internal_cf.hpp>
#include <agrobus/net/message.hpp>
#include <agrobus/net/network_manager.hpp>
#include <agrobus/net/types.hpp>
#include <datapod/datapod.hpp>
#include <echo/echo.hpp>

namespace agrobus::isobus {
    using namespace agrobus::net;

    // ─── ISO 11783-13 File Transfer Protocol ─────────────────────────────────────

    // ─── ISO 11783-13 File operations ────────────────────────────────────────────
    enum class FileOperation : u8 {
        Read = 0x01,
        Write = 0x02,
        Delete = 0x03,
        List = 0x04,
        GetAttributes = 0x05,
        SetAttributes = 0x06,
        OpenFile = 0x10,
        CloseFile = 0x11,
        ReadData = 0x12,
        WriteData = 0x13,
        SeekFile = 0x14,
        GetCurrentDir = 0x20,
        ChangeCurrentDir = 0x21,
        MakeDir = 0x22,
        RemoveDir = 0x23,
        MoveFile = 0x30,
        CopyFile = 0x31,
        GetFileSize = 0x40,
        GetFreeSpace = 0x41,
        GetVolumeInfo = 0x50,
        GetServerStatus = 0x60
    };

    enum class FileTransferError : u8 {
        NoError = 0x00,
        FileNotFound = 0x01,
        AccessDenied = 0x02,
        DiskFull = 0x03,
        InvalidFilename = 0x04,
        ServerBusy = 0x05,
        InvalidHandle = 0x06,
        EndOfFile = 0x07,
        VolumeNotMounted = 0x08,
        IOError = 0x09,
        InvalidSeekPosition = 0x0A,
        InvalidParameter = 0x0B,
        FileAlreadyOpen = 0x0C,
        DirectoryNotEmpty = 0x0D,
        Unknown = 0xFF
    };

    enum class FileServerState : u8 { Idle, Active, Busy };
    enum class FileClientState : u8 { Idle, Transferring, Complete, Error };

    // ─── File attributes (ISO 11783-13) ───────────────────────────────────────────
    enum class FileAttribute : u8 {
        ReadOnly = 0x01,
        Hidden = 0x02,
        System = 0x04,
        Directory = 0x10,
        Archive = 0x20,
        Volume = 0x40
    };

    struct FileProperties {
        dp::String name;
        u32 size_bytes = 0;
        u8 attributes = 0;
        u32 date = 0; // Packed date (DOS format)
        u32 time = 0; // Packed time (DOS format)

        bool is_directory() const noexcept { return (attributes & static_cast<u8>(FileAttribute::Directory)) != 0; }
        bool is_read_only() const noexcept { return (attributes & static_cast<u8>(FileAttribute::ReadOnly)) != 0; }
    };

    // ─── Volume information (ISO 11783-13) ────────────────────────────────────────
    struct VolumeInfo {
        dp::String name;
        u32 total_bytes = 0;
        u32 free_bytes = 0;
        u8 attributes = 0;
        bool removable = false;
    };

    // ─── File server status message (sent periodically per ISO 11783-13) ──────────
    inline constexpr u32 FILE_SERVER_STATUS_INTERVAL_MS = 2000;
    inline constexpr u32 FILE_SERVER_BUSY_STATUS_INTERVAL_MS = 200;

    // ─── File client request timeout (ISO 11783-13: wait for response) ──────────
    inline constexpr u32 FS_REQUEST_TIMEOUT_MS = 6000;

    // ─── File Server Config ──────────────────────────────────────────────────────
    struct FileServerConfig {
        dp::String base_path = "";
        u32 status_interval_ms = FILE_SERVER_STATUS_INTERVAL_MS;
        u8 max_open_files = 16;
        dp::String volume_name = "ISOBUS";
        u32 volume_total_bytes = 1024 * 1024; // 1 MB default
        u32 volume_free_bytes = 512 * 1024;   // 512 KB default

        FileServerConfig &path(dp::String p) {
            base_path = std::move(p);
            return *this;
        }
        FileServerConfig &status_interval(u32 ms) {
            status_interval_ms = ms;
            return *this;
        }
        FileServerConfig &max_files(u8 n) {
            max_open_files = n;
            return *this;
        }
        FileServerConfig &volume(dp::String name, u32 total, u32 free) {
            volume_name = std::move(name);
            volume_total_bytes = total;
            volume_free_bytes = free;
            return *this;
        }
    };

    // ─── Open file state ──────────────────────────────────────────────────────────
    struct OpenFileState {
        dp::String filename;
        dp::Vector<u8> data; // File content buffer
        u32 position = 0;    // Current read/write position
        bool writable = false;
    };

    // ─── File Server ─────────────────────────────────────────────────────────────
    class FileServer {
        IsoNet &net_;
        InternalCF *cf_;
        dp::String base_path_;
        dp::Vector<FileProperties> file_list_;
        dp::Map<u8, OpenFileState> open_files_; // handle -> state
        u8 next_handle_ = 1;
        u32 status_timer_ms_ = 0;
        FileServerConfig config_;
        VolumeInfo volume_;
        bool busy_ = false;

      public:
        FileServer(IsoNet &net, InternalCF *cf, FileServerConfig config = {})
            : net_(net), cf_(cf), base_path_(std::move(config.base_path)), config_(config) {
            volume_.name = config_.volume_name;
            volume_.total_bytes = config_.volume_total_bytes;
            volume_.free_bytes = config_.volume_free_bytes;
        }

        Result<void> initialize() {
            if (!cf_) {
                return Result<void>::err(Error::invalid_state("control function not set"));
            }
            net_.register_pgn_callback(PGN_FILE_CLIENT_TO_SERVER,
                                       [this](const Message &msg) { handle_client_request(msg); });
            echo::category("isobus.protocol.file_server").info("File server initialized: ", base_path_);
            return {};
        }

        Result<void> set_base_path(dp::String path) {
            base_path_ = std::move(path);
            return {};
        }

        // Register files that this server provides (with properties)
        Result<void> add_file(dp::String filename, dp::Vector<u8> data = {}, u8 attributes = 0) {
            FileProperties props;
            props.name = std::move(filename);
            props.size_bytes = static_cast<u32>(data.size());
            props.attributes = attributes;
            file_list_.push_back(std::move(props));
            // Store content in the file content map
            if (!data.empty()) {
                file_contents_[file_list_.back().name] = std::move(data);
            }
            return {};
        }

        Result<void> remove_file(const dp::String &filename) {
            for (auto it = file_list_.begin(); it != file_list_.end(); ++it) {
                if (it->name == filename) {
                    file_list_.erase(it);
                    file_contents_.erase(filename);
                    return {};
                }
            }
            return Result<void>::err(Error::invalid_state("file not found"));
        }

        // Set file data content
        Result<void> set_file_data(const dp::String &filename, dp::Vector<u8> data) {
            for (auto &f : file_list_) {
                if (f.name == filename) {
                    f.size_bytes = static_cast<u32>(data.size());
                    file_contents_[filename] = std::move(data);
                    return {};
                }
            }
            return Result<void>::err(Error::invalid_state("file not found"));
        }

        dp::Optional<dp::Vector<u8>> get_file_data(const dp::String &filename) const {
            auto it = file_contents_.find(filename);
            if (it != file_contents_.end())
                return it->second;
            return dp::nullopt;
        }

        const dp::Vector<FileProperties> &files() const noexcept { return file_list_; }
        const VolumeInfo &volume() const noexcept { return volume_; }
        void set_volume(VolumeInfo info) { volume_ = std::move(info); }

        const dp::String &base_path() const noexcept { return base_path_; }

        // ─── Busy-status cadence (ISO 11783-13) ────────────────────────────────
        void set_busy(bool b) {
            busy_ = b;
            echo::category("isobus.protocol.file_server").debug("busy state: ", b ? "true" : "false");
        }
        bool is_busy() const noexcept { return busy_; }

        u32 effective_status_interval() const noexcept {
            return busy_ ? FILE_SERVER_BUSY_STATUS_INTERVAL_MS : config_.status_interval_ms;
        }

        // Events
        Event<dp::String, Address> on_file_read_request;
        Event<dp::String, dp::Vector<u8>, Address> on_file_write_complete; // filename, data, source
        Event<dp::String, Address> on_file_delete_request;

        void update(u32 elapsed_ms) {
            status_timer_ms_ += elapsed_ms;
            u32 interval = effective_status_interval();
            if (status_timer_ms_ >= interval) {
                status_timer_ms_ -= interval;
                send_server_status();
            }
        }

      private:
        dp::Map<dp::String, dp::Vector<u8>> file_contents_; // filename -> data

        void handle_client_request(const Message &msg) {
            if (msg.data.empty())
                return;

            auto op = static_cast<FileOperation>(msg.data[0]);

            switch (op) {
            case FileOperation::OpenFile:
                handle_open(msg);
                break;
            case FileOperation::CloseFile:
                handle_close(msg);
                break;
            case FileOperation::ReadData:
                handle_read_data(msg);
                break;
            case FileOperation::WriteData:
                handle_write_data(msg);
                break;
            case FileOperation::SeekFile:
                handle_seek(msg);
                break;
            case FileOperation::List:
                handle_list(msg);
                break;
            case FileOperation::Delete:
                handle_delete(msg);
                break;
            case FileOperation::GetAttributes:
                handle_get_attributes(msg);
                break;
            case FileOperation::GetFileSize:
                handle_get_file_size(msg);
                break;
            case FileOperation::GetFreeSpace:
                handle_get_free_space(msg);
                break;
            case FileOperation::GetVolumeInfo:
                handle_get_volume_info(msg);
                break;
            case FileOperation::GetServerStatus:
                handle_get_server_status(msg);
                break;
            default:
                echo::category("isobus.protocol.file_server").trace("Unhandled file op: ", static_cast<u8>(op));
                break;
            }
        }

        Result<void> send_response(const dp::Vector<u8> &data, Address requester) {
            ControlFunction dest_cf;
            dest_cf.address = requester;
            return net_.send(PGN_FILE_SERVER_TO_CLIENT, data, cf_, &dest_cf, Priority::Default);
        }

        void handle_open(const Message &msg) {
            if (msg.data.size() < 3)
                return;

            u8 name_len = msg.data[1];
            dp::String filename;
            for (usize i = 0; i < name_len && (2 + i) < msg.data.size(); ++i) {
                filename += static_cast<char>(msg.data[2 + i]);
            }

            // Check for max open files
            if (open_files_.size() >= config_.max_open_files) {
                dp::Vector<u8> response(8, 0xFF);
                response[0] = static_cast<u8>(FileOperation::OpenFile);
                response[1] = static_cast<u8>(FileTransferError::ServerBusy);
                send_response(response, msg.source);
                return;
            }

            // Check if file exists
            bool found = false;
            for (const auto &f : file_list_) {
                if (f.name == filename) {
                    found = true;
                    break;
                }
            }

            dp::Vector<u8> response(8, 0xFF);
            response[0] = static_cast<u8>(FileOperation::OpenFile);
            if (found) {
                u8 handle = next_handle_++;
                OpenFileState state;
                state.filename = filename;
                state.position = 0;
                // Load file data if available
                auto it = file_contents_.find(filename);
                if (it != file_contents_.end()) {
                    state.data = it->second;
                }
                open_files_[handle] = std::move(state);
                response[1] = static_cast<u8>(FileTransferError::NoError);
                response[2] = handle;
                echo::category("isobus.protocol.file_server").debug("file opened: ", filename, " handle=", handle);
                on_file_read_request.emit(filename, msg.source);
            } else {
                response[1] = static_cast<u8>(FileTransferError::FileNotFound);
                response[2] = 0;
                echo::category("isobus.protocol.file_server").debug("file not found: ", filename);
            }
            send_response(response, msg.source);
        }

        void handle_close(const Message &msg) {
            if (msg.data.size() < 2)
                return;
            u8 handle = msg.data[1];

            dp::Vector<u8> response(8, 0xFF);
            response[0] = static_cast<u8>(FileOperation::CloseFile);

            auto it = open_files_.find(handle);
            if (it != open_files_.end()) {
                // If file was written, persist data and emit event
                if (it->second.writable && !it->second.data.empty()) {
                    file_contents_[it->second.filename] = it->second.data;
                    // Update file size in properties
                    for (auto &f : file_list_) {
                        if (f.name == it->second.filename) {
                            f.size_bytes = static_cast<u32>(it->second.data.size());
                            break;
                        }
                    }
                    on_file_write_complete.emit(it->second.filename, it->second.data, msg.source);
                }
                open_files_.erase(it);
                response[1] = static_cast<u8>(FileTransferError::NoError);
            } else {
                response[1] = static_cast<u8>(FileTransferError::InvalidHandle);
            }
            send_response(response, msg.source);
        }

        void handle_read_data(const Message &msg) {
            if (msg.data.size() < 4)
                return;
            u8 handle = msg.data[1];
            u16 read_len = static_cast<u16>(msg.data[2]) | (static_cast<u16>(msg.data[3]) << 8);

            auto it = open_files_.find(handle);
            if (it == open_files_.end()) {
                dp::Vector<u8> response(8, 0xFF);
                response[0] = static_cast<u8>(FileOperation::ReadData);
                response[1] = static_cast<u8>(FileTransferError::InvalidHandle);
                send_response(response, msg.source);
                return;
            }

            auto &file = it->second;
            dp::Vector<u8> response;
            response.push_back(static_cast<u8>(FileOperation::ReadData));

            if (file.position >= file.data.size()) {
                response.push_back(static_cast<u8>(FileTransferError::EndOfFile));
                response.push_back(0);
                response.push_back(0); // 0 bytes read
                while (response.size() < 8)
                    response.push_back(0xFF);
            } else {
                u32 avail = static_cast<u32>(file.data.size() - file.position);
                u32 to_read = (read_len > avail) ? avail : read_len;
                response.push_back(static_cast<u8>(FileTransferError::NoError));
                response.push_back(static_cast<u8>(to_read & 0xFF));
                response.push_back(static_cast<u8>((to_read >> 8) & 0xFF));
                for (u32 i = 0; i < to_read; ++i) {
                    response.push_back(file.data[file.position + i]);
                }
                file.position += to_read;
            }
            while (response.size() < 8)
                response.push_back(0xFF);
            send_response(response, msg.source);
        }

        void handle_write_data(const Message &msg) {
            if (msg.data.size() < 4)
                return;
            u8 handle = msg.data[1];
            u16 write_len = static_cast<u16>(msg.data[2]) | (static_cast<u16>(msg.data[3]) << 8);

            auto it = open_files_.find(handle);
            if (it == open_files_.end()) {
                dp::Vector<u8> response(8, 0xFF);
                response[0] = static_cast<u8>(FileOperation::WriteData);
                response[1] = static_cast<u8>(FileTransferError::InvalidHandle);
                send_response(response, msg.source);
                return;
            }

            auto &file = it->second;
            file.writable = true;

            // Append or overwrite at current position
            u16 data_avail = static_cast<u16>(msg.data.size() - 4);
            u16 actual_len = (write_len > data_avail) ? data_avail : write_len;

            if (file.position + actual_len > file.data.size()) {
                file.data.resize(file.position + actual_len);
            }
            for (u16 i = 0; i < actual_len; ++i) {
                file.data[file.position + i] = msg.data[4 + i];
            }
            file.position += actual_len;

            dp::Vector<u8> response(8, 0xFF);
            response[0] = static_cast<u8>(FileOperation::WriteData);
            response[1] = static_cast<u8>(FileTransferError::NoError);
            response[2] = static_cast<u8>(actual_len & 0xFF);
            response[3] = static_cast<u8>((actual_len >> 8) & 0xFF);
            send_response(response, msg.source);
        }

        void handle_seek(const Message &msg) {
            if (msg.data.size() < 6)
                return;
            u8 handle = msg.data[1];
            u32 position = static_cast<u32>(msg.data[2]) | (static_cast<u32>(msg.data[3]) << 8) |
                           (static_cast<u32>(msg.data[4]) << 16) | (static_cast<u32>(msg.data[5]) << 24);

            auto it = open_files_.find(handle);
            dp::Vector<u8> response(8, 0xFF);
            response[0] = static_cast<u8>(FileOperation::SeekFile);

            if (it == open_files_.end()) {
                response[1] = static_cast<u8>(FileTransferError::InvalidHandle);
            } else if (position > it->second.data.size()) {
                response[1] = static_cast<u8>(FileTransferError::InvalidSeekPosition);
            } else {
                it->second.position = position;
                response[1] = static_cast<u8>(FileTransferError::NoError);
                response[2] = static_cast<u8>(position & 0xFF);
                response[3] = static_cast<u8>((position >> 8) & 0xFF);
                response[4] = static_cast<u8>((position >> 16) & 0xFF);
                response[5] = static_cast<u8>((position >> 24) & 0xFF);
            }
            send_response(response, msg.source);
        }

        void handle_list(const Message &msg) {
            dp::Vector<u8> response;
            response.push_back(static_cast<u8>(FileOperation::List));
            response.push_back(static_cast<u8>(FileTransferError::NoError));
            response.push_back(static_cast<u8>(file_list_.size()));

            for (const auto &f : file_list_) {
                response.push_back(static_cast<u8>(f.name.size()));
                for (char c : f.name) {
                    response.push_back(static_cast<u8>(c));
                }
            }
            while (response.size() < 8)
                response.push_back(0xFF);
            send_response(response, msg.source);
        }

        void handle_delete(const Message &msg) {
            if (msg.data.size() < 3)
                return;
            u8 name_len = msg.data[1];
            dp::String filename;
            for (usize i = 0; i < name_len && (2 + i) < msg.data.size(); ++i) {
                filename += static_cast<char>(msg.data[2 + i]);
            }

            on_file_delete_request.emit(filename, msg.source);
            remove_file(filename);

            dp::Vector<u8> response(8, 0xFF);
            response[0] = static_cast<u8>(FileOperation::Delete);
            response[1] = static_cast<u8>(FileTransferError::NoError);
            send_response(response, msg.source);
        }

        void handle_get_attributes(const Message &msg) {
            if (msg.data.size() < 3)
                return;
            u8 name_len = msg.data[1];
            dp::String filename;
            for (usize i = 0; i < name_len && (2 + i) < msg.data.size(); ++i) {
                filename += static_cast<char>(msg.data[2 + i]);
            }

            dp::Vector<u8> response(8, 0xFF);
            response[0] = static_cast<u8>(FileOperation::GetAttributes);

            for (const auto &f : file_list_) {
                if (f.name == filename) {
                    response[1] = static_cast<u8>(FileTransferError::NoError);
                    response[2] = f.attributes;
                    response[3] = static_cast<u8>(f.size_bytes & 0xFF);
                    response[4] = static_cast<u8>((f.size_bytes >> 8) & 0xFF);
                    response[5] = static_cast<u8>((f.size_bytes >> 16) & 0xFF);
                    response[6] = static_cast<u8>((f.size_bytes >> 24) & 0xFF);
                    send_response(response, msg.source);
                    return;
                }
            }
            response[1] = static_cast<u8>(FileTransferError::FileNotFound);
            send_response(response, msg.source);
        }

        void handle_get_file_size(const Message &msg) {
            if (msg.data.size() < 2)
                return;
            u8 handle = msg.data[1];

            dp::Vector<u8> response(8, 0xFF);
            response[0] = static_cast<u8>(FileOperation::GetFileSize);

            auto it = open_files_.find(handle);
            if (it != open_files_.end()) {
                u32 size = static_cast<u32>(it->second.data.size());
                response[1] = static_cast<u8>(FileTransferError::NoError);
                response[2] = static_cast<u8>(size & 0xFF);
                response[3] = static_cast<u8>((size >> 8) & 0xFF);
                response[4] = static_cast<u8>((size >> 16) & 0xFF);
                response[5] = static_cast<u8>((size >> 24) & 0xFF);
            } else {
                response[1] = static_cast<u8>(FileTransferError::InvalidHandle);
            }
            send_response(response, msg.source);
        }

        void handle_get_free_space(const Message &msg) {
            dp::Vector<u8> response(8, 0xFF);
            response[0] = static_cast<u8>(FileOperation::GetFreeSpace);
            response[1] = static_cast<u8>(FileTransferError::NoError);
            response[2] = static_cast<u8>(volume_.free_bytes & 0xFF);
            response[3] = static_cast<u8>((volume_.free_bytes >> 8) & 0xFF);
            response[4] = static_cast<u8>((volume_.free_bytes >> 16) & 0xFF);
            response[5] = static_cast<u8>((volume_.free_bytes >> 24) & 0xFF);
            send_response(response, msg.source);
        }

        void handle_get_volume_info(const Message &msg) {
            dp::Vector<u8> response;
            response.push_back(static_cast<u8>(FileOperation::GetVolumeInfo));
            response.push_back(static_cast<u8>(FileTransferError::NoError));
            // Total space
            response.push_back(static_cast<u8>(volume_.total_bytes & 0xFF));
            response.push_back(static_cast<u8>((volume_.total_bytes >> 8) & 0xFF));
            response.push_back(static_cast<u8>((volume_.total_bytes >> 16) & 0xFF));
            response.push_back(static_cast<u8>((volume_.total_bytes >> 24) & 0xFF));
            // Free space
            response.push_back(static_cast<u8>(volume_.free_bytes & 0xFF));
            response.push_back(static_cast<u8>((volume_.free_bytes >> 8) & 0xFF));
            response.push_back(static_cast<u8>((volume_.free_bytes >> 16) & 0xFF));
            response.push_back(static_cast<u8>((volume_.free_bytes >> 24) & 0xFF));
            // Volume name
            response.push_back(static_cast<u8>(volume_.name.size()));
            for (char c : volume_.name)
                response.push_back(static_cast<u8>(c));
            while (response.size() < 8)
                response.push_back(0xFF);
            send_response(response, msg.source);
        }

        void handle_get_server_status(const Message &msg) { send_server_status_to(msg.source); }

        void send_server_status() {
            // Broadcast server status
            dp::Vector<u8> data(8, 0xFF);
            data[0] = static_cast<u8>(FileOperation::GetServerStatus);
            data[1] = static_cast<u8>(open_files_.size()); // Number of open files
            data[2] = config_.max_open_files;              // Max open files
            data[3] = static_cast<u8>(file_list_.size());  // Number of files
            net_.send(PGN_FILE_SERVER_TO_CLIENT, data, cf_);
        }

        void send_server_status_to(Address dest) {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = static_cast<u8>(FileOperation::GetServerStatus);
            data[1] = static_cast<u8>(open_files_.size());
            data[2] = config_.max_open_files;
            data[3] = static_cast<u8>(file_list_.size());
            ControlFunction dest_cf;
            dest_cf.address = dest;
            net_.send(PGN_FILE_SERVER_TO_CLIENT, data, cf_, &dest_cf, Priority::Default);
        }
    };

    // ─── File Client ─────────────────────────────────────────────────────────────
    class FileClient {
        IsoNet &net_;
        InternalCF *cf_;
        ControlFunction *server_ = nullptr;
        FileClientState state_ = FileClientState::Idle;
        u8 current_handle_ = 0;
        bool request_pending_ = false;
        u32 pending_timeout_ms_ = 0;

      public:
        FileClient(IsoNet &net, InternalCF *cf, ControlFunction *server = nullptr)
            : net_(net), cf_(cf), server_(server) {}

        Result<void> initialize() {
            if (!cf_) {
                return Result<void>::err(Error::invalid_state("control function not set"));
            }
            net_.register_pgn_callback(PGN_FILE_SERVER_TO_CLIENT,
                                       [this](const Message &msg) { handle_server_response(msg); });
            echo::category("isobus.protocol.file_client").debug("initialized");
            return {};
        }

        void set_server(ControlFunction *server) { server_ = server; }

        Result<void> request_open(const dp::String &filename) {
            if (request_pending_) {
                return Result<void>::err(Error::invalid_state("request already pending"));
            }
            echo::category("isobus.protocol.file_client").debug("requesting open: ", filename);
            dp::Vector<u8> data;
            data.push_back(static_cast<u8>(FileOperation::OpenFile));
            data.push_back(static_cast<u8>(filename.size()));
            for (auto c : filename) {
                data.push_back(static_cast<u8>(c));
            }
            while (data.size() < 8)
                data.push_back(0xFF);

            state_ = FileClientState::Transferring;
            request_pending_ = true;
            pending_timeout_ms_ = 0;
            return net_.send(PGN_FILE_CLIENT_TO_SERVER, data, cf_, server_, Priority::Default);
        }

        Result<void> request_close(u8 handle) {
            if (request_pending_) {
                return Result<void>::err(Error::invalid_state("request already pending"));
            }
            dp::Vector<u8> data(8, 0xFF);
            data[0] = static_cast<u8>(FileOperation::CloseFile);
            data[1] = handle;
            request_pending_ = true;
            pending_timeout_ms_ = 0;
            return net_.send(PGN_FILE_CLIENT_TO_SERVER, data, cf_, server_, Priority::Default);
        }

        Result<void> request_list() {
            if (request_pending_) {
                return Result<void>::err(Error::invalid_state("request already pending"));
            }
            echo::category("isobus.protocol.file_client").debug("requesting list");
            dp::Vector<u8> data(8, 0xFF);
            data[0] = static_cast<u8>(FileOperation::List);
            request_pending_ = true;
            pending_timeout_ms_ = 0;
            return net_.send(PGN_FILE_CLIENT_TO_SERVER, data, cf_, server_, Priority::Default);
        }

        Result<void> request_delete(const dp::String &filename) {
            if (request_pending_) {
                return Result<void>::err(Error::invalid_state("request already pending"));
            }
            echo::category("isobus.protocol.file_client").debug("requesting delete: ", filename);
            dp::Vector<u8> data;
            data.push_back(static_cast<u8>(FileOperation::Delete));
            data.push_back(static_cast<u8>(filename.size()));
            for (auto c : filename) {
                data.push_back(static_cast<u8>(c));
            }
            while (data.size() < 8)
                data.push_back(0xFF);
            request_pending_ = true;
            pending_timeout_ms_ = 0;
            return net_.send(PGN_FILE_CLIENT_TO_SERVER, data, cf_, server_, Priority::Default);
        }

        FileClientState state() const noexcept { return state_; }
        u8 current_handle() const noexcept { return current_handle_; }
        bool is_request_pending() const noexcept { return request_pending_; }

        // Events
        Event<u8> on_file_opened;                      // handle
        Event<FileTransferError, dp::String> on_error; // error, context
        Event<u8> on_file_count;                       // number of files
        Event<> on_file_deleted;
        Event<> on_timeout; // request timed out (ISO 11783-13)

        void update(u32 elapsed_ms) {
            if (request_pending_) {
                pending_timeout_ms_ += elapsed_ms;
                if (pending_timeout_ms_ >= FS_REQUEST_TIMEOUT_MS) {
                    request_pending_ = false;
                    pending_timeout_ms_ = 0;
                    echo::category("isobus.protocol.file_client")
                        .warn("request timeout after ", FS_REQUEST_TIMEOUT_MS, " ms");
                    on_timeout.emit();
                }
            }
        }

      private:
        void handle_server_response(const Message &msg) {
            if (msg.data.empty())
                return;

            // Any response clears the pending state (ISO 11783-13 one-inflight rule)
            request_pending_ = false;
            pending_timeout_ms_ = 0;

            auto op = static_cast<FileOperation>(msg.data[0]);
            auto err = (msg.data.size() > 1) ? static_cast<FileTransferError>(msg.data[1]) : FileTransferError::NoError;

            switch (op) {
            case FileOperation::OpenFile:
                if (err == FileTransferError::NoError && msg.data.size() > 2) {
                    current_handle_ = msg.data[2];
                    state_ = FileClientState::Complete;
                    on_file_opened.emit(current_handle_);
                } else {
                    state_ = FileClientState::Error;
                    on_error.emit(err, "open failed");
                }
                break;
            case FileOperation::List:
                if (err == FileTransferError::NoError && msg.data.size() > 2) {
                    on_file_count.emit(msg.data[2]);
                }
                break;
            case FileOperation::Delete:
                if (err == FileTransferError::NoError) {
                    on_file_deleted.emit();
                }
                break;
            default:
                break;
            }
        }
    };
} // namespace agrobus::isobus
