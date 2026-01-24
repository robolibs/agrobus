#pragma once

#include "control_function.hpp"
#include <agrobus/net/event.hpp>
#include <datapod/datapod.hpp>

namespace agrobus::net {

    // ─── NAME filter for partner matching ────────────────────────────────────────
    enum class NameFilterField {
        IdentityNumber,
        ManufacturerCode,
        ECUInstance,
        FunctionInstance,
        FunctionCode,
        DeviceClass,
        DeviceClassInstance,
        IndustryGroup
    };

    struct NameFilter {
        NameFilterField field;
        u32 value;

        bool matches(const Name &name) const noexcept {
            switch (field) {
            case NameFilterField::IdentityNumber:
                return name.identity_number() == value;
            case NameFilterField::ManufacturerCode:
                return name.manufacturer_code() == static_cast<u16>(value);
            case NameFilterField::ECUInstance:
                return name.ecu_instance() == static_cast<u8>(value);
            case NameFilterField::FunctionInstance:
                return name.function_instance() == static_cast<u8>(value);
            case NameFilterField::FunctionCode:
                return name.function_code() == static_cast<u8>(value);
            case NameFilterField::DeviceClass:
                return name.device_class() == static_cast<u8>(value);
            case NameFilterField::DeviceClassInstance:
                return name.device_class_instance() == static_cast<u8>(value);
            case NameFilterField::IndustryGroup:
                return name.industry_group() == static_cast<u8>(value);
            }
            return false;
        }
    };

    // ─── Partner control function (external ECU we track) ────────────────────────
    class PartnerCF {
        ControlFunction cf_;
        dp::Vector<NameFilter> filters_;

      public:
        PartnerCF(u8 port, dp::Vector<NameFilter> filters) : filters_(std::move(filters)) {
            cf_.can_port = port;
            cf_.type = CFType::Partnered;
        }

        const ControlFunction &cf() const noexcept { return cf_; }
        ControlFunction &cf() noexcept { return cf_; }
        Name name() const noexcept { return cf_.name; }
        Address address() const noexcept { return cf_.address; }
        u8 port() const noexcept { return cf_.can_port; }
        const dp::Vector<NameFilter> &filters() const noexcept { return filters_; }

        void set_address(Address addr) noexcept { cf_.address = addr; }
        void set_name(Name name) noexcept { cf_.name = name; }
        void set_state(CFState state) noexcept { cf_.state = state; }

        bool matches_name(const Name &name) const noexcept {
            for (const auto &filter : filters_) {
                if (!filter.matches(name))
                    return false;
            }
            return true;
        }

        // Events
        Event<Address> on_partner_found;
        Event<> on_partner_lost;
    };
} // namespace agrobus::net
