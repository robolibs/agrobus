#pragma once

#include <agrobus/net/error.hpp>
#include <agrobus/net/types.hpp>
#include <datapod/datapod.hpp>
#include <initializer_list>
#include <string>

namespace agrobus::isobus::vt {
    using namespace agrobus::net;

    using ObjectID = u16;

    // ─── VT Object types ─────────────────────────────────────────────────────────
    enum class ObjectType : u8 {
        WorkingSet = 0,
        DataMask = 1,
        AlarmMask = 2,
        Container = 3,
        SoftKeyMask = 4,
        Key = 5,
        Button = 6,
        InputBoolean = 7,
        InputString = 8,
        InputNumber = 9,
        InputList = 10,
        OutputString = 11,
        OutputNumber = 12,
        Line = 13,
        Rectangle = 14,
        Ellipse = 15,
        Polygon = 16,
        Meter = 17,
        LinearBarGraph = 18,
        ArchedBarGraph = 19,
        PictureGraphic = 20,
        NumberVariable = 21,
        StringVariable = 22,
        FontAttributes = 23,
        LineAttributes = 24,
        FillAttributes = 25,
        InputAttributes = 26,
        ObjectPointer = 27,
        Macro = 28,
        AuxFunction = 29,
        AuxInput = 30,
        AuxFunction2 = 31,
        AuxInput2 = 32,
        AuxControlDesig = 33,
        WindowMask = 34,
        KeyGroup = 35,
        GraphicData = 36,
        ScaledGraphic = 37,
        Animation = 38,
        ColourMap = 39,
        GraphicContext = 40,
        // VT Version 6 additions (ISO 11783-6 Ed.4, 2018)
        ExternalObjectDefinition = 41,
        ExternalReferenceName = 42,
        ExternalObjectPointer = 43,
        ColourPalette = 44,
        GraphicsContext = 45,
        ObjectLabelRef = 46,
        ScaledBitmap = 47
    };

    // ─── Type-Specific Object Bodies ────────────────────────────────────────────
    // ISO 11783-6 §4.6.21: Window Mask Object (Type 34)
    struct WindowMaskBody {
        u8 window_type = 0; // 0=freeform, 1=numeric output, 2=list
        u8 background_color = 0;
        u8 options = 0; // bit 0=available for adjustment, bit 1=removable
        ObjectID name = 0xFFFF;
        ObjectID window_title = 0xFFFF;
        ObjectID window_icon = 0xFFFF;

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data;
            data.push_back(window_type);
            data.push_back(background_color);
            data.push_back(options);
            data.push_back(static_cast<u8>(name & 0xFF));
            data.push_back(static_cast<u8>((name >> 8) & 0xFF));
            data.push_back(static_cast<u8>(window_title & 0xFF));
            data.push_back(static_cast<u8>((window_title >> 8) & 0xFF));
            data.push_back(static_cast<u8>(window_icon & 0xFF));
            data.push_back(static_cast<u8>((window_icon >> 8) & 0xFF));
            return data;
        }

        static Result<WindowMaskBody> decode(const dp::Vector<u8> &body) {
            if (body.size() < 9)
                return Result<WindowMaskBody>::err(Error::invalid_data("Window Mask body too short"));

            WindowMaskBody wm;
            wm.window_type = body[0];
            wm.background_color = body[1];
            wm.options = body[2];
            wm.name = static_cast<u16>(body[3]) | (static_cast<u16>(body[4]) << 8);
            wm.window_title = static_cast<u16>(body[5]) | (static_cast<u16>(body[6]) << 8);
            wm.window_icon = static_cast<u16>(body[7]) | (static_cast<u16>(body[8]) << 8);
            return Result<WindowMaskBody>::ok(std::move(wm));
        }
    };

    // ISO 11783-6 §4.6.22: Key Group Object (Type 35)
    struct KeyGroupBody {
        u8 options = 0; // bit 0=available, bit 1=transparent
        ObjectID name = 0xFFFF;
        ObjectID key_group_icon = 0xFFFF;

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data;
            data.push_back(options);
            data.push_back(static_cast<u8>(name & 0xFF));
            data.push_back(static_cast<u8>((name >> 8) & 0xFF));
            data.push_back(static_cast<u8>(key_group_icon & 0xFF));
            data.push_back(static_cast<u8>((key_group_icon >> 8) & 0xFF));
            return data;
        }

        static Result<KeyGroupBody> decode(const dp::Vector<u8> &body) {
            if (body.size() < 5)
                return Result<KeyGroupBody>::err(Error::invalid_data("Key Group body too short"));

            KeyGroupBody kg;
            kg.options = body[0];
            kg.name = static_cast<u16>(body[1]) | (static_cast<u16>(body[2]) << 8);
            kg.key_group_icon = static_cast<u16>(body[3]) | (static_cast<u16>(body[4]) << 8);
            return Result<KeyGroupBody>::ok(std::move(kg));
        }
    };

    // ISO 11783-6 §4.6.7: Key Object (Type 5)
    struct KeyBody {
        u8 background_color = 0;
        u8 key_code = 0;
        u8 options = 0; // bit 0=latchable

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data;
            data.push_back(background_color);
            data.push_back(key_code);
            data.push_back(options);
            return data;
        }

        static Result<KeyBody> decode(const dp::Vector<u8> &body) {
            if (body.size() < 3)
                return Result<KeyBody>::err(Error::invalid_data("Key body too short"));

            KeyBody k;
            k.background_color = body[0];
            k.key_code = body[1];
            k.options = body[2];
            return Result<KeyBody>::ok(std::move(k));
        }
    };

    // ISO 11783-6 §4.6.30: Macro Object (Type 28)
    struct MacroCommand {
        u8 command_type = 0;
        dp::Vector<u8> parameters;

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data;
            data.push_back(command_type);
            for (auto p : parameters)
                data.push_back(p);
            return data;
        }

        // Get the total length of a VT command (including command byte)
        // Returns 0 for unknown/variable-length commands
        static u16 get_command_length(u8 cmd) {
            switch (cmd) {
            case 0xA0:
                return 6; // Hide/Show
            case 0xA1:
                return 5; // Enable/Disable
            case 0xA2:
                return 2; // Select Active Working Set
            case 0xA3:
                return 4; // Control Audio Signal
            case 0xA4:
                return 8; // Change Size
            case 0xA5:
                return 4; // Change Background Colour
            case 0xA6:
                return 7; // Change Child Location
            case 0xA7:
                return 9; // Change Child Position
            case 0xA8:
                return 8; // Change Numeric Value
            case 0xA9:
                return 3; // Set Audio Volume
            case 0xAD:
                return 4; // Change Active Mask
            case 0xAE:
                return 5; // Change Soft Key Mask
            case 0xAF:
                return 8; // Change Attribute
            case 0xB0:
                return 6; // Change List Item
            case 0xB1:
                return 5; // Change Fill Attributes
            case 0xB2:
                return 5; // Change Font Attributes
            case 0xB3:
                return 0; // Change String Value (variable length)
            case 0xB4:
                return 4; // Change Priority
            case 0xB5:
                return 9; // Change End Point
            case 0xBD:
                return 5; // Lock/Unlock Mask
            case 0xBE:
                return 2; // Execute Macro
            default:
                return 0; // Unknown or variable length
            }
        }
    };

    struct MacroBody {
        dp::Vector<MacroCommand> commands;

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data;
            // Macro commands are serialized sequentially
            for (const auto &cmd : commands) {
                auto cmd_data = cmd.encode();
                data.insert(data.end(), cmd_data.begin(), cmd_data.end());
            }
            return data;
        }

        static Result<MacroBody> decode(const dp::Vector<u8> &body) {
            MacroBody mb;
            usize offset = 0;

            while (offset < body.size()) {
                if (offset + 1 > body.size())
                    break; // Not enough data for command byte

                MacroCommand cmd;
                cmd.command_type = body[offset];
                offset++;

                // Get command length (excluding command byte)
                u16 cmd_len = MacroCommand::get_command_length(cmd.command_type);

                if (cmd_len == 0) {
                    // Variable length or unknown command - try to parse or skip
                    if (cmd.command_type == 0xB3) { // Change String Value
                        // Format: [cmd][obj_id_lo][obj_id_hi][len_lo][len_hi][string...]
                        if (offset + 4 > body.size())
                            return Result<MacroBody>::err(
                                Error::invalid_data("incomplete Change String Value in macro"));
                        u16 str_len = static_cast<u16>(body[offset + 2]) | (static_cast<u16>(body[offset + 3]) << 8);
                        u16 total_len = 4 + str_len;
                        if (offset + total_len > body.size())
                            return Result<MacroBody>::err(Error::invalid_data("string data exceeds macro body"));
                        for (u16 i = 0; i < total_len; ++i)
                            cmd.parameters.push_back(body[offset + i]);
                        offset += total_len;
                    } else {
                        // Unknown command - cannot safely parse further
                        return Result<MacroBody>::err(
                            Error::invalid_data("unknown or variable-length macro command 0x" +
                                                dp::String(std::to_string(cmd.command_type))));
                    }
                } else {
                    // Fixed-length command - copy parameters
                    u16 param_len = cmd_len - 1; // Exclude command byte
                    if (offset + param_len > body.size())
                        return Result<MacroBody>::err(Error::invalid_data("macro command parameters exceed body"));
                    for (u16 i = 0; i < param_len; ++i)
                        cmd.parameters.push_back(body[offset + i]);
                    offset += param_len;
                }

                mb.commands.push_back(std::move(cmd));
            }

            return Result<MacroBody>::ok(std::move(mb));
        }
    };

    // ISO 11783-6 §4.6.3: Alarm Mask Object (Type 2) - with priority extension
    struct AlarmMaskBody {
        u8 background_color = 0;
        ObjectID soft_key_mask = 0xFFFF;
        u8 priority = 0; // 0=Critical (highest), 1=Warning, 2=Information (lowest)
        u8 acoustic_signal = 0;
        u8 options = 0;

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data;
            data.push_back(background_color);
            data.push_back(static_cast<u8>(soft_key_mask & 0xFF));
            data.push_back(static_cast<u8>((soft_key_mask >> 8) & 0xFF));
            data.push_back(priority);
            data.push_back(acoustic_signal);
            data.push_back(options);
            return data;
        }

        static Result<AlarmMaskBody> decode(const dp::Vector<u8> &body) {
            if (body.size() < 6)
                return Result<AlarmMaskBody>::err(Error::invalid_data("Alarm Mask body too short"));

            AlarmMaskBody am;
            am.background_color = body[0];
            am.soft_key_mask = static_cast<u16>(body[1]) | (static_cast<u16>(body[2]) << 8);
            am.priority = body[3];
            am.acoustic_signal = body[4];
            am.options = body[5];
            return Result<AlarmMaskBody>::ok(std::move(am));
        }
    };

    // ─── VT Object ───────────────────────────────────────────────────────────────
    // Serialized format (length-driven):
    //   [0..1] Object ID (LE)
    //   [2]    Object type
    //   [3..4] Body length (LE) - number of bytes following this field
    //   [5..]  Object-specific body (includes children list)
    struct VTObject {
        ObjectID id = 0;
        ObjectType type = ObjectType::WorkingSet;
        dp::Vector<u8> body; // Object-specific data (type-dependent layout)
        dp::Vector<ObjectID> children;

        // Fluent setters
        VTObject &set_id(ObjectID v) {
            id = v;
            return *this;
        }
        VTObject &set_type(ObjectType v) {
            type = v;
            return *this;
        }
        VTObject &set_body(dp::Vector<u8> v) {
            body = std::move(v);
            return *this;
        }
        VTObject &set_body(std::initializer_list<u8> bytes) {
            body = dp::Vector<u8>(bytes);
            return *this;
        }
        VTObject &set_children(dp::Vector<ObjectID> v) {
            children = std::move(v);
            return *this;
        }
        VTObject &set_children(std::initializer_list<ObjectID> ids) {
            children = dp::Vector<ObjectID>(ids);
            return *this;
        }
        VTObject &add_child(ObjectID v) {
            children.push_back(v);
            return *this;
        }

        // ─── Type-Specific Body Helpers ───────────────────────────────────────────
        VTObject &set_window_mask_body(const WindowMaskBody &wm) {
            body = wm.encode();
            return *this;
        }

        VTObject &set_key_group_body(const KeyGroupBody &kg) {
            body = kg.encode();
            return *this;
        }

        VTObject &set_key_body(const KeyBody &k) {
            body = k.encode();
            return *this;
        }

        VTObject &set_macro_body(const MacroBody &m) {
            body = m.encode();
            return *this;
        }

        VTObject &set_alarm_mask_body(const AlarmMaskBody &am) {
            body = am.encode();
            return *this;
        }

        Result<WindowMaskBody> get_window_mask_body() const {
            if (type != ObjectType::WindowMask)
                return Result<WindowMaskBody>::err(Error::invalid_data("object is not a Window Mask"));
            return WindowMaskBody::decode(body);
        }

        Result<KeyGroupBody> get_key_group_body() const {
            if (type != ObjectType::KeyGroup)
                return Result<KeyGroupBody>::err(Error::invalid_data("object is not a Key Group"));
            return KeyGroupBody::decode(body);
        }

        Result<KeyBody> get_key_body() const {
            if (type != ObjectType::Key)
                return Result<KeyBody>::err(Error::invalid_data("object is not a Key"));
            return KeyBody::decode(body);
        }

        Result<MacroBody> get_macro_body() const {
            if (type != ObjectType::Macro)
                return Result<MacroBody>::err(Error::invalid_data("object is not a Macro"));
            return MacroBody::decode(body);
        }

        Result<AlarmMaskBody> get_alarm_mask_body() const {
            if (type != ObjectType::AlarmMask)
                return Result<AlarmMaskBody>::err(Error::invalid_data("object is not an Alarm Mask"));
            return AlarmMaskBody::decode(body);
        }

        dp::Vector<u8> serialize() const {
            dp::Vector<u8> data;
            // Object ID
            data.push_back(static_cast<u8>(id & 0xFF));
            data.push_back(static_cast<u8>((id >> 8) & 0xFF));
            // Object type
            data.push_back(static_cast<u8>(type));
            // Calculate body length: body data + children list (2 bytes per child + 2 byte count)
            u16 children_size = children.empty() ? 0 : static_cast<u16>(2 + children.size() * 2);
            u16 body_len = static_cast<u16>(body.size() + children_size);
            data.push_back(static_cast<u8>(body_len & 0xFF));
            data.push_back(static_cast<u8>((body_len >> 8) & 0xFF));
            // Body data
            for (auto b : body)
                data.push_back(b);
            // Children list
            if (!children.empty()) {
                u16 num = static_cast<u16>(children.size());
                data.push_back(static_cast<u8>(num & 0xFF));
                data.push_back(static_cast<u8>((num >> 8) & 0xFF));
                for (auto child_id : children) {
                    data.push_back(static_cast<u8>(child_id & 0xFF));
                    data.push_back(static_cast<u8>((child_id >> 8) & 0xFF));
                }
            }
            return data;
        }
    };

    // ─── Object pool ─────────────────────────────────────────────────────────────
    class ObjectPool {
        dp::Vector<VTObject> objects_;
        dp::String version_label_{}; // Explicit pool identifier

      public:
        void set_version_label(dp::String label) { version_label_ = std::move(label); }
        const dp::String &version_label() const noexcept { return version_label_; }

        Result<void> add(VTObject obj) {
            for (const auto &existing : objects_) {
                if (existing.id == obj.id) {
                    return Result<void>::err(Error::invalid_state("duplicate object ID"));
                }
            }
            objects_.push_back(std::move(obj));
            return {};
        }

        dp::Optional<VTObject *> find(ObjectID id) {
            for (auto &obj : objects_) {
                if (obj.id == id)
                    return &obj;
            }
            return dp::nullopt;
        }

        dp::Optional<const VTObject *> find(ObjectID id) const {
            for (const auto &obj : objects_) {
                if (obj.id == id)
                    return &obj;
            }
            return dp::nullopt;
        }

        Result<dp::Vector<u8>> serialize() const {
            dp::Vector<u8> data;
            for (const auto &obj : objects_) {
                auto bytes = obj.serialize();
                data.insert(data.end(), bytes.begin(), bytes.end());
            }
            return Result<dp::Vector<u8>>::ok(std::move(data));
        }

        // Deserialize a pool from binary data (length-driven parsing)
        static Result<ObjectPool> deserialize(const dp::Vector<u8> &data) {
            ObjectPool pool;
            usize offset = 0;

            while (offset + 5 <= data.size()) {
                VTObject obj;
                // Object ID
                obj.id = static_cast<u16>(data[offset]) | (static_cast<u16>(data[offset + 1]) << 8);
                // Object type
                obj.type = static_cast<ObjectType>(data[offset + 2]);
                // Body length
                u16 body_len = static_cast<u16>(data[offset + 3]) | (static_cast<u16>(data[offset + 4]) << 8);
                offset += 5;

                if (offset + body_len > data.size()) {
                    return Result<ObjectPool>::err(
                        Error(ErrorCode::PoolValidation, "object body extends past pool data"));
                }

                // Copy body bytes
                for (u16 i = 0; i < body_len; ++i) {
                    obj.body.push_back(data[offset + i]);
                }
                offset += body_len;

                auto r = pool.add(std::move(obj));
                if (!r.is_ok())
                    return Result<ObjectPool>::err(r.error());
            }

            return Result<ObjectPool>::ok(std::move(pool));
        }

        // ─── Pool validation (ISO 11783-6 §4.6.8) ────────────────────────────────
        Result<void> validate() const {
            // Count WorkingSet objects
            u16 ws_count = 0;
            for (const auto &obj : objects_) {
                if (obj.type == ObjectType::WorkingSet)
                    ws_count++;
            }
            if (ws_count == 0)
                return Result<void>::err(Error::invalid_state("pool must contain exactly one Working Set object"));
            if (ws_count > 1)
                return Result<void>::err(Error::invalid_state(
                    "pool must contain exactly one Working Set object, found " + dp::String(std::to_string(ws_count))));

            // Verify Working Set has at least one active mask (DataMask or AlarmMask child)
            for (const auto &obj : objects_) {
                if (obj.type == ObjectType::WorkingSet) {
                    bool has_mask = false;
                    for (auto child_id : obj.children) {
                        auto child = find(child_id);
                        if (child.has_value()) {
                            auto child_type = (*child)->type;
                            if (child_type == ObjectType::DataMask || child_type == ObjectType::AlarmMask) {
                                has_mask = true;
                                break;
                            }
                        }
                    }
                    if (!has_mask)
                        return Result<void>::err(
                            Error::invalid_state("Working Set must reference at least one Data Mask or Alarm Mask"));
                    break;
                }
            }

            // Verify no orphan object references (children point to existing objects)
            for (const auto &obj : objects_) {
                for (auto child_id : obj.children) {
                    if (!find(child_id).has_value()) {
                        return Result<void>::err(Error::invalid_state("object " + dp::String(std::to_string(obj.id)) +
                                                                      " references non-existent child " +
                                                                      dp::String(std::to_string(child_id))));
                    }
                }
            }

            return {};
        }

        usize size() const noexcept { return objects_.size(); }
        bool empty() const noexcept { return objects_.empty(); }
        void clear() { objects_.clear(); }

        const dp::Vector<VTObject> &objects() const noexcept { return objects_; }

        // ─── Fluent API ────────────────────────────────────────────────────────────
        ObjectPool &with_object(VTObject obj) {
            add(std::move(obj));
            return *this;
        }
        ObjectPool &with_version_label(dp::String label) {
            set_version_label(std::move(label));
            return *this;
        }
    };

    // ─── Object Builder Helpers ─────────────────────────────────────────────────
    inline VTObject create_window_mask(ObjectID id, const WindowMaskBody &body) {
        VTObject obj;
        obj.set_id(id).set_type(ObjectType::WindowMask).set_window_mask_body(body);
        return obj;
    }

    inline VTObject create_key_group(ObjectID id, const KeyGroupBody &body) {
        VTObject obj;
        obj.set_id(id).set_type(ObjectType::KeyGroup).set_key_group_body(body);
        return obj;
    }

    inline VTObject create_key(ObjectID id, const KeyBody &body) {
        VTObject obj;
        obj.set_id(id).set_type(ObjectType::Key).set_key_body(body);
        return obj;
    }

    inline VTObject create_macro(ObjectID id, const MacroBody &body) {
        VTObject obj;
        obj.set_id(id).set_type(ObjectType::Macro).set_macro_body(body);
        return obj;
    }

    inline VTObject create_alarm_mask(ObjectID id, const AlarmMaskBody &body) {
        VTObject obj;
        obj.set_id(id).set_type(ObjectType::AlarmMask).set_alarm_mask_body(body);
        return obj;
    }

    // ═════════════════════════════════════════════════════════════════════════════
    // VT Version 6 Advanced Features (ISO 11783-6 Edition 4, 2018)
    // ═════════════════════════════════════════════════════════════════════════════

    // ─── Touch Gesture Support ───────────────────────────────────────────────────
    enum class GestureType : u8 {
        None = 0,
        Tap = 1,
        DoubleTap = 2,
        LongPress = 3,
        SwipeLeft = 4,
        SwipeRight = 5,
        SwipeUp = 6,
        SwipeDown = 7,
        PinchIn = 8,     // Zoom out
        PinchOut = 9,    // Zoom in
        Rotate = 10,
        TwoFingerTap = 11,
        ThreeFingerTap = 12
    };

    struct TouchGesture {
        GestureType type = GestureType::None;
        i16 x = 0;          // Touch X coordinate (pixels)
        i16 y = 0;          // Touch Y coordinate (pixels)
        u16 duration_ms = 0; // Gesture duration (for long press)
        i16 distance = 0;    // Swipe distance (pixels)
        f32 scale = 1.0f;    // Pinch scale factor (1.0 = no change)
        f32 rotation_deg = 0.0f; // Rotation angle (degrees)
        u8 touch_count = 1;  // Number of simultaneous touches
        ObjectID target_object = 0xFFFF; // Object that received gesture

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data;
            data.push_back(static_cast<u8>(type));
            data.push_back(static_cast<u8>(x & 0xFF));
            data.push_back(static_cast<u8>((x >> 8) & 0xFF));
            data.push_back(static_cast<u8>(y & 0xFF));
            data.push_back(static_cast<u8>((y >> 8) & 0xFF));
            data.push_back(static_cast<u8>(duration_ms & 0xFF));
            data.push_back(static_cast<u8>((duration_ms >> 8) & 0xFF));
            data.push_back(static_cast<u8>(distance & 0xFF));
            data.push_back(static_cast<u8>((distance >> 8) & 0xFF));
            data.push_back(touch_count);
            data.push_back(static_cast<u8>(target_object & 0xFF));
            data.push_back(static_cast<u8>((target_object >> 8) & 0xFF));
            return data;
        }

        static TouchGesture decode(const dp::Vector<u8> &data) {
            TouchGesture gesture;
            if (data.size() < 12)
                return gesture;
            gesture.type = static_cast<GestureType>(data[0]);
            gesture.x = static_cast<i16>(static_cast<u16>(data[1]) | (static_cast<u16>(data[2]) << 8));
            gesture.y = static_cast<i16>(static_cast<u16>(data[3]) | (static_cast<u16>(data[4]) << 8));
            gesture.duration_ms = static_cast<u16>(data[5]) | (static_cast<u16>(data[6]) << 8);
            gesture.distance = static_cast<i16>(static_cast<u16>(data[7]) | (static_cast<u16>(data[8]) << 8));
            gesture.touch_count = data[9];
            gesture.target_object = static_cast<u16>(data[10]) | (static_cast<u16>(data[11]) << 8);
            return gesture;
        }
    };

    // ─── Extended Graphics Context (VT6) ─────────────────────────────────────────
    struct GraphicsContextV6 {
        u8 transparency = 0xFF;      // 0-255 (0=fully transparent, 255=opaque)
        u8 line_style = 0;           // 0=solid, 1=dashed, 2=dotted
        u16 line_width = 1;          // Line width in pixels
        u32 fill_color_rgb = 0;      // 24-bit RGB color (VT6)
        u32 line_color_rgb = 0;      // 24-bit RGB color (VT6)
        bool anti_aliasing = false;  // Enable anti-aliasing
        u8 blend_mode = 0;           // 0=normal, 1=multiply, 2=screen, 3=overlay

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data;
            data.push_back(transparency);
            data.push_back(line_style);
            data.push_back(static_cast<u8>(line_width & 0xFF));
            data.push_back(static_cast<u8>((line_width >> 8) & 0xFF));
            // 24-bit RGB fill color
            data.push_back(static_cast<u8>(fill_color_rgb & 0xFF));
            data.push_back(static_cast<u8>((fill_color_rgb >> 8) & 0xFF));
            data.push_back(static_cast<u8>((fill_color_rgb >> 16) & 0xFF));
            // 24-bit RGB line color
            data.push_back(static_cast<u8>(line_color_rgb & 0xFF));
            data.push_back(static_cast<u8>((line_color_rgb >> 8) & 0xFF));
            data.push_back(static_cast<u8>((line_color_rgb >> 16) & 0xFF));
            data.push_back(anti_aliasing ? 0x01 : 0x00);
            data.push_back(blend_mode);
            return data;
        }

        static GraphicsContextV6 decode(const dp::Vector<u8> &data) {
            GraphicsContextV6 ctx;
            if (data.size() < 12)
                return ctx;
            ctx.transparency = data[0];
            ctx.line_style = data[1];
            ctx.line_width = static_cast<u16>(data[2]) | (static_cast<u16>(data[3]) << 8);
            ctx.fill_color_rgb = static_cast<u32>(data[4]) | (static_cast<u32>(data[5]) << 8) | (static_cast<u32>(data[6]) << 16);
            ctx.line_color_rgb = static_cast<u32>(data[7]) | (static_cast<u32>(data[8]) << 8) | (static_cast<u32>(data[9]) << 16);
            ctx.anti_aliasing = (data[10] != 0);
            ctx.blend_mode = data[11];
            return ctx;
        }
    };

    // ─── External Object Definition (VT6) ────────────────────────────────────────
    // Allows referencing objects from external libraries
    struct ExternalObjectDefinitionBody {
        u8 options = 0; // bit 0=enabled
        ObjectID default_object_id = 0xFFFF; // Fallback if external not found
        ObjectID external_reference_name = 0xFFFF; // Reference to name object
        ObjectID external_object_id = 0xFFFF; // ID in external library

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data;
            data.push_back(options);
            data.push_back(static_cast<u8>(default_object_id & 0xFF));
            data.push_back(static_cast<u8>((default_object_id >> 8) & 0xFF));
            data.push_back(static_cast<u8>(external_reference_name & 0xFF));
            data.push_back(static_cast<u8>((external_reference_name >> 8) & 0xFF));
            data.push_back(static_cast<u8>(external_object_id & 0xFF));
            data.push_back(static_cast<u8>((external_object_id >> 8) & 0xFF));
            return data;
        }

        static Result<ExternalObjectDefinitionBody> decode(const dp::Vector<u8> &data) {
            if (data.size() < 7)
                return Result<ExternalObjectDefinitionBody>::err(Error::invalid_data("External Object Definition body too short"));
            ExternalObjectDefinitionBody ext;
            ext.options = data[0];
            ext.default_object_id = static_cast<u16>(data[1]) | (static_cast<u16>(data[2]) << 8);
            ext.external_reference_name = static_cast<u16>(data[3]) | (static_cast<u16>(data[4]) << 8);
            ext.external_object_id = static_cast<u16>(data[5]) | (static_cast<u16>(data[6]) << 8);
            return Result<ExternalObjectDefinitionBody>::ok(std::move(ext));
        }
    };

    // ─── Scaled Bitmap (VT6) ─────────────────────────────────────────────────────
    // Enhanced bitmap with scaling and transformation support
    struct ScaledBitmapBody {
        u16 width = 0;
        u16 height = 0;
        f32 scale_x = 1.0f;
        f32 scale_y = 1.0f;
        i16 offset_x = 0;
        i16 offset_y = 0;
        u8 format = 0; // 0=1bit, 1=4bit, 2=8bit, 3=24bit RGB
        u8 options = 0; // bit 0=RLE compressed
        ObjectID bitmap_data = 0xFFFF;

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data;
            data.push_back(static_cast<u8>(width & 0xFF));
            data.push_back(static_cast<u8>((width >> 8) & 0xFF));
            data.push_back(static_cast<u8>(height & 0xFF));
            data.push_back(static_cast<u8>((height >> 8) & 0xFF));
            // Scale factors as u16 fixed-point (8.8)
            u16 sx = static_cast<u16>(scale_x * 256.0f);
            u16 sy = static_cast<u16>(scale_y * 256.0f);
            data.push_back(static_cast<u8>(sx & 0xFF));
            data.push_back(static_cast<u8>((sx >> 8) & 0xFF));
            data.push_back(static_cast<u8>(sy & 0xFF));
            data.push_back(static_cast<u8>((sy >> 8) & 0xFF));
            data.push_back(static_cast<u8>(offset_x & 0xFF));
            data.push_back(static_cast<u8>((offset_x >> 8) & 0xFF));
            data.push_back(static_cast<u8>(offset_y & 0xFF));
            data.push_back(static_cast<u8>((offset_y >> 8) & 0xFF));
            data.push_back(format);
            data.push_back(options);
            data.push_back(static_cast<u8>(bitmap_data & 0xFF));
            data.push_back(static_cast<u8>((bitmap_data >> 8) & 0xFF));
            return data;
        }

        static Result<ScaledBitmapBody> decode(const dp::Vector<u8> &data) {
            if (data.size() < 16)
                return Result<ScaledBitmapBody>::err(Error::invalid_data("Scaled Bitmap body too short"));
            ScaledBitmapBody bmp;
            bmp.width = static_cast<u16>(data[0]) | (static_cast<u16>(data[1]) << 8);
            bmp.height = static_cast<u16>(data[2]) | (static_cast<u16>(data[3]) << 8);
            u16 sx = static_cast<u16>(data[4]) | (static_cast<u16>(data[5]) << 8);
            u16 sy = static_cast<u16>(data[6]) | (static_cast<u16>(data[7]) << 8);
            bmp.scale_x = static_cast<f32>(sx) / 256.0f;
            bmp.scale_y = static_cast<f32>(sy) / 256.0f;
            bmp.offset_x = static_cast<i16>(static_cast<u16>(data[8]) | (static_cast<u16>(data[9]) << 8));
            bmp.offset_y = static_cast<i16>(static_cast<u16>(data[10]) | (static_cast<u16>(data[11]) << 8));
            bmp.format = data[12];
            bmp.options = data[13];
            bmp.bitmap_data = static_cast<u16>(data[14]) | (static_cast<u16>(data[15]) << 8);
            return Result<ScaledBitmapBody>::ok(std::move(bmp));
        }
    };

    // ─── Colour Palette (VT6 24-bit color support) ───────────────────────────────
    struct ColourPaletteEntry {
        u32 rgb = 0; // 24-bit RGB color
        dp::String name; // Optional color name
    };

    struct ColourPalette {
        dp::Vector<ColourPaletteEntry> entries;

        void add_color(u32 rgb, const dp::String &name = "") {
            ColourPaletteEntry entry;
            entry.rgb = rgb;
            entry.name = name;
            entries.push_back(entry);
        }

        // Standard palette initialization (VT6 extended colors)
        static ColourPalette create_standard_v6() {
            ColourPalette palette;
            // Add standard VT6 extended colors (24-bit RGB)
            palette.add_color(0x000000, "black");
            palette.add_color(0xFFFFFF, "white");
            palette.add_color(0xFF0000, "red");
            palette.add_color(0x00FF00, "green");
            palette.add_color(0x0000FF, "blue");
            palette.add_color(0xFFFF00, "yellow");
            palette.add_color(0xFF00FF, "magenta");
            palette.add_color(0x00FFFF, "cyan");
            palette.add_color(0x808080, "gray");
            palette.add_color(0xFFA500, "orange");
            palette.add_color(0x800080, "purple");
            palette.add_color(0xA52A2A, "brown");
            palette.add_color(0xFFC0CB, "pink");
            palette.add_color(0x00FF7F, "spring green");
            palette.add_color(0x4B0082, "indigo");
            palette.add_color(0xFF6347, "tomato");
            return palette;
        }
    };

} // namespace agrobus::isobus::vt
