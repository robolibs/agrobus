#pragma once

#include <agrobus/net/types.hpp>
#include <cstring>
#include <datapod/datapod.hpp>
#include <initializer_list>

namespace agrobus::isobus::tc {
    using namespace agrobus::net;

    // ─── Data Dictionary Identifier type ─────────────────────────────────────────
    using DDI = u16;
    using ElementNumber = u16;
    using ObjectID = u16;

    // ─── TC Object types ─────────────────────────────────────────────────────────
    enum class TCObjectType : u8 {
        Device = 0,
        DeviceElement = 1,
        DeviceProcessData = 2,
        DeviceProperty = 3,
        DeviceValuePresentation = 4
    };

    // ─── Device element types ────────────────────────────────────────────────────
    enum class DeviceElementType : u8 {
        Device = 1,
        Function = 2,
        Bin = 3,
        Section = 4,
        Unit = 5,
        Connector = 6,
        NavigationReference = 7
    };

    // ─── Process data trigger methods ────────────────────────────────────────────
    enum class TriggerMethod : u8 {
        TimeInterval = 0x01,
        DistanceInterval = 0x02,
        ThresholdLimits = 0x04,
        OnChange = 0x08,
        Total = 0x10
    };

    // ─── Device object ───────────────────────────────────────────────────────────
    struct DeviceObject {
        ObjectID id = 0;
        dp::String designator;
        dp::String software_version;
        dp::String serial_number;
        dp::Array<u8, 7> structure_label = {};
        dp::Array<u8, 7> localization_label = {};

        // Fluent setters
        DeviceObject &set_id(ObjectID v) {
            id = v;
            return *this;
        }
        DeviceObject &set_designator(dp::String v) {
            designator = std::move(v);
            return *this;
        }
        DeviceObject &set_software_version(dp::String v) {
            software_version = std::move(v);
            return *this;
        }
        DeviceObject &set_serial_number(dp::String v) {
            serial_number = std::move(v);
            return *this;
        }
        DeviceObject &set_structure_label(dp::Array<u8, 7> v) {
            structure_label = v;
            return *this;
        }
        DeviceObject &set_localization_label(dp::Array<u8, 7> v) {
            localization_label = v;
            return *this;
        }

        dp::Vector<u8> serialize() const {
            dp::Vector<u8> data;
            data.push_back(static_cast<u8>(TCObjectType::Device));
            // Object ID
            data.push_back(static_cast<u8>(id & 0xFF));
            data.push_back(static_cast<u8>((id >> 8) & 0xFF));
            // Designator length + string
            data.push_back(static_cast<u8>(designator.size()));
            for (char c : designator)
                data.push_back(static_cast<u8>(c));
            // Software version
            data.push_back(static_cast<u8>(software_version.size()));
            for (char c : software_version)
                data.push_back(static_cast<u8>(c));
            // Serial number
            data.push_back(static_cast<u8>(serial_number.size()));
            for (char c : serial_number)
                data.push_back(static_cast<u8>(c));
            // Structure label
            for (auto b : structure_label)
                data.push_back(b);
            // Localization label
            for (auto b : localization_label)
                data.push_back(b);
            return data;
        }
    };

    // ─── Device element ──────────────────────────────────────────────────────────
    struct DeviceElement {
        ObjectID id = 0;
        DeviceElementType type = DeviceElementType::Device;
        ElementNumber number = 0;
        ObjectID parent_id = 0;
        dp::String designator;
        dp::Vector<ObjectID> child_objects;

        // Fluent setters
        DeviceElement &set_id(ObjectID v) {
            id = v;
            return *this;
        }
        DeviceElement &set_type(DeviceElementType v) {
            type = v;
            return *this;
        }
        DeviceElement &set_number(ElementNumber v) {
            number = v;
            return *this;
        }
        DeviceElement &set_parent(ObjectID v) {
            parent_id = v;
            return *this;
        }
        DeviceElement &set_designator(dp::String v) {
            designator = std::move(v);
            return *this;
        }
        DeviceElement &set_children(dp::Vector<ObjectID> v) {
            child_objects = std::move(v);
            return *this;
        }
        DeviceElement &add_child(ObjectID v) {
            child_objects.push_back(v);
            return *this;
        }

        dp::Vector<u8> serialize() const {
            dp::Vector<u8> data;
            data.push_back(static_cast<u8>(TCObjectType::DeviceElement));
            data.push_back(static_cast<u8>(id & 0xFF));
            data.push_back(static_cast<u8>((id >> 8) & 0xFF));
            data.push_back(static_cast<u8>(type));
            data.push_back(static_cast<u8>(designator.size()));
            for (char c : designator)
                data.push_back(static_cast<u8>(c));
            data.push_back(static_cast<u8>(number & 0xFF));
            data.push_back(static_cast<u8>((number >> 8) & 0xFF));
            data.push_back(static_cast<u8>(parent_id & 0xFF));
            data.push_back(static_cast<u8>((parent_id >> 8) & 0xFF));
            // Number of child object references
            u16 num_children = static_cast<u16>(child_objects.size());
            data.push_back(static_cast<u8>(num_children & 0xFF));
            data.push_back(static_cast<u8>((num_children >> 8) & 0xFF));
            for (auto obj_id : child_objects) {
                data.push_back(static_cast<u8>(obj_id & 0xFF));
                data.push_back(static_cast<u8>((obj_id >> 8) & 0xFF));
            }
            return data;
        }
    };

    // ─── Device process data ─────────────────────────────────────────────────────
    struct DeviceProcessData {
        ObjectID id = 0;
        DDI ddi = 0;
        u8 trigger_methods = 0;
        ObjectID presentation_object_id = 0xFFFF; // Reference to DeviceValuePresentation (0xFFFF = none)
        dp::String designator;

        // Fluent setters
        DeviceProcessData &set_id(ObjectID v) {
            id = v;
            return *this;
        }
        DeviceProcessData &set_ddi(DDI v) {
            ddi = v;
            return *this;
        }
        DeviceProcessData &set_triggers(u8 v) {
            trigger_methods = v;
            return *this;
        }
        DeviceProcessData &add_trigger(TriggerMethod t) {
            trigger_methods |= static_cast<u8>(t);
            return *this;
        }
        DeviceProcessData &set_presentation(ObjectID v) {
            presentation_object_id = v;
            return *this;
        }
        DeviceProcessData &set_designator(dp::String v) {
            designator = std::move(v);
            return *this;
        }

        dp::Vector<u8> serialize() const {
            dp::Vector<u8> data;
            data.push_back(static_cast<u8>(TCObjectType::DeviceProcessData));
            data.push_back(static_cast<u8>(id & 0xFF));
            data.push_back(static_cast<u8>((id >> 8) & 0xFF));
            data.push_back(static_cast<u8>(ddi & 0xFF));
            data.push_back(static_cast<u8>((ddi >> 8) & 0xFF));
            data.push_back(trigger_methods);
            data.push_back(static_cast<u8>(presentation_object_id & 0xFF));
            data.push_back(static_cast<u8>((presentation_object_id >> 8) & 0xFF));
            data.push_back(static_cast<u8>(designator.size()));
            for (char c : designator)
                data.push_back(static_cast<u8>(c));
            return data;
        }
    };

    // ─── Device property ─────────────────────────────────────────────────────────
    struct DeviceProperty {
        ObjectID id = 0;
        DDI ddi = 0;
        i32 value = 0;                            // Fixed property value (part of definition, unlike DeviceProcessData)
        ObjectID presentation_object_id = 0xFFFF; // Reference to DeviceValuePresentation (0xFFFF = none)
        dp::String designator;

        // Fluent setters
        DeviceProperty &set_id(ObjectID v) {
            id = v;
            return *this;
        }
        DeviceProperty &set_ddi(DDI v) {
            ddi = v;
            return *this;
        }
        DeviceProperty &set_value(i32 v) {
            value = v;
            return *this;
        }
        DeviceProperty &set_presentation(ObjectID v) {
            presentation_object_id = v;
            return *this;
        }
        DeviceProperty &set_designator(dp::String v) {
            designator = std::move(v);
            return *this;
        }

        dp::Vector<u8> serialize() const {
            dp::Vector<u8> data;
            data.push_back(static_cast<u8>(TCObjectType::DeviceProperty));
            data.push_back(static_cast<u8>(id & 0xFF));
            data.push_back(static_cast<u8>((id >> 8) & 0xFF));
            data.push_back(static_cast<u8>(ddi & 0xFF));
            data.push_back(static_cast<u8>((ddi >> 8) & 0xFF));
            data.push_back(static_cast<u8>(value & 0xFF));
            data.push_back(static_cast<u8>((value >> 8) & 0xFF));
            data.push_back(static_cast<u8>((value >> 16) & 0xFF));
            data.push_back(static_cast<u8>((value >> 24) & 0xFF));
            data.push_back(static_cast<u8>(presentation_object_id & 0xFF));
            data.push_back(static_cast<u8>((presentation_object_id >> 8) & 0xFF));
            data.push_back(static_cast<u8>(designator.size()));
            for (char c : designator)
                data.push_back(static_cast<u8>(c));
            return data;
        }
    };

    // ─── Device value presentation ───────────────────────────────────────────────
    struct DeviceValuePresentation {
        ObjectID id = 0;
        i32 offset = 0;
        f32 scale = 1.0f;
        u8 decimal_digits = 0;
        dp::String unit_designator;

        // Fluent setters
        DeviceValuePresentation &set_id(ObjectID v) {
            id = v;
            return *this;
        }
        DeviceValuePresentation &set_offset(i32 v) {
            offset = v;
            return *this;
        }
        DeviceValuePresentation &set_scale(f32 v) {
            scale = v;
            return *this;
        }
        DeviceValuePresentation &set_decimals(u8 v) {
            decimal_digits = v;
            return *this;
        }
        DeviceValuePresentation &set_unit(dp::String v) {
            unit_designator = std::move(v);
            return *this;
        }

        dp::Vector<u8> serialize() const {
            dp::Vector<u8> data;
            data.push_back(static_cast<u8>(TCObjectType::DeviceValuePresentation));
            data.push_back(static_cast<u8>(id & 0xFF));
            data.push_back(static_cast<u8>((id >> 8) & 0xFF));
            data.push_back(static_cast<u8>(offset & 0xFF));
            data.push_back(static_cast<u8>((offset >> 8) & 0xFF));
            data.push_back(static_cast<u8>((offset >> 16) & 0xFF));
            data.push_back(static_cast<u8>((offset >> 24) & 0xFF));
            // Scale as IEEE 754 float
            u32 scale_bits;
            std::memcpy(&scale_bits, &scale, sizeof(u32));
            data.push_back(static_cast<u8>(scale_bits & 0xFF));
            data.push_back(static_cast<u8>((scale_bits >> 8) & 0xFF));
            data.push_back(static_cast<u8>((scale_bits >> 16) & 0xFF));
            data.push_back(static_cast<u8>((scale_bits >> 24) & 0xFF));
            data.push_back(decimal_digits);
            data.push_back(static_cast<u8>(unit_designator.size()));
            for (char c : unit_designator)
                data.push_back(static_cast<u8>(c));
            return data;
        }
    };

} // namespace agrobus::isobus::tc
