#pragma once

#include "objects.hpp"
#include <agrobus/net/error.hpp>
#include <cstring>
#include <datapod/datapod.hpp>
#include <echo/echo.hpp>

namespace agrobus::isobus::tc {
    using namespace agrobus::net;

    // ─── Device Descriptor Object Pool ──────────────────────────────────────────
    class DDOP {
        dp::Vector<DeviceObject> devices_;
        dp::Vector<DeviceElement> elements_;
        dp::Vector<DeviceProcessData> process_data_;
        dp::Vector<DeviceProperty> properties_;
        dp::Vector<DeviceValuePresentation> value_presentations_;
        ObjectID next_id_ = 0;

      public:
        ObjectID next_id() noexcept { return next_id_++; }

        Result<ObjectID> add_device(DeviceObject obj) {
            if (obj.designator.empty()) {
                return Result<ObjectID>::err(Error::invalid_state("device designator is required"));
            }
            if (obj.id == 0)
                obj.id = next_id();
            devices_.push_back(std::move(obj));
            return Result<ObjectID>::ok(devices_.back().id);
        }

        Result<ObjectID> add_element(DeviceElement elem) {
            if (elem.id == 0)
                elem.id = next_id();
            elements_.push_back(std::move(elem));
            return Result<ObjectID>::ok(elements_.back().id);
        }

        Result<ObjectID> add_process_data(DeviceProcessData pd) {
            if (pd.id == 0)
                pd.id = next_id();
            process_data_.push_back(std::move(pd));
            return Result<ObjectID>::ok(process_data_.back().id);
        }

        Result<ObjectID> add_property(DeviceProperty prop) {
            if (prop.id == 0)
                prop.id = next_id();
            properties_.push_back(std::move(prop));
            return Result<ObjectID>::ok(properties_.back().id);
        }

        Result<ObjectID> add_value_presentation(DeviceValuePresentation vp) {
            if (vp.id == 0)
                vp.id = next_id();
            value_presentations_.push_back(std::move(vp));
            return Result<ObjectID>::ok(value_presentations_.back().id);
        }

        // Serialize the entire DDOP to binary
        Result<dp::Vector<u8>> serialize() const {
            dp::Vector<u8> data;

            for (const auto &dev : devices_) {
                auto bytes = dev.serialize();
                data.insert(data.end(), bytes.begin(), bytes.end());
            }
            for (const auto &elem : elements_) {
                auto bytes = elem.serialize();
                data.insert(data.end(), bytes.begin(), bytes.end());
            }
            for (const auto &pd : process_data_) {
                auto bytes = pd.serialize();
                data.insert(data.end(), bytes.begin(), bytes.end());
            }
            for (const auto &prop : properties_) {
                auto bytes = prop.serialize();
                data.insert(data.end(), bytes.begin(), bytes.end());
            }
            for (const auto &vp : value_presentations_) {
                auto bytes = vp.serialize();
                data.insert(data.end(), bytes.begin(), bytes.end());
            }

            echo::category("isobus.tc").debug("DDOP serialized: ", data.size(), " bytes");
            return Result<dp::Vector<u8>>::ok(std::move(data));
        }

        // Validate the pool structure
        Result<void> validate() const {
            if (devices_.empty()) {
                return Result<void>::err(Error::invalid_state("DDOP must have at least one device"));
            }
            if (elements_.empty()) {
                return Result<void>::err(Error::invalid_state("DDOP must have at least one device element"));
            }
            // Check that all parent references are valid
            for (const auto &elem : elements_) {
                if (elem.parent_id != 0) {
                    if (!object_exists(elem.parent_id)) {
                        return Result<void>::err(
                            Error(ErrorCode::PoolValidation, "element references non-existent parent"));
                    }
                }
                // Validate child object references
                for (auto child_id : elem.child_objects) {
                    if (!object_exists(child_id)) {
                        return Result<void>::err(
                            Error(ErrorCode::PoolValidation, "element references non-existent child object"));
                    }
                }
            }
            // Validate presentation object references in process data
            for (const auto &pd : process_data_) {
                if (pd.presentation_object_id != 0xFFFF) {
                    if (!vp_exists(pd.presentation_object_id)) {
                        return Result<void>::err(Error(ErrorCode::PoolValidation,
                                                       "process data references non-existent presentation object"));
                    }
                }
            }
            // Validate presentation object references in properties
            for (const auto &prop : properties_) {
                if (prop.presentation_object_id != 0xFFFF) {
                    if (!vp_exists(prop.presentation_object_id)) {
                        return Result<void>::err(
                            Error(ErrorCode::PoolValidation, "property references non-existent presentation object"));
                    }
                }
            }
            return {};
        }

        // Accessors
        const dp::Vector<DeviceObject> &devices() const noexcept { return devices_; }
        const dp::Vector<DeviceElement> &elements() const noexcept { return elements_; }
        const dp::Vector<DeviceProcessData> &process_data() const noexcept { return process_data_; }
        const dp::Vector<DeviceProperty> &properties() const noexcept { return properties_; }

        usize object_count() const noexcept {
            return devices_.size() + elements_.size() + process_data_.size() + properties_.size() +
                   value_presentations_.size();
        }

        // Fluent API (chainable)
        DDOP &with_device(DeviceObject device) {
            add_device(std::move(device));
            return *this;
        }
        DDOP &with_element(DeviceElement elem) {
            add_element(std::move(elem));
            return *this;
        }
        DDOP &with_process_data(DeviceProcessData pd) {
            add_process_data(std::move(pd));
            return *this;
        }
        DDOP &with_property(DeviceProperty prop) {
            add_property(std::move(prop));
            return *this;
        }
        DDOP &with_value_presentation(DeviceValuePresentation vp) {
            add_value_presentation(std::move(vp));
            return *this;
        }

        // ─── Deserialization ────────────────────────────────────────────────────
        // Parse a binary DDOP back into an object tree
        static Result<DDOP> deserialize(const dp::Vector<u8> &data) {
            DDOP ddop;
            usize offset = 0;

            while (offset < data.size()) {
                if (offset + 3 > data.size()) {
                    return Result<DDOP>::err(
                        Error(ErrorCode::PoolValidation, "DDOP truncated: not enough bytes for object header"));
                }

                auto obj_type = static_cast<TCObjectType>(data[offset]);
                ObjectID obj_id = static_cast<u16>(data[offset + 1]) | (static_cast<u16>(data[offset + 2]) << 8);
                offset += 3;

                switch (obj_type) {
                case TCObjectType::Device: {
                    auto result = parse_device(data, offset, obj_id);
                    if (!result.is_ok())
                        return Result<DDOP>::err(result.error());
                    ddop.devices_.push_back(std::move(result.value()));
                    break;
                }
                case TCObjectType::DeviceElement: {
                    auto result = parse_element(data, offset, obj_id);
                    if (!result.is_ok())
                        return Result<DDOP>::err(result.error());
                    ddop.elements_.push_back(std::move(result.value()));
                    break;
                }
                case TCObjectType::DeviceProcessData: {
                    auto result = parse_process_data(data, offset, obj_id);
                    if (!result.is_ok())
                        return Result<DDOP>::err(result.error());
                    ddop.process_data_.push_back(std::move(result.value()));
                    break;
                }
                case TCObjectType::DeviceProperty: {
                    auto result = parse_property(data, offset, obj_id);
                    if (!result.is_ok())
                        return Result<DDOP>::err(result.error());
                    ddop.properties_.push_back(std::move(result.value()));
                    break;
                }
                case TCObjectType::DeviceValuePresentation: {
                    auto result = parse_value_presentation(data, offset, obj_id);
                    if (!result.is_ok())
                        return Result<DDOP>::err(result.error());
                    ddop.value_presentations_.push_back(std::move(result.value()));
                    break;
                }
                default:
                    return Result<DDOP>::err(Error(ErrorCode::PoolValidation, "unknown TC object type in DDOP"));
                }

                if (obj_id >= ddop.next_id_)
                    ddop.next_id_ = obj_id + 1;
            }

            echo::category("isobus.tc").debug("DDOP deserialized: ", ddop.object_count(), " objects");
            return Result<DDOP>::ok(std::move(ddop));
        }

        // ─── ISOXML generation ─────────────────────────────────────────────────────
        // Generates ISO 11783-10 TASKDATA.xml-conformant XML fragment for this DDOP
        dp::String to_isoxml() const {
            dp::String xml;
            xml += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
            xml += "<ISO11783_TaskData VersionMajor=\"4\" VersionMinor=\"0\"";
            xml += " DataTransferOrigin=\"1\">\n";

            for (const auto &dev : devices_) {
                xml += "  <DVC A=\"DVC-";
                xml += dp::to_string(dev.id);
                xml += "\" B=\"";
                xml += xml_escape(dev.designator);
                xml += "\" C=\"";
                xml += xml_escape(dev.software_version);
                xml += "\" D=\"";
                xml += xml_escape(dev.serial_number);
                xml += "\">\n";

                // Emit elements that belong to this device
                for (const auto &elem : elements_) {
                    xml += emit_element_xml(elem);
                }

                xml += "  </DVC>\n";
            }

            // Emit value presentations as standalone elements
            for (const auto &vp : value_presentations_) {
                xml += "  <DVP A=\"DVP-";
                xml += dp::to_string(vp.id);
                xml += "\" B=\"";
                xml += dp::to_string(vp.offset);
                xml += "\" C=\"";
                xml += dp::to_string(vp.scale);
                xml += "\" D=\"";
                xml += dp::to_string(static_cast<u32>(vp.decimal_digits));
                xml += "\" E=\"";
                xml += xml_escape(vp.unit_designator);
                xml += "\"/>\n";
            }

            xml += "</ISO11783_TaskData>\n";
            return xml;
        }

        void clear() {
            devices_.clear();
            elements_.clear();
            process_data_.clear();
            properties_.clear();
            value_presentations_.clear();
            next_id_ = 0;
        }

      private:
        // ─── Deserialization helpers ────────────────────────────────────────────
        static Result<DeviceObject> parse_device(const dp::Vector<u8> &data, usize &offset, ObjectID id) {
            DeviceObject dev;
            dev.id = id;
            // Designator
            if (offset >= data.size())
                return Result<DeviceObject>::err(truncated_error());
            u8 des_len = data[offset++];
            if (offset + des_len > data.size())
                return Result<DeviceObject>::err(truncated_error());
            for (u8 i = 0; i < des_len; ++i)
                dev.designator += static_cast<char>(data[offset++]);
            // Software version
            if (offset >= data.size())
                return Result<DeviceObject>::err(truncated_error());
            u8 sw_len = data[offset++];
            if (offset + sw_len > data.size())
                return Result<DeviceObject>::err(truncated_error());
            for (u8 i = 0; i < sw_len; ++i)
                dev.software_version += static_cast<char>(data[offset++]);
            // Serial number
            if (offset >= data.size())
                return Result<DeviceObject>::err(truncated_error());
            u8 sn_len = data[offset++];
            if (offset + sn_len > data.size())
                return Result<DeviceObject>::err(truncated_error());
            for (u8 i = 0; i < sn_len; ++i)
                dev.serial_number += static_cast<char>(data[offset++]);
            // Structure label (7 bytes)
            if (offset + 7 > data.size())
                return Result<DeviceObject>::err(truncated_error());
            for (usize i = 0; i < 7; ++i)
                dev.structure_label[i] = data[offset++];
            // Localization label (7 bytes)
            if (offset + 7 > data.size())
                return Result<DeviceObject>::err(truncated_error());
            for (usize i = 0; i < 7; ++i)
                dev.localization_label[i] = data[offset++];
            return Result<DeviceObject>::ok(std::move(dev));
        }

        static Result<DeviceElement> parse_element(const dp::Vector<u8> &data, usize &offset, ObjectID id) {
            DeviceElement elem;
            elem.id = id;
            if (offset >= data.size())
                return Result<DeviceElement>::err(truncated_error());
            elem.type = static_cast<DeviceElementType>(data[offset++]);
            // Designator
            if (offset >= data.size())
                return Result<DeviceElement>::err(truncated_error());
            u8 des_len = data[offset++];
            if (offset + des_len > data.size())
                return Result<DeviceElement>::err(truncated_error());
            for (u8 i = 0; i < des_len; ++i)
                elem.designator += static_cast<char>(data[offset++]);
            // Element number
            if (offset + 2 > data.size())
                return Result<DeviceElement>::err(truncated_error());
            elem.number = static_cast<u16>(data[offset]) | (static_cast<u16>(data[offset + 1]) << 8);
            offset += 2;
            // Parent ID
            if (offset + 2 > data.size())
                return Result<DeviceElement>::err(truncated_error());
            elem.parent_id = static_cast<u16>(data[offset]) | (static_cast<u16>(data[offset + 1]) << 8);
            offset += 2;
            // Child objects
            if (offset + 2 > data.size())
                return Result<DeviceElement>::err(truncated_error());
            u16 num_children = static_cast<u16>(data[offset]) | (static_cast<u16>(data[offset + 1]) << 8);
            offset += 2;
            if (offset + num_children * 2 > data.size())
                return Result<DeviceElement>::err(truncated_error());
            for (u16 i = 0; i < num_children; ++i) {
                ObjectID child = static_cast<u16>(data[offset]) | (static_cast<u16>(data[offset + 1]) << 8);
                offset += 2;
                elem.child_objects.push_back(child);
            }
            return Result<DeviceElement>::ok(std::move(elem));
        }

        static Result<DeviceProcessData> parse_process_data(const dp::Vector<u8> &data, usize &offset, ObjectID id) {
            DeviceProcessData pd;
            pd.id = id;
            if (offset + 5 > data.size())
                return Result<DeviceProcessData>::err(truncated_error());
            pd.ddi = static_cast<u16>(data[offset]) | (static_cast<u16>(data[offset + 1]) << 8);
            offset += 2;
            pd.trigger_methods = data[offset++];
            pd.presentation_object_id = static_cast<u16>(data[offset]) | (static_cast<u16>(data[offset + 1]) << 8);
            offset += 2;
            // Designator
            if (offset >= data.size())
                return Result<DeviceProcessData>::err(truncated_error());
            u8 des_len = data[offset++];
            if (offset + des_len > data.size())
                return Result<DeviceProcessData>::err(truncated_error());
            for (u8 i = 0; i < des_len; ++i)
                pd.designator += static_cast<char>(data[offset++]);
            return Result<DeviceProcessData>::ok(std::move(pd));
        }

        static Result<DeviceProperty> parse_property(const dp::Vector<u8> &data, usize &offset, ObjectID id) {
            DeviceProperty prop;
            prop.id = id;
            if (offset + 8 > data.size())
                return Result<DeviceProperty>::err(truncated_error());
            prop.ddi = static_cast<u16>(data[offset]) | (static_cast<u16>(data[offset + 1]) << 8);
            offset += 2;
            prop.value = static_cast<i32>(static_cast<u32>(data[offset]) | (static_cast<u32>(data[offset + 1]) << 8) |
                                          (static_cast<u32>(data[offset + 2]) << 16) |
                                          (static_cast<u32>(data[offset + 3]) << 24));
            offset += 4;
            prop.presentation_object_id = static_cast<u16>(data[offset]) | (static_cast<u16>(data[offset + 1]) << 8);
            offset += 2;
            // Designator
            if (offset >= data.size())
                return Result<DeviceProperty>::err(truncated_error());
            u8 des_len = data[offset++];
            if (offset + des_len > data.size())
                return Result<DeviceProperty>::err(truncated_error());
            for (u8 i = 0; i < des_len; ++i)
                prop.designator += static_cast<char>(data[offset++]);
            return Result<DeviceProperty>::ok(std::move(prop));
        }

        static Result<DeviceValuePresentation> parse_value_presentation(const dp::Vector<u8> &data, usize &offset,
                                                                        ObjectID id) {
            DeviceValuePresentation vp;
            vp.id = id;
            if (offset + 9 > data.size())
                return Result<DeviceValuePresentation>::err(truncated_error());
            vp.offset = static_cast<i32>(static_cast<u32>(data[offset]) | (static_cast<u32>(data[offset + 1]) << 8) |
                                         (static_cast<u32>(data[offset + 2]) << 16) |
                                         (static_cast<u32>(data[offset + 3]) << 24));
            offset += 4;
            u32 scale_bits = static_cast<u32>(data[offset]) | (static_cast<u32>(data[offset + 1]) << 8) |
                             (static_cast<u32>(data[offset + 2]) << 16) | (static_cast<u32>(data[offset + 3]) << 24);
            std::memcpy(&vp.scale, &scale_bits, sizeof(f32));
            offset += 4;
            vp.decimal_digits = data[offset++];
            // Unit designator
            if (offset >= data.size())
                return Result<DeviceValuePresentation>::err(truncated_error());
            u8 des_len = data[offset++];
            if (offset + des_len > data.size())
                return Result<DeviceValuePresentation>::err(truncated_error());
            for (u8 i = 0; i < des_len; ++i)
                vp.unit_designator += static_cast<char>(data[offset++]);
            return Result<DeviceValuePresentation>::ok(std::move(vp));
        }

        static Error truncated_error() {
            return Error(ErrorCode::PoolValidation, "DDOP data truncated during deserialization");
        }

        // ─── ISOXML helpers ────────────────────────────────────────────────────
        static dp::String xml_escape(const dp::String &s) {
            dp::String out;
            for (char c : s) {
                switch (c) {
                case '&':
                    out += "&amp;";
                    break;
                case '<':
                    out += "&lt;";
                    break;
                case '>':
                    out += "&gt;";
                    break;
                case '"':
                    out += "&quot;";
                    break;
                case '\'':
                    out += "&apos;";
                    break;
                default:
                    out += c;
                    break;
                }
            }
            return out;
        }

        dp::String emit_element_xml(const DeviceElement &elem) const {
            dp::String xml;
            dp::String type_str;
            switch (elem.type) {
            case DeviceElementType::Device:
                type_str = "1";
                break;
            case DeviceElementType::Function:
                type_str = "2";
                break;
            case DeviceElementType::Bin:
                type_str = "3";
                break;
            case DeviceElementType::Section:
                type_str = "4";
                break;
            case DeviceElementType::Unit:
                type_str = "5";
                break;
            case DeviceElementType::Connector:
                type_str = "6";
                break;
            case DeviceElementType::NavigationReference:
                type_str = "7";
                break;
            }

            xml += "    <DET A=\"DET-";
            xml += dp::to_string(elem.id);
            xml += "\" B=\"";
            xml += type_str;
            xml += "\" C=\"";
            xml += xml_escape(elem.designator);
            xml += "\" D=\"";
            xml += dp::to_string(elem.number);
            xml += "\" E=\"DET-";
            xml += dp::to_string(elem.parent_id);
            xml += "\">\n";

            // Emit process data and properties that are children of this element
            for (auto child_id : elem.child_objects) {
                for (const auto &pd : process_data_) {
                    if (pd.id == child_id) {
                        xml += "      <DPD A=\"DPD-";
                        xml += dp::to_string(pd.id);
                        xml += "\" B=\"";
                        xml += dp::to_string(pd.ddi);
                        xml += "\" C=\"";
                        xml += dp::to_string(static_cast<u32>(pd.trigger_methods));
                        xml += "\" D=\"";
                        xml += xml_escape(pd.designator);
                        xml += "\"";
                        if (pd.presentation_object_id != 0xFFFF) {
                            xml += " E=\"DVP-";
                            xml += dp::to_string(pd.presentation_object_id);
                            xml += "\"";
                        }
                        xml += "/>\n";
                    }
                }
                for (const auto &prop : properties_) {
                    if (prop.id == child_id) {
                        xml += "      <DPT A=\"DPT-";
                        xml += dp::to_string(prop.id);
                        xml += "\" B=\"";
                        xml += dp::to_string(prop.ddi);
                        xml += "\" C=\"";
                        xml += dp::to_string(prop.value);
                        xml += "\" D=\"";
                        xml += xml_escape(prop.designator);
                        xml += "\"";
                        if (prop.presentation_object_id != 0xFFFF) {
                            xml += " E=\"DVP-";
                            xml += dp::to_string(prop.presentation_object_id);
                            xml += "\"";
                        }
                        xml += "/>\n";
                    }
                }
            }

            xml += "    </DET>\n";
            return xml;
        }
        bool object_exists(ObjectID id) const {
            for (const auto &d : devices_) {
                if (d.id == id)
                    return true;
            }
            for (const auto &e : elements_) {
                if (e.id == id)
                    return true;
            }
            for (const auto &pd : process_data_) {
                if (pd.id == id)
                    return true;
            }
            for (const auto &p : properties_) {
                if (p.id == id)
                    return true;
            }
            for (const auto &vp : value_presentations_) {
                if (vp.id == id)
                    return true;
            }
            return false;
        }

        bool vp_exists(ObjectID id) const {
            for (const auto &vp : value_presentations_) {
                if (vp.id == id)
                    return true;
            }
            return false;
        }
    };

} // namespace agrobus::isobus::tc
