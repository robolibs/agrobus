#pragma once

#include <agrobus/net/types.hpp>
#include <datapod/datapod.hpp>

namespace agrobus::isobus::fs {
    using namespace agrobus::net;

    // ─── Enhanced FS command codes (ISO 11783-13 extensions) ────────────────────
    inline constexpr u8 FS_CMD_GET_PROPERTIES = 0x70;
    inline constexpr u8 FS_CMD_GET_VOLUME_STATUS = 0x71;
    inline constexpr u8 FS_CMD_PREPARE_VOLUME_REMOVAL = 0x72;
    inline constexpr u8 FS_CMD_MAINTAIN_VOLUME = 0x73;

    // ─── File Server Properties ─────────────────────────────────────────────────
    struct FileServerProperties {
        u8 version_number = 2; // FS protocol version
        u8 max_open_files = 16;
        bool supports_volumes = true;
        bool supports_long_filenames = true;
        u8 max_simultaneous_clients = 4;

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);

            // Byte 0: Command response identifier
            data[0] = FS_CMD_GET_PROPERTIES;

            // Byte 1: Version number
            data[1] = version_number;

            // Byte 2: Max open files
            data[2] = max_open_files;

            // Byte 3: Capabilities bitfield
            u8 capabilities = 0;
            if (supports_volumes)
                capabilities |= 0x01;
            if (supports_long_filenames)
                capabilities |= 0x02;
            data[3] = capabilities;

            // Byte 4: Max simultaneous clients
            data[4] = max_simultaneous_clients;

            // Bytes 5-7: Reserved (0xFF)
            return data;
        }

        static FileServerProperties decode(const dp::Vector<u8> &data) {
            FileServerProperties props;

            if (data.size() < 5)
                return props;

            // Skip byte 0 (command code)
            usize offset = 0;
            if (data.size() >= 8 && data[0] == FS_CMD_GET_PROPERTIES) {
                offset = 1; // Skip command byte if present
            }

            props.version_number = data[offset + 0];
            props.max_open_files = data[offset + 1];

            u8 capabilities = data[offset + 2];
            props.supports_volumes = (capabilities & 0x01) != 0;
            props.supports_long_filenames = (capabilities & 0x02) != 0;

            props.max_simultaneous_clients = data[offset + 3];

            return props;
        }
    };

    // ─── Volume state ──────────────────────────────────────────────────────────
    enum class VolumeState : u8 { Mounted = 0, NotMounted = 1, PrepareForRemoval = 2, Maintenance = 3 };

    // ─── Volume status ─────────────────────────────────────────────────────────
    struct VolumeStatus {
        dp::String name;
        VolumeState state = VolumeState::Mounted;
        u32 total_bytes = 0;
        u32 free_bytes = 0;
        bool removable = false;

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data;

            // Byte 0: Command response identifier
            data.push_back(FS_CMD_GET_VOLUME_STATUS);

            // Byte 1: Volume state
            data.push_back(static_cast<u8>(state));

            // Byte 2: Flags (bit 0 = removable)
            u8 flags = 0;
            if (removable)
                flags |= 0x01;
            data.push_back(flags);

            // Bytes 3-6: Total bytes (little-endian)
            data.push_back(static_cast<u8>(total_bytes & 0xFF));
            data.push_back(static_cast<u8>((total_bytes >> 8) & 0xFF));
            data.push_back(static_cast<u8>((total_bytes >> 16) & 0xFF));
            data.push_back(static_cast<u8>((total_bytes >> 24) & 0xFF));

            // Bytes 7-10: Free bytes (little-endian)
            data.push_back(static_cast<u8>(free_bytes & 0xFF));
            data.push_back(static_cast<u8>((free_bytes >> 8) & 0xFF));
            data.push_back(static_cast<u8>((free_bytes >> 16) & 0xFF));
            data.push_back(static_cast<u8>((free_bytes >> 24) & 0xFF));

            // Byte 11: Name length
            u8 name_len = static_cast<u8>(name.size() > 255 ? 255 : name.size());
            data.push_back(name_len);

            // Bytes 12+: Name characters
            for (u8 i = 0; i < name_len; ++i) {
                data.push_back(static_cast<u8>(name[i]));
            }

            return data;
        }

        static VolumeStatus decode(const dp::Vector<u8> &data) {
            VolumeStatus vol;

            if (data.size() < 12)
                return vol;

            usize offset = 0;
            // Skip command byte if present
            if (data[0] == FS_CMD_GET_VOLUME_STATUS) {
                offset = 1;
            }

            if (offset + 11 > data.size())
                return vol;

            // State
            vol.state = static_cast<VolumeState>(data[offset + 0]);

            // Flags
            u8 flags = data[offset + 1];
            vol.removable = (flags & 0x01) != 0;

            // Total bytes
            vol.total_bytes = static_cast<u32>(data[offset + 2]) | (static_cast<u32>(data[offset + 3]) << 8) |
                              (static_cast<u32>(data[offset + 4]) << 16) | (static_cast<u32>(data[offset + 5]) << 24);

            // Free bytes
            vol.free_bytes = static_cast<u32>(data[offset + 6]) | (static_cast<u32>(data[offset + 7]) << 8) |
                             (static_cast<u32>(data[offset + 8]) << 16) | (static_cast<u32>(data[offset + 9]) << 24);

            // Name length
            u8 name_len = data[offset + 10];

            // Name
            vol.name.clear();
            for (u8 i = 0; i < name_len && (offset + 11 + i) < data.size(); ++i) {
                vol.name += static_cast<char>(data[offset + 11 + i]);
            }

            return vol;
        }
    };

    // ─── NACK helper for undefined commands ─────────────────────────────────────
    struct FSNack {
        u8 command_code;
        u8 error_code; // 0x01 = not supported

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = command_code;
            data[1] = error_code;
            return data;
        }
    };

    // ─── NACK error codes ──────────────────────────────────────────────────────
    inline constexpr u8 FS_NACK_NOT_SUPPORTED = 0x01;
    inline constexpr u8 FS_NACK_INVALID_ACCESS = 0x02;
    inline constexpr u8 FS_NACK_VOLUME_BUSY = 0x03;

} // namespace agrobus::isobus::fs
