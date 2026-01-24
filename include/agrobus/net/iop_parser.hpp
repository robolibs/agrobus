#pragma once

// GCC false positive: datapod SSO union member in ObjectPool move constructor
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

#include <agrobus/isobus/vt/objects.hpp>
#include <agrobus/net/error.hpp>
#include <agrobus/net/types.hpp>
#include <datapod/datapod.hpp>
#include <echo/echo.hpp>

namespace agrobus::net {
    namespace vt = agrobus::isobus::vt;

    // ─── IOP (ISOBUS Object Pool) File Parser ───────────────────────────────────
    // Parses standard ISOBUS Object Pool binary files for loading VT object pools.
    // IOP files contain serialized VT objects per ISO 11783-6 Annex B.
    class IOPParser {
      public:
        // Read IOP file from disk and return raw binary data
        static Result<dp::Vector<u8>> read_iop_file(const dp::String &filepath) {
            FILE *f = fopen(filepath.c_str(), "rb");
            if (!f) {
                return Result<dp::Vector<u8>>::err(
                    Error(ErrorCode::DriverError, "Failed to open IOP file: " + filepath));
            }

            fseek(f, 0, SEEK_END);
            isize size = static_cast<isize>(ftell(f));
            fseek(f, 0, SEEK_SET);

            if (size <= 0) {
                fclose(f);
                return Result<dp::Vector<u8>>::err(Error(ErrorCode::DriverError, "Empty IOP file"));
            }

            dp::Vector<u8> data(static_cast<usize>(size));
            usize read = fread(data.data(), 1, static_cast<usize>(size), f);
            fclose(f);

            if (read != static_cast<usize>(size)) {
                return Result<dp::Vector<u8>>::err(Error(ErrorCode::DriverError, "Incomplete IOP read"));
            }

            echo::category("isobus.util.iop").info("Read IOP file: ", filepath, " (", size, " bytes)");
            return Result<dp::Vector<u8>>::ok(std::move(data));
        }

        // Parse object pool data into structured ObjectPool
        static Result<vt::ObjectPool> parse_iop_data(const dp::Vector<u8> &data) {
            vt::ObjectPool pool;
            usize offset = 0;

            while (offset + 7 <= data.size()) {
                vt::VTObject obj;

                // Object ID (2 bytes, little-endian)
                obj.id = static_cast<u16>(data[offset]) | (static_cast<u16>(data[offset + 1]) << 8);
                offset += 2;

                // Object type (1 byte)
                obj.type = static_cast<vt::ObjectType>(data[offset]);
                offset += 1;

                // Width (2 bytes) + Height (2 bytes) stored as first 4 bytes of body
                obj.body.push_back(data[offset]);
                obj.body.push_back(data[offset + 1]);
                offset += 2;

                obj.body.push_back(data[offset]);
                obj.body.push_back(data[offset + 1]);
                offset += 2;

                // Object-specific data length depends on type
                usize obj_data_len = get_object_data_length(obj.type, data, offset);
                if (offset + obj_data_len > data.size()) {
                    echo::category("isobus.util.iop").warn("Truncated object at offset ", offset, " id=", obj.id);
                    break;
                }

                for (usize i = 0; i < obj_data_len; ++i) {
                    obj.body.push_back(data[offset + i]);
                }
                offset += obj_data_len;

                auto result = pool.add(std::move(obj));
                if (!result.is_ok()) {
                    echo::category("isobus.util.iop").warn("Duplicate object ID: ", obj.id);
                }
            }

            echo::category("isobus.util.iop").info("Parsed ", pool.size(), " objects from IOP data");
            return Result<vt::ObjectPool>::ok(std::move(pool));
        }

        // Generate a version hash string from object pool data
        static Result<dp::String> hash_to_version(const dp::Vector<u8> &data) {
            // Simple FNV-1a hash for version identification
            u32 hash = 2166136261u;
            for (auto b : data) {
                hash ^= b;
                hash *= 16777619u;
            }

            // Convert to 7-character version string (ISO 11783-6 version label)
            dp::String version;
            for (u8 i = 0; i < 7; ++i) {
                u8 ch = static_cast<u8>((hash >> (i * 4)) & 0x0F);
                version += static_cast<char>('A' + ch);
            }
            return Result<dp::String>::ok(version);
        }

        // Validate IOP structure without fully parsing
        static Result<bool> validate(const dp::Vector<u8> &data) {
            if (data.size() < 7) {
                return Result<bool>::ok(false);
            }

            usize offset = 0;
            u32 object_count = 0;

            while (offset + 7 <= data.size()) {
                // Skip ID (2) + type (1) + width (2) + height (2) = 7 bytes header
                auto type = static_cast<vt::ObjectType>(data[offset + 2]);
                offset += 7;

                usize obj_len = get_object_data_length(type, data, offset);
                if (offset + obj_len > data.size()) {
                    echo::category("isobus.util.iop").warn("Validation failed: truncated at object ", object_count);
                    return Result<bool>::ok(false);
                }
                offset += obj_len;
                object_count++;
            }

            echo::category("isobus.util.iop").debug("Validation passed: ", object_count, " objects");
            return Result<bool>::ok(true);
        }

      private:
        // Estimate object-specific data length based on type
        static usize get_object_data_length(vt::ObjectType type, const dp::Vector<u8> &data, usize offset) {
            switch (type) {
            case vt::ObjectType::WorkingSet:
                return 4; // background_color(1) + selectable(1) + active_mask(2)
            case vt::ObjectType::DataMask:
                return 3; // background_color(1) + soft_key_mask(2)
            case vt::ObjectType::AlarmMask:
                return 4; // background_color(1) + soft_key_mask(2) + priority(1)
            case vt::ObjectType::Container:
                return 0; // No extra data in minimal form
            case vt::ObjectType::Button:
                return 6; // key_code(1) + border_color(1) + background_color(1) + options(1) + ...
            case vt::ObjectType::InputNumber:
                return 16; // Multiple fields
            case vt::ObjectType::OutputString:
                if (offset + 2 <= data.size()) {
                    u16 str_len = static_cast<u16>(data[offset]) | (static_cast<u16>(data[offset + 1]) << 8);
                    return 2 + str_len + 4; // len + string + other attrs
                }
                return 6;
            case vt::ObjectType::OutputNumber:
                return 12; // value(4) + options + font + ...
            case vt::ObjectType::NumberVariable:
                return 4; // value (4 bytes)
            case vt::ObjectType::StringVariable:
                if (offset + 2 <= data.size()) {
                    u16 str_len = static_cast<u16>(data[offset]) | (static_cast<u16>(data[offset + 1]) << 8);
                    return 2 + str_len;
                }
                return 2;
            case vt::ObjectType::FontAttributes:
                return 3; // color(1) + size(1) + type(1)
            case vt::ObjectType::LineAttributes:
                return 3; // color(1) + width(1) + pattern(1)
            case vt::ObjectType::FillAttributes:
                return 5; // type(1) + color(1) + pattern(2) + ...
            case vt::ObjectType::Macro:
                if (offset + 1 <= data.size()) {
                    return 1 + data[offset]; // num_commands + commands
                }
                return 1;
            default:
                return 0;
            }
        }
    };

} // namespace agrobus::net
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
