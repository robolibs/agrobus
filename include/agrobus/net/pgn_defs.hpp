#pragma once

#include <agrobus/net/types.hpp>

namespace agrobus::net {

    // ═════════════════════════════════════════════════════════════════════════════
    // ALL PGN DEFINITIONS
    //
    // Consolidated from ISOBUS (ISO 11783), J1939, and NMEA2000 standards.
    // Every PGN constant in the library lives here.
    // ═════════════════════════════════════════════════════════════════════════════

    // ─── Core Protocol (ISO 11783-3/5, J1939-21) ─────────────────────────────────
    inline constexpr PGN PGN_REQUEST = 0xEA00;
    inline constexpr PGN PGN_ADDRESS_CLAIMED = 0xEE00;
    inline constexpr PGN PGN_COMMANDED_ADDRESS = 0xFED8;
    inline constexpr PGN PGN_TP_CM = 0xEC00;
    inline constexpr PGN PGN_TP_DT = 0xEB00;
    inline constexpr PGN PGN_ETP_CM = 0xC800;
    inline constexpr PGN PGN_ETP_DT = 0xC700;
    inline constexpr PGN PGN_ACKNOWLEDGMENT = 0xE800;
    inline constexpr PGN PGN_REQUEST2 = 0xC900; // Request2 (ISO 11783-3)
    inline constexpr PGN PGN_TRANSFER = 0xCA00; // Transfer PGN (ISO 11783-3)

    // ─── Proprietary Messages (ISO 11783-3, Section 5.4.6) ─────────────────────
    inline constexpr PGN PGN_PROPRIETARY_A = 0xEF00;      // Destination-specific, up to 1785 bytes
    inline constexpr PGN PGN_PROPRIETARY_A2 = 0x1EF00;    // Extended data page Proprietary A
    inline constexpr PGN PGN_PROPRIETARY_B_BASE = 0xFF00; // Broadcast range: 0xFF00-0xFFFF

    // ─── Diagnostics (J1939-73) ──────────────────────────────────────────────────
    inline constexpr PGN PGN_DM1 = 0xFECA;
    inline constexpr PGN PGN_DM2 = 0xFECB;
    inline constexpr PGN PGN_DM3 = 0xFECC;
    inline constexpr PGN PGN_DM11 = 0xFED3;
    inline constexpr PGN PGN_DM13 = 0xDF00;
    inline constexpr PGN PGN_DM14 = 0xD900; // Memory access request
    inline constexpr PGN PGN_DM15 = 0xD800; // Memory access response
    inline constexpr PGN PGN_DM16 = 0xD700; // Binary data transfer
    inline constexpr PGN PGN_DM22 = 0xC300;
    inline constexpr PGN PGN_DIAGNOSTIC_PROTOCOL_ID = 0xFD32;
    inline constexpr PGN PGN_ECU_IDENTIFICATION = 0xFDC5;

    // ─── Network Management (ISO 11783-5) ────────────────────────────────────────
    inline constexpr PGN PGN_HEARTBEAT = 0xF0E4;
    inline constexpr PGN PGN_NAME_MANAGEMENT = 0x9300; // PGN 37632 (ISO 11783-5, Sec 4.4.3)
    inline constexpr PGN PGN_SOFTWARE_ID = 0xFEDA;
    inline constexpr PGN PGN_PRODUCT_IDENTIFICATION = 0xFC8D;
    inline constexpr PGN PGN_CF_FUNCTIONALITIES = 0xFC8E;
    inline constexpr PGN PGN_WORKING_SET_MASTER = 0xFE0D;
    inline constexpr PGN PGN_LANGUAGE_COMMAND = 0xFE0F;
    inline constexpr PGN PGN_MAINTAIN_POWER = 0xFE47;

    // ─── Virtual Terminal (ISO 11783-6) ──────────────────────────────────────────
    inline constexpr PGN PGN_VT_TO_ECU = 0xE600;
    inline constexpr PGN PGN_ECU_TO_VT = 0xE700;
    inline constexpr PGN PGN_SHORTCUT_BUTTON = 0xFDB6;

    // ─── Task Controller (ISO 11783-10) ──────────────────────────────────────────
    inline constexpr PGN PGN_TC_TO_ECU = 0xCB00;
    inline constexpr PGN PGN_ECU_TO_TC = 0xCC00;

    // ─── Vehicle / Machine Speed (J1939 / ISO 11783-7) ───────────────────────────
    inline constexpr PGN PGN_TIME_DATE = 0xFEE6;
    inline constexpr PGN PGN_VEHICLE_SPEED = 0xFEF1;
    inline constexpr PGN PGN_WHEEL_SPEED = 0xFE48;
    inline constexpr PGN PGN_GROUND_SPEED = 0xFE49;
    inline constexpr PGN PGN_MACHINE_SPEED = 0xF022;

    // ─── Guidance (ISO 11783-7) ──────────────────────────────────────────────────
    inline constexpr PGN PGN_GUIDANCE_MACHINE = 0xFE44;
    inline constexpr PGN PGN_GUIDANCE_SYSTEM = 0xFE45;

    // ─── Auxiliary Functions (ISO 11783-11) ──────────────────────────────────────
    inline constexpr PGN PGN_AUX_ASSIGNMENT = 0xFD20;
    inline constexpr PGN PGN_AUX_INPUT_STATUS = 0xFD21;
    inline constexpr PGN PGN_AUX_INPUT_TYPE2 = 0xFD22;

    // ─── File Transfer (ISO 11783-13) ────────────────────────────────────────────
    inline constexpr PGN PGN_FILE_SERVER_TO_CLIENT = 0xAB00; // PGN 43776 (Server → Client)
    inline constexpr PGN PGN_FILE_CLIENT_TO_SERVER = 0xAA00; // PGN 43520 (Client → Server)

    // ─── Network Interconnection Unit (ISO 11783-4) ──────────────────────────────
    inline constexpr PGN PGN_NIU_NETWORK_MSG = 0xED00; // PGN 60672 (NIU message, Sec 6.5)

    // ─── Sequence Control (ISO 11783-14) ─────────────────────────────────────────
    inline constexpr PGN PGN_SC_MASTER_STATUS = 0x8E00; // PGN 36352 (SCM → SCC)
    inline constexpr PGN PGN_SC_CLIENT_STATUS = 0x8D00; // PGN 36096 (SCC → SCM)

    // ─── Tractor Implement Management (ISO 11783-7/9) ────────────────────────────
    inline constexpr PGN PGN_FRONT_PTO = 0xFE54;
    inline constexpr PGN PGN_REAR_PTO = 0xF003;
    inline constexpr PGN PGN_FRONT_HITCH = 0xFE08;
    inline constexpr PGN PGN_REAR_HITCH = 0xF005;
    inline constexpr PGN PGN_AUX_VALVE_0_7 = 0xFE20;
    inline constexpr PGN PGN_AUX_VALVE_8_15 = 0xFE21;
    inline constexpr PGN PGN_AUX_VALVE_16_23 = 0xFE22;
    inline constexpr PGN PGN_AUX_VALVE_24_31 = 0xFE23;

    // ─── Tractor Commands (ISO 11783-7 Section 11) ───────────────────────────────
    inline constexpr PGN PGN_REAR_HITCH_CMD = 0xFE50;
    inline constexpr PGN PGN_FRONT_HITCH_CMD = 0xFE51;
    inline constexpr PGN PGN_REAR_PTO_CMD = 0xFE52;
    inline constexpr PGN PGN_FRONT_PTO_CMD = 0xFE53;
    inline constexpr PGN PGN_AUX_VALVE_CMD = 0xFE30;
    inline constexpr PGN PGN_TRACTOR_CONTROL_MODE = 0xFE0B;
    inline constexpr PGN PGN_MACHINE_SELECTED_SPEED_CMD = 0xFD43;

    // ─── Tractor Facilities (ISO 11783-9) ────────────────────────────────────────
    inline constexpr PGN PGN_TRACTOR_FACILITIES_RESPONSE = 0xFE09;
    inline constexpr PGN PGN_REQUIRED_TRACTOR_FACILITIES = 0xFE0A;

    // ─── Auxiliary Valve Flow (ISO 11783-7 Class 2 TECU) ─────────────────────────
    inline constexpr PGN PGN_AUX_VALVE_ESTIMATED_FLOW_BASE = 0xFE10;
    inline constexpr PGN PGN_AUX_VALVE_MEASURED_FLOW_BASE = 0xFE20;

    // ─── Lighting (ISO 11783-7 Section 4.5) ──────────────────────────────────────
    inline constexpr PGN PGN_LIGHTING_DATA = 0xFE40;
    inline constexpr PGN PGN_LIGHTING_COMMAND = 0xFE41;

    // ─── Speed / Distance / Direction (ISO 11783-7) ────────────────────────────
    // Note: PGN_WHEEL_SPEED (0xFE48) and PGN_GROUND_SPEED (0xFE49) defined above
    // These aliases clarify the full message name:
    inline constexpr PGN PGN_WHEEL_BASED_SPEED_DIST = PGN_WHEEL_SPEED;   // Wheel-Based Speed & Distance
    inline constexpr PGN PGN_GROUND_BASED_SPEED_DIST = PGN_GROUND_SPEED; // Ground-Based Speed & Distance
    inline constexpr PGN PGN_MACHINE_SELECTED_SPEED = PGN_MACHINE_SPEED; // Machine Selected Speed

    // ─── Guidance Extended (ISO 11783-7 G Addendum) ────────────────────────────
    inline constexpr PGN PGN_GUIDANCE_CURVATURE_CMD = 0xFE46;
    inline constexpr PGN PGN_GUIDANCE_SYSTEM_CMD = 0xAD00; // Class xG external steering

    // ─── Drive Strategy (ISO 11783-7 Section 11) ─────────────────────────────────
    inline constexpr PGN PGN_DRIVE_STRATEGY_CMD = 0xFCCE;

    // ─── Hitch Roll/Pitch (ISO 11783-7 Section 11) ──────────────────────────────
    inline constexpr PGN PGN_FRONT_HITCH_ROLL_PITCH_CMD = 0xF100;
    inline constexpr PGN PGN_REAR_HITCH_ROLL_PITCH_CMD = 0xF102;
    inline constexpr PGN PGN_HITCH_PTO_COMBINED_CMD = 0xFE42;

    // ─── Working Set (ISO 11783-7 Section 10) ────────────────────────────────────
    inline constexpr PGN PGN_WORKING_SET_MEMBER = 0xFE0C;

    // ─── J1939 Engine / Powertrain ─────────────────────────────────────────────
    inline constexpr PGN PGN_EEC1 = 0x0F004;               // Electronic Engine Controller 1
    inline constexpr PGN PGN_EEC2 = 0x0F003;               // Electronic Engine Controller 2
    inline constexpr PGN PGN_EEC3 = 0x0FEC0;               // Electronic Engine Controller 3
    inline constexpr PGN PGN_ET1 = 0x0FEEE;                // Engine Temperature 1
    inline constexpr PGN PGN_ET2 = 0x0FEED;                // Engine Temperature 2
    inline constexpr PGN PGN_EFLP = 0x0FEEF;               // Engine Fluid Level/Pressure
    inline constexpr PGN PGN_ENGINE_HOURS = 0x0FEE5;       // Engine Hours
    inline constexpr PGN PGN_FUEL_ECONOMY = 0x0FEF2;       // Fuel Economy
    inline constexpr PGN PGN_FUEL_CONSUMPTION = 0x0FEE9;   // Fuel Consumption
    inline constexpr PGN PGN_TRANSMISSION_1 = 0x0F005;     // Transmission Control 1
    inline constexpr PGN PGN_CRUISE_CONTROL = 0x0FEF1;     // Cruise Control/Vehicle Speed
    inline constexpr PGN PGN_TSC1 = 0x0F006;               // Torque/Speed Control 1
    inline constexpr PGN PGN_VEP1 = 0x0F009;               // Vehicle Electrical Power 1
    inline constexpr PGN PGN_AMBIENT_CONDITIONS = 0x0FEF5; // Ambient Conditions
    inline constexpr PGN PGN_DASH_DISPLAY = 0x0FEFC;       // Dash Display
    inline constexpr PGN PGN_VEHICLE_POSITION = 0x0FEF7;   // Vehicle Position (J1939)
    inline constexpr PGN PGN_COMPONENT_ID = 0x0FEEB;       // Component Identification
    inline constexpr PGN PGN_VEHICLE_ID = 0x0FEEC;         // Vehicle Identification

    // ═════════════════════════════════════════════════════════════════════════════
    // NMEA2000 PGNs
    // ═════════════════════════════════════════════════════════════════════════════

    // ─── NMEA2000 System / Network Management ────────────────────────────────────
    inline constexpr PGN PGN_ISO_ADDRESS_CLAIM = 60928;
    inline constexpr PGN PGN_SYSTEM_TIME = 126992;
    inline constexpr PGN PGN_PRODUCT_INFO = 126996;
    inline constexpr PGN PGN_CONFIG_INFO = 126998;
    inline constexpr PGN PGN_HEARTBEAT_N2K = 126993;

    // ─── NMEA2000 Navigation / GNSS ──────────────────────────────────────────────
    inline constexpr PGN PGN_GNSS_POSITION_RAPID = 129025; // Position, Rapid Update (8B)
    inline constexpr PGN PGN_GNSS_COG_SOG_RAPID = 129026;  // COG & SOG, Rapid Update (8B)
    inline constexpr PGN PGN_GNSS_POSITION_DELTA = 129027; // Position Delta, High Precision
    inline constexpr PGN PGN_GNSS_POSITION_DATA = 129029;  // GNSS Position Data (FP, 43+B)
    inline constexpr PGN PGN_GNSS_DOPs = 129539;           // GNSS DOPs (8B)
    inline constexpr PGN PGN_GNSS_SATELLITES_IN_VIEW = 129540;
    inline constexpr PGN PGN_LOCAL_TIME_OFFSET = 129033;
    inline constexpr PGN PGN_HEADING_TRACK = 127250;      // Vessel Heading (8B)
    inline constexpr PGN PGN_RATE_OF_TURN = 127251;       // Rate of Turn (8B)
    inline constexpr PGN PGN_HEAVE = 127252;              // Heave (8B)
    inline constexpr PGN PGN_ATTITUDE = 127257;           // Attitude (Yaw/Pitch/Roll, 8B)
    inline constexpr PGN PGN_MAGNETIC_VARIATION = 127258; // Magnetic Variation (8B)

    // Backward-compatible aliases for core GNSS PGNs
    inline constexpr PGN PGN_GNSS_POSITION = PGN_GNSS_POSITION_RAPID;
    inline constexpr PGN PGN_GNSS_COG_SOG = PGN_GNSS_COG_SOG_RAPID;
    inline constexpr PGN PGN_GNSS_POSITION_DETAIL = PGN_GNSS_POSITION_DATA;

    // ─── NMEA2000 Steering / Rudder ──────────────────────────────────────────────
    inline constexpr PGN PGN_MOB = 127233;
    inline constexpr PGN PGN_HEADING_TRACK_CONTROL = 127237;
    inline constexpr PGN PGN_RUDDER = 127245;

    // ─── NMEA2000 Engine / Propulsion ────────────────────────────────────────────
    inline constexpr PGN PGN_ENGINE_PARAMS_RAPID = 127488;
    inline constexpr PGN PGN_ENGINE_PARAMS_DYNAMIC = 127489;
    inline constexpr PGN PGN_TRANSMISSION_PARAMS = 127493;
    inline constexpr PGN PGN_ENGINE_TRIP = 127497;

    // ─── NMEA2000 Electrical / Power ─────────────────────────────────────────────
    inline constexpr PGN PGN_BINARY_SWITCH_STATUS = 127501;
    inline constexpr PGN PGN_BINARY_SWITCH_CONTROL = 127502;
    inline constexpr PGN PGN_FLUID_LEVEL = 127505;
    inline constexpr PGN PGN_DC_DETAILED_STATUS = 127506;
    inline constexpr PGN PGN_CHARGER_STATUS = 127507;
    inline constexpr PGN PGN_BATTERY_STATUS = 127508;
    inline constexpr PGN PGN_CHARGER_CONFIG = 127510;
    inline constexpr PGN PGN_BATTERY_CONFIG = 127513;
    inline constexpr PGN PGN_CONVERTER_STATUS = 127750;
    inline constexpr PGN PGN_DC_VOLTAGE_CURRENT = 127751;

    // ─── NMEA2000 Speed / Distance / Depth ───────────────────────────────────────
    inline constexpr PGN PGN_LEEWAY = 128000;
    inline constexpr PGN PGN_SPEED_WATER = 128259;
    inline constexpr PGN PGN_WATER_DEPTH = 128267;
    inline constexpr PGN PGN_DISTANCE_LOG = 128275;

    // ─── NMEA2000 Windlass ───────────────────────────────────────────────────────
    inline constexpr PGN PGN_WINDLASS_CONTROL = 128776;
    inline constexpr PGN PGN_WINDLASS_OPERATING = 128777;
    inline constexpr PGN PGN_WINDLASS_MONITORING = 128778;

    // ─── NMEA2000 Navigation Control ─────────────────────────────────────────────
    inline constexpr PGN PGN_XTE = 129283;
    inline constexpr PGN PGN_NAVIGATION_DATA = 129284;
    inline constexpr PGN PGN_ROUTE_WP_INFO = 129285;

    // ─── NMEA2000 AIS ────────────────────────────────────────────────────────────
    inline constexpr PGN PGN_AIS_CLASS_A_POSITION = 129038;
    inline constexpr PGN PGN_AIS_CLASS_B_POSITION = 129039;
    inline constexpr PGN PGN_AIS_ATON_REPORT = 129041;
    inline constexpr PGN PGN_AIS_CLASS_A_STATIC = 129794;
    inline constexpr PGN PGN_AIS_SAFETY_MSG = 129802;
    inline constexpr PGN PGN_AIS_CLASS_B_STATIC_A = 129809;
    inline constexpr PGN PGN_AIS_CLASS_B_STATIC_B = 129810;

    // ─── NMEA2000 Waypoints ──────────────────────────────────────────────────────
    inline constexpr PGN PGN_WAYPOINT_LIST = 130074;

    // ─── NMEA2000 Environment (Wind / Temp / Humidity / Pressure) ────────────────
    inline constexpr PGN PGN_WIND_DATA = 130306;
    inline constexpr PGN PGN_OUTSIDE_ENVIRONMENTAL = 130310;
    inline constexpr PGN PGN_ENVIRONMENTAL_PARAMS = 130311;
    inline constexpr PGN PGN_TEMPERATURE = 130312;
    inline constexpr PGN PGN_HUMIDITY = 130313;
    inline constexpr PGN PGN_PRESSURE = 130314;
    inline constexpr PGN PGN_SET_PRESSURE = 130315;
    inline constexpr PGN PGN_TEMPERATURE_EXT = 130316;
    inline constexpr PGN PGN_METEOROLOGICAL = 130323;

    // ─── NMEA2000 Trim / Direction ───────────────────────────────────────────────
    inline constexpr PGN PGN_TRIM_TAB = 130576;
    inline constexpr PGN PGN_DIRECTION_DATA = 130577;

} // namespace agrobus::net
