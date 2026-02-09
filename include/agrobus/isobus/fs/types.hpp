#pragma once

#include "error_codes.hpp"
#include <agrobus/net/types.hpp>
#include <datapod/datapod.hpp>

namespace agrobus::isobus::fs {
    using namespace agrobus::net;

    // ═════════════════════════════════════════════════════════════════════════════
    // ISO 11783-13 File Server Types
    // ═════════════════════════════════════════════════════════════════════════════

    // ─── Transaction Number (TAN) ────────────────────────────────────────────────
    // Used for request/response matching and idempotency
    // TAN wraps at 255 back to 0
    using TAN = u8;

    inline constexpr TAN INVALID_TAN = 0xFF;

    // ─── File Handle ─────────────────────────────────────────────────────────────
    // Server-assigned handle for open files
    // 0 and 0xFF are reserved
    using FileHandle = u8;

    inline constexpr FileHandle INVALID_FILE_HANDLE = 0xFF;
    inline constexpr FileHandle RESERVED_FILE_HANDLE_0 = 0x00;

    // ─── Function Codes (ISO 11783-13 Section 7) ────────────────────────────────
    enum class FSFunction : u8 {
        // Directory operations
        GetCurrentDirectory = 0x00,
        ChangeDirectory = 0x01,
        OpenFile = 0x02,
        SeekFile = 0x03,
        ReadFile = 0x04,
        WriteFile = 0x05,
        CloseFile = 0x06,
        MoveFile = 0x10,
        DeleteFile = 0x11,
        GetFileAttributes = 0x12,
        SetFileAttributes = 0x13,
        GetFileDateTime = 0x14,
        InitializeVolume = 0x20,

        // Status and properties
        FileServerStatus = 0x30,
        GetFileServerProperties = 0x31,

        // Volume operations
        VolumeStatus = 0x40
    };

    // ─── Volume State (ISO 11783-13 Section 7.7) ────────────────────────────────
    enum class VolumeState : u8 {
        Present = 0,             // Media present, idle
        InUse = 1,               // Files open or maintain sent
        PreparingForRemoval = 2, // Shutdown sequence in progress
        Removed = 3              // No access allowed
    };

    // ─── File Server Properties (ISO 11783-13 Section 7.5.2) ────────────────────
    struct FileServerProperties {
        u8 version_number = 1;          // File server version (1 or 2)
        u8 max_simultaneous_files = 16; // Maximum files open at once

        // Capability bits
        bool supports_directories = true;
        bool supports_volume_management = true;
        bool supports_file_attributes = true;
        bool supports_move_file = true;
        bool supports_delete_file = true;

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);

            data[0] = version_number;
            data[1] = max_simultaneous_files;

            // Byte 2: capability flags
            data[2] = 0;
            if (supports_directories)
                data[2] |= (1 << 0);
            if (supports_volume_management)
                data[2] |= (1 << 1);
            if (supports_file_attributes)
                data[2] |= (1 << 2);
            if (supports_move_file)
                data[2] |= (1 << 3);
            if (supports_delete_file)
                data[2] |= (1 << 4);

            // Bytes 3-7: reserved
            return data;
        }

        static FileServerProperties decode(const dp::Vector<u8> &data) {
            FileServerProperties props;
            if (data.size() < 3)
                return props;

            props.version_number = data[0];
            props.max_simultaneous_files = data[1];

            u8 caps = data[2];
            props.supports_directories = (caps & (1 << 0)) != 0;
            props.supports_volume_management = (caps & (1 << 1)) != 0;
            props.supports_file_attributes = (caps & (1 << 2)) != 0;
            props.supports_move_file = (caps & (1 << 3)) != 0;
            props.supports_delete_file = (caps & (1 << 4)) != 0;

            return props;
        }
    };

    // ─── File Entry (directory listing) ─────────────────────────────────────────
    struct FileEntry {
        dp::String name;
        u32 size = 0;
        FileAttributes attributes = FileAttributes::None;
        u16 date = 0; // DOS date format
        u16 time = 0; // DOS time format

        bool is_directory() const { return has_attribute(attributes, FileAttributes::Directory); }

        bool is_read_only() const { return has_attribute(attributes, FileAttributes::ReadOnly); }
    };

    // ─── TAN Cache Entry (for idempotency) ──────────────────────────────────────
    struct TANResponse {
        TAN tan = INVALID_TAN;
        dp::Vector<u8> response_data;
        u32 timestamp_ms = 0;

        bool is_expired(u32 current_time_ms, u32 timeout_ms) const {
            return (current_time_ms - timestamp_ms) > timeout_ms;
        }
    };

    // ─── File Server Status (ISO 11783-13 Section 7.3.1) ────────────────────────
    struct FileServerStatus {
        bool busy = false;
        u8 number_of_open_files = 0;

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = busy ? 0x01 : 0x00;
            data[1] = number_of_open_files;
            return data;
        }

        static FileServerStatus decode(const dp::Vector<u8> &data) {
            FileServerStatus status;
            if (data.size() >= 2) {
                status.busy = (data[0] & 0x01) != 0;
                status.number_of_open_files = data[1];
            }
            return status;
        }
    };

    // ─── Client Connection Maintenance (CCM) Message ─────────────────────────────
    // Sent by client every 2 seconds to maintain connection
    struct CCMMessage {
        u8 version = 1;
        TAN tan = 0;

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = version;
            data[1] = tan;
            return data;
        }

        static CCMMessage decode(const dp::Vector<u8> &data) {
            CCMMessage msg;
            if (data.size() >= 2) {
                msg.version = data[0];
                msg.tan = data[1];
            }
            return msg;
        }
    };

    // ─── Path Utilities ──────────────────────────────────────────────────────────

    // Validate path component (no illegal characters)
    inline bool is_valid_path_component(const dp::String &path) {
        if (path.empty())
            return false;

        // Check for illegal characters in ISO 11783-13
        for (char c : path) {
            if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' ||
                c == '|') {
                return false;
            }
        }

        return true;
    }

    // Check if path is absolute (starts with \\)
    inline bool is_absolute_path(const dp::String &path) {
        return path.size() >= 2 && path[0] == '\\' && path[1] == '\\';
    }

    // Check if path contains wildcard characters
    inline bool has_wildcards(const dp::String &path) {
        return path.find('*') != dp::String::npos || path.find('?') != dp::String::npos;
    }

    // ─── DOS Date/Time Conversion ────────────────────────────────────────────────

    // Pack date into DOS format: (year-1980)<<9 | month<<5 | day
    inline u16 pack_dos_date(u16 year, u8 month, u8 day) { return ((year - 1980) << 9) | (month << 5) | day; }

    // Pack time into DOS format: hour<<11 | minute<<5 | (second/2)
    inline u16 pack_dos_time(u8 hour, u8 minute, u8 second) { return (hour << 11) | (minute << 5) | (second / 2); }

    // Unpack DOS date
    inline void unpack_dos_date(u16 dos_date, u16 &year, u8 &month, u8 &day) {
        year = ((dos_date >> 9) & 0x7F) + 1980;
        month = (dos_date >> 5) & 0x0F;
        day = dos_date & 0x1F;
    }

    // Unpack DOS time
    inline void unpack_dos_time(u16 dos_time, u8 &hour, u8 &minute, u8 &second) {
        hour = (dos_time >> 11) & 0x1F;
        minute = (dos_time >> 5) & 0x3F;
        second = (dos_time & 0x1F) * 2;
    }

} // namespace agrobus::isobus::fs
