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
        GraphicContext = 40
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

} // namespace agrobus::isobus::vt
