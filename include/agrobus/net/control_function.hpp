#pragma once

#include <agrobus/net/constants.hpp>
#include <agrobus/net/name.hpp>
#include <agrobus/net/types.hpp>

namespace agrobus::net {

    // ─── Control function type ───────────────────────────────────────────────────
    enum class CFType { Internal, External, Partnered };
    enum class CFState { Online, Offline };

    // ─── Base control function (address + NAME) ──────────────────────────────────
    struct ControlFunction {
        Name name;
        Address address = NULL_ADDRESS;
        u8 can_port = 0;
        CFType type = CFType::External;
        CFState state = CFState::Offline;

        bool address_valid() const noexcept { return address <= MAX_ADDRESS; }

        bool is_online() const noexcept { return state == CFState::Online; }
    };
} // namespace agrobus::net
