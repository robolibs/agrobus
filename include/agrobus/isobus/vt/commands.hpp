#pragma once

#include <agrobus/net/types.hpp>

namespace agrobus::isobus::vt {
    using namespace agrobus::net;

    // ─── VT command function codes (ISO 11783-6) ──────────────────────────────────
    // Shared between VT client and server.
    namespace vt_cmd {
        // ─── VT-to-ECU notifications (ISO 11783-6 Table B.1) ─────────────────────
        inline constexpr u8 SOFT_KEY_ACTIVATION = 0x00;
        inline constexpr u8 BUTTON_ACTIVATION = 0x01;
        inline constexpr u8 POINTING_EVENT = 0x02;
        inline constexpr u8 NUMERIC_VALUE_CHANGE = 0x03;
        inline constexpr u8 STRING_VALUE_CHANGE = 0x04;
        inline constexpr u8 VT_STATUS = 0xFE;
        inline constexpr u8 VT_ESC = 0x09;
        inline constexpr u8 VT_CHANGE_ACTIVE_MASK = 0xAD;

        // ─── ECU-to-VT commands (ISO 11783-6 Table B.2) ──────────────────────────
        inline constexpr u8 HIDE_SHOW = 0xA0;
        inline constexpr u8 ENABLE_DISABLE = 0xA1;
        inline constexpr u8 SELECT_ACTIVE_WORKING_SET = 0xA2;
        inline constexpr u8 CHANGE_NUMERIC_VALUE = 0xA8;
        inline constexpr u8 CHANGE_STRING_VALUE = 0xB3;
        inline constexpr u8 CHANGE_ACTIVE_MASK = 0xAD;
        inline constexpr u8 CHANGE_SOFT_KEY_MASK = 0xAE;
        inline constexpr u8 CHANGE_ATTRIBUTE = 0xAF;
        inline constexpr u8 CHANGE_SIZE = 0xA4;
        inline constexpr u8 CHANGE_BACKGROUND_COLOUR = 0xA5;
        inline constexpr u8 CHANGE_CHILD_LOCATION = 0xA6;
        inline constexpr u8 CHANGE_CHILD_POSITION = 0xA7;
        inline constexpr u8 CHANGE_LIST_ITEM = 0xB0;
        inline constexpr u8 CHANGE_FILL_ATTRIBUTES = 0xB1;
        inline constexpr u8 CHANGE_FONT_ATTRIBUTES = 0xB2;
        inline constexpr u8 CHANGE_PRIORITY = 0xB4;
        inline constexpr u8 CHANGE_END_POINT = 0xB5;
        inline constexpr u8 SELECT_INPUT_OBJECT = 0x05;
        inline constexpr u8 EXECUTE_MACRO = 0xBE;
        inline constexpr u8 LOCK_UNLOCK_MASK = 0xBD;
        inline constexpr u8 ESC_INPUT = 0x92;
        inline constexpr u8 CONTROL_AUDIO_SIGNAL = 0xA3;
        inline constexpr u8 SET_AUDIO_VOLUME = 0xA9;

        // ─── Object pool transfer commands (ISO 11783-6 Annex F) ─────────────────
        inline constexpr u8 GET_MEMORY = 0xC0;
        inline constexpr u8 GET_MEMORY_RESPONSE = 0xC0;
        inline constexpr u8 STORE_POOL = 0xC1;         // Store Version command (version label)
        inline constexpr u8 LOAD_POOL = 0xC2;          // Load Version command
        inline constexpr u8 END_OF_POOL = 0xC3;        // End of Object Pool Transfer
        inline constexpr u8 POOL_ACTIVATE = 0xC4;      // not standard; maps to End of Pool response
        inline constexpr u8 DELETE_OBJECT_POOL = 0xC5; // Delete Version
        inline constexpr u8 GET_VERSIONS = 0xC7;       // Get Versions
        inline constexpr u8 GET_VERSIONS_RESPONSE = 0xC8;
        inline constexpr u8 OBJECT_POOL_TRANSFER = 0x11; // Object Pool Transfer (per ISO 11783-6 F.39)

        // ─── Extended commands ───────────────────────────────────────────────────
        inline constexpr u8 GET_ATTRIBUTE_VALUE = 0xB9;
        inline constexpr u8 GET_NUMBER_SOFTKEYS = 0xC6;
        inline constexpr u8 GET_TEXT_FONT_DATA = 0xC9;
        inline constexpr u8 GET_HARDWARE = 0xCA;
        inline constexpr u8 GET_SUPPORTED_WIDECHARS = 0xCB;
        inline constexpr u8 GET_WINDOW_MASK_DATA = 0xCC;
        inline constexpr u8 GET_SUPPORTED_OBJECTS = 0xCD;

        // ─── VT v5 Extended Version commands (ISO 11783-6:2018 Annex F) ─────────
        inline constexpr u8 EXTENDED_GET_VERSIONS = 0xC0;
        inline constexpr u8 EXTENDED_STORE_VERSION = 0xC1;
        inline constexpr u8 EXTENDED_LOAD_VERSION = 0xC2;
        inline constexpr u8 EXTENDED_DELETE_VERSION = 0xC3;
        inline constexpr u8 EXTENDED_GET_VERSIONS_RESPONSE = 0xC4;

        // Extended version label size (VT v5: 32 bytes vs classic 7 bytes)
        inline constexpr usize EXTENDED_VERSION_LABEL_SIZE = 32;
        inline constexpr usize CLASSIC_VERSION_LABEL_SIZE = 7;

        // VT v5 extended version sub-function identifier (byte 1 distinguishes from classic)
        inline constexpr u8 EXTENDED_VERSION_SUBFUNCTION = 0xFE;
    } // namespace vt_cmd

    // ─── Button/Key activation codes ─────────────────────────────────────────────
    enum class ActivationCode : u8 { Released = 0, Pressed = 1, Held = 2, Aborted = 3 };

    // ─── Key activation code (alias for server compatibility) ────────────────────
    enum class KeyActivationCode : u8 { Released = 0, Pressed = 1, Held = 2, Aborted = 3 };

} // namespace agrobus::isobus::vt
