#pragma once

// ─── ISO 11783-11 Data Dictionary (DDI) Database ─────────────────────────────
// Generated from ISO 11783-11 online database export, Version: 2025121001
//
// Copyright International Organization for Standardization, see:
// www.iso.org/iso/copyright.htm
// No reproduction on networking permitted without license from ISO.
// The export file from the online data base is supplied without liability.
// Hard and Saved copies of this document are considered uncontrolled and
// represents a snap-shot of the ISO11783-11 online data base.
//
// Total entries: 760
// DDI range: 0 - 65535
// ─────────────────────────────────────────────────────────────────────────────

#include <agrobus/net/types.hpp>
#include <datapod/datapod.hpp>

namespace agrobus::isobus::tc {
    using namespace agrobus::net;

    // ─── DDI Entry structure ─────────────────────────────────────────────────
    struct DDIDefinition {
        u16 ddi;
        const char *name;
        const char *unit;
        f64 resolution;
        i32 min_value;
        i32 max_value;
    };

    // Legacy alias
    using DDIEntry = DDIDefinition;

    // ─── DDI Constants (ISO 11783-11, complete) ──────────────────────────────
    namespace ddi {

        inline constexpr u16 INTERNAL_DATA_BASE_DDI = 0;                                         // 0x0000
        inline constexpr u16 SETPOINT_VOLUME_PER_AREA_APPLICATION_RATE = 1;                      // 0x0001
        inline constexpr u16 ACTUAL_VOLUME_PER_AREA_APPLICATION_RATE = 2;                        // 0x0002
        inline constexpr u16 DEFAULT_VOLUME_PER_AREA_APPLICATION_RATE = 3;                       // 0x0003
        inline constexpr u16 MINIMUM_VOLUME_PER_AREA_APPLICATION_RATE = 4;                       // 0x0004
        inline constexpr u16 MAXIMUM_VOLUME_PER_AREA_APPLICATION_RATE = 5;                       // 0x0005
        inline constexpr u16 SETPOINT_MASS_PER_AREA_APPLICATION_RATE = 6;                        // 0x0006
        inline constexpr u16 ACTUAL_MASS_PER_AREA_APPLICATION_RATE = 7;                          // 0x0007
        inline constexpr u16 DEFAULT_MASS_PER_AREA_APPLICATION_RATE = 8;                         // 0x0008
        inline constexpr u16 MINIMUM_MASS_PER_AREA_APPLICATION_RATE = 9;                         // 0x0009
        inline constexpr u16 MAXIMUM_MASS_PER_AREA_APPLICATION_RATE = 10;                        // 0x000A
        inline constexpr u16 SETPOINT_COUNT_PER_AREA_APPLICATION_RATE = 11;                      // 0x000B
        inline constexpr u16 ACTUAL_COUNT_PER_AREA_APPLICATION_RATE = 12;                        // 0x000C
        inline constexpr u16 DEFAULT_COUNT_PER_AREA_APPLICATION_RATE = 13;                       // 0x000D
        inline constexpr u16 MINIMUM_COUNT_PER_AREA_APPLICATION_RATE = 14;                       // 0x000E
        inline constexpr u16 MAXIMUM_COUNT_PER_AREA_APPLICATION_RATE = 15;                       // 0x000F
        inline constexpr u16 SETPOINT_SPACING_APPLICATION_RATE = 16;                             // 0x0010
        inline constexpr u16 ACTUAL_SPACING_APPLICATION_RATE = 17;                               // 0x0011
        inline constexpr u16 DEFAULT_SPACING_APPLICATION_RATE = 18;                              // 0x0012
        inline constexpr u16 MINIMUM_SPACING_APPLICATION_RATE = 19;                              // 0x0013
        inline constexpr u16 MAXIMUM_SPACING_APPLICATION_RATE = 20;                              // 0x0014
        inline constexpr u16 SETPOINT_VOLUME_PER_VOLUME_APPLICATION_RATE = 21;                   // 0x0015
        inline constexpr u16 ACTUAL_VOLUME_PER_VOLUME_APPLICATION_RATE = 22;                     // 0x0016
        inline constexpr u16 DEFAULT_VOLUME_PER_VOLUME_APPLICATION_RATE = 23;                    // 0x0017
        inline constexpr u16 MINIMUM_VOLUME_PER_VOLUME_APPLICATION_RATE = 24;                    // 0x0018
        inline constexpr u16 MAXIMUM_VOLUME_PER_VOLUME_APPLICATION_RATE = 25;                    // 0x0019
        inline constexpr u16 SETPOINT_MASS_PER_MASS_APPLICATION_RATE = 26;                       // 0x001A
        inline constexpr u16 ACTUAL_MASS_PER_MASS_APPLICATION_RATE = 27;                         // 0x001B
        inline constexpr u16 DEFAULT_MASS_PER_MASS_APPLICATION_RATE = 28;                        // 0x001C
        inline constexpr u16 MINIMUM_MASS_PER_MASS_APPLICATION_RATE = 29;                        // 0x001D
        inline constexpr u16 MAXIMUMMASS_PER_MASS_APPLICATION_RATE = 30;                         // 0x001E
        inline constexpr u16 SETPOINT_VOLUME_PER_MASS_APPLICATION_RATE = 31;                     // 0x001F
        inline constexpr u16 ACTUAL_VOLUME_PER_MASS_APPLICATION_RATE = 32;                       // 0x0020
        inline constexpr u16 DEFAULT_VOLUME_PER_MASS_APPLICATION_RATE = 33;                      // 0x0021
        inline constexpr u16 MINIMUM_VOLUME_PER_MASS_APPLICATION_RATE = 34;                      // 0x0022
        inline constexpr u16 MAXIMUM_VOLUME_PER_MASS_APPLICATION_RATE = 35;                      // 0x0023
        inline constexpr u16 SETPOINT_VOLUME_PER_TIME_APPLICATION_RATE = 36;                     // 0x0024
        inline constexpr u16 ACTUAL_VOLUME_PER_TIME_APPLICATION_RATE = 37;                       // 0x0025
        inline constexpr u16 DEFAULT_VOLUME_PER_TIME_APPLICATION_RATE = 38;                      // 0x0026
        inline constexpr u16 MINIMUM_VOLUME_PER_TIME_APPLICATION_RATE = 39;                      // 0x0027
        inline constexpr u16 MAXIMUM_VOLUME_PER_TIME_APPLICATION_RATE = 40;                      // 0x0028
        inline constexpr u16 SETPOINT_MASS_PER_TIME_APPLICATION_RATE = 41;                       // 0x0029
        inline constexpr u16 ACTUAL_MASS_PER_TIME_APPLICATION_RATE = 42;                         // 0x002A
        inline constexpr u16 DEFAULT_MASS_PER_TIME_APPLICATION_RATE = 43;                        // 0x002B
        inline constexpr u16 MINIMUM_MASS_PER_TIME_APPLICATION_RATE = 44;                        // 0x002C
        inline constexpr u16 MAXIMUM_MASS_PER_TIME_APPLICATION_RATE = 45;                        // 0x002D
        inline constexpr u16 SETPOINT_COUNT_PER_TIME_APPLICATION_RATE = 46;                      // 0x002E
        inline constexpr u16 ACTUAL_COUNT_PER_TIME_APPLICATION_RATE = 47;                        // 0x002F
        inline constexpr u16 DEFAULT_COUNT_PER_TIME_APPLICATION_RATE = 48;                       // 0x0030
        inline constexpr u16 MINIMUM_COUNT_PER_TIME_APPLICATION_RATE = 49;                       // 0x0031
        inline constexpr u16 MAXIMUM_COUNT_PER_TIME_APPLICATION_RATE = 50;                       // 0x0032
        inline constexpr u16 SETPOINT_TILLAGE_DEPTH = 51;                                        // 0x0033
        inline constexpr u16 ACTUAL_TILLAGE_DEPTH = 52;                                          // 0x0034
        inline constexpr u16 DEFAULT_TILLAGE_DEPTH = 53;                                         // 0x0035
        inline constexpr u16 MINIMUM_TILLAGE_DEPTH = 54;                                         // 0x0036
        inline constexpr u16 MAXIMUM_TILLAGE_DEPTH = 55;                                         // 0x0037
        inline constexpr u16 SETPOINT_SEEDING_DEPTH = 56;                                        // 0x0038
        inline constexpr u16 ACTUAL_SEEDING_DEPTH = 57;                                          // 0x0039
        inline constexpr u16 DEFAULT_SEEDING_DEPTH = 58;                                         // 0x003A
        inline constexpr u16 MINIMUM_SEEDING_DEPTH = 59;                                         // 0x003B
        inline constexpr u16 MAXIMUM_SEEDING_DEPTH = 60;                                         // 0x003C
        inline constexpr u16 SETPOINT_WORKING_HEIGHT = 61;                                       // 0x003D
        inline constexpr u16 ACTUAL_WORKING_HEIGHT = 62;                                         // 0x003E
        inline constexpr u16 DEFAULT_WORKING_HEIGHT = 63;                                        // 0x003F
        inline constexpr u16 MINIMUM_WORKING_HEIGHT = 64;                                        // 0x0040
        inline constexpr u16 MAXIMUM_WORKING_HEIGHT = 65;                                        // 0x0041
        inline constexpr u16 SETPOINT_WORKING_WIDTH = 66;                                        // 0x0042
        inline constexpr u16 ACTUAL_WORKING_WIDTH = 67;                                          // 0x0043
        inline constexpr u16 DEFAULT_WORKING_WIDTH = 68;                                         // 0x0044
        inline constexpr u16 MINIMUM_WORKING_WIDTH = 69;                                         // 0x0045
        inline constexpr u16 MAXIMUM_WORKING_WIDTH = 70;                                         // 0x0046
        inline constexpr u16 SETPOINT_VOLUME_CONTENT = 71;                                       // 0x0047
        inline constexpr u16 ACTUAL_VOLUME_CONTENT = 72;                                         // 0x0048
        inline constexpr u16 MAXIMUM_VOLUME_CONTENT = 73;                                        // 0x0049
        inline constexpr u16 SETPOINT_MASS_CONTENT = 74;                                         // 0x004A
        inline constexpr u16 ACTUAL_MASS_CONTENT = 75;                                           // 0x004B
        inline constexpr u16 MAXIMUM_MASS_CONTENT = 76;                                          // 0x004C
        inline constexpr u16 SETPOINT_COUNT_CONTENT = 77;                                        // 0x004D
        inline constexpr u16 ACTUAL_COUNT_CONTENT = 78;                                          // 0x004E
        inline constexpr u16 MAXIMUM_COUNT_CONTENT = 79;                                         // 0x004F
        inline constexpr u16 APPLICATION_TOTAL_VOLUME = 80;                                      // 0x0050
        inline constexpr u16 APPLICATION_TOTAL_MASS = 81;                                        // 0x0051
        inline constexpr u16 APPLICATION_TOTAL_COUNT = 82;                                       // 0x0052
        inline constexpr u16 VOLUME_PER_AREA_YIELD = 83;                                         // 0x0053
        inline constexpr u16 MASS_PER_AREA_YIELD = 84;                                           // 0x0054
        inline constexpr u16 COUNT_PER_AREA_YIELD = 85;                                          // 0x0055
        inline constexpr u16 VOLUME_PER_TIME_YIELD = 86;                                         // 0x0056
        inline constexpr u16 MASS_PER_TIME_YIELD = 87;                                           // 0x0057
        inline constexpr u16 COUNT_PER_TIME_YIELD = 88;                                          // 0x0058
        inline constexpr u16 YIELD_TOTAL_VOLUME = 89;                                            // 0x0059
        inline constexpr u16 YIELD_TOTAL_MASS = 90;                                              // 0x005A
        inline constexpr u16 YIELD_TOTAL_COUNT = 91;                                             // 0x005B
        inline constexpr u16 VOLUME_PER_AREA_CROP_LOSS = 92;                                     // 0x005C
        inline constexpr u16 MASS_PER_AREA_CROP_LOSS = 93;                                       // 0x005D
        inline constexpr u16 COUNT_PER_AREA_CROP_LOSS = 94;                                      // 0x005E
        inline constexpr u16 VOLUME_PER_TIME_CROP_LOSS = 95;                                     // 0x005F
        inline constexpr u16 MASS_PER_TIME_CROP_LOSS = 96;                                       // 0x0060
        inline constexpr u16 COUNT_PER_TIME_CROP_LOSS = 97;                                      // 0x0061
        inline constexpr u16 PERCENTAGE_CROP_LOSS = 98;                                          // 0x0062
        inline constexpr u16 CROP_MOISTURE = 99;                                                 // 0x0063
        inline constexpr u16 CROP_CONTAMINATION = 100;                                           // 0x0064
        inline constexpr u16 SETPOINT_BALE_WIDTH = 101;                                          // 0x0065
        inline constexpr u16 ACTUAL_BALE_WIDTH = 102;                                            // 0x0066
        inline constexpr u16 DEFAULT_BALE_WIDTH = 103;                                           // 0x0067
        inline constexpr u16 MINIMUM_BALE_WIDTH = 104;                                           // 0x0068
        inline constexpr u16 MAXIMUM_BALE_WIDTH = 105;                                           // 0x0069
        inline constexpr u16 SETPOINT_BALE_HEIGHT = 106;                                         // 0x006A
        inline constexpr u16 ACTUAL_BALE_HEIGHT = 107;                                           // 0x006B
        inline constexpr u16 DEFAULT_BALE_HEIGHT = 108;                                          // 0x006C
        inline constexpr u16 MINIMUM_BALE_HEIGHT = 109;                                          // 0x006D
        inline constexpr u16 MAXIMUM_BALE_HEIGHT = 110;                                          // 0x006E
        inline constexpr u16 SETPOINT_BALE_SIZE = 111;                                           // 0x006F
        inline constexpr u16 ACTUAL_BALE_SIZE = 112;                                             // 0x0070
        inline constexpr u16 DEFAULT_BALE_SIZE = 113;                                            // 0x0071
        inline constexpr u16 MINIMUM_BALE_SIZE = 114;                                            // 0x0072
        inline constexpr u16 MAXIMUM_BALE_SIZE = 115;                                            // 0x0073
        inline constexpr u16 TOTAL_AREA = 116;                                                   // 0x0074
        inline constexpr u16 EFFECTIVE_TOTAL_DISTANCE = 117;                                     // 0x0075
        inline constexpr u16 INEFFECTIVE_TOTAL_DISTANCE = 118;                                   // 0x0076
        inline constexpr u16 EFFECTIVE_TOTAL_TIME = 119;                                         // 0x0077
        inline constexpr u16 INEFFECTIVE_TOTAL_TIME = 120;                                       // 0x0078
        inline constexpr u16 PRODUCT_DENSITY_MASS_PER_VOLUME = 121;                              // 0x0079
        inline constexpr u16 PRODUCT_DENSITY_MASS_PERCOUNT = 122;                                // 0x007A
        inline constexpr u16 PRODUCT_DENSITY_VOLUME_PER_COUNT = 123;                             // 0x007B
        inline constexpr u16 AUXILIARY_VALVE_SCALING_EXTEND = 124;                               // 0x007C
        inline constexpr u16 AUXILIARY_VALVE_SCALING_RETRACT = 125;                              // 0x007D
        inline constexpr u16 AUXILIARY_VALVE_RAMP_EXTEND_UP = 126;                               // 0x007E
        inline constexpr u16 AUXILIARY_VALVE_RAMP_EXTEND_DOWN = 127;                             // 0x007F
        inline constexpr u16 AUXILIARY_VALVE_RAMP_RETRACT_UP = 128;                              // 0x0080
        inline constexpr u16 AUXILIARY_VALVE_RAMP_RETRACT_DOWN = 129;                            // 0x0081
        inline constexpr u16 AUXILIARY_VALVE_FLOAT_THRESHOLD = 130;                              // 0x0082
        inline constexpr u16 AUXILIARY_VALVE_PROGRESSIVITY_EXTEND = 131;                         // 0x0083
        inline constexpr u16 AUXILIARY_VALVE_PROGRESSIVITY_RETRACT = 132;                        // 0x0084
        inline constexpr u16 AUXILIARY_VALVE_INVERT_PORTS = 133;                                 // 0x0085
        inline constexpr u16 DEVICE_ELEMENT_OFFSET_X = 134;                                      // 0x0086
        inline constexpr u16 DEVICE_ELEMENT_OFFSET_Y = 135;                                      // 0x0087
        inline constexpr u16 DEVICE_ELEMENT_OFFSET_Z = 136;                                      // 0x0088
        inline constexpr u16 DEVICE_VOLUME_CAPACITY_DEPRECATED = 137;                            // 0x0089
        inline constexpr u16 DEVICE_MASS_CAPACITY_DEPRECATED = 138;                              // 0x008A
        inline constexpr u16 DEVICE_COUNT_CAPACITY_DEPRECATED = 139;                             // 0x008B
        inline constexpr u16 SETPOINT_PERCENTAGE_APPLICATION_RATE = 140;                         // 0x008C
        inline constexpr u16 ACTUAL_WORK_STATE = 141;                                            // 0x008D
        inline constexpr u16 PHYSICAL_SETPOINT_TIME_LATENCY = 142;                               // 0x008E
        inline constexpr u16 PHYSICAL_ACTUAL_VALUE_TIME_LATENCY = 143;                           // 0x008F
        inline constexpr u16 YAW_ANGLE = 144;                                                    // 0x0090
        inline constexpr u16 ROLL_ANGLE = 145;                                                   // 0x0091
        inline constexpr u16 PITCH_ANGLE = 146;                                                  // 0x0092
        inline constexpr u16 LOG_COUNT = 147;                                                    // 0x0093
        inline constexpr u16 TOTAL_FUEL_CONSUMPTION = 148;                                       // 0x0094
        inline constexpr u16 INSTANTANEOUS_FUEL_CONSUMPTION_PER_TIME = 149;                      // 0x0095
        inline constexpr u16 INSTANTANEOUS_FUEL_CONSUMPTION_PER_AREA = 150;                      // 0x0096
        inline constexpr u16 INSTANTANEOUS_AREA_PER_TIME_CAPACITY = 151;                         // 0x0097
        inline constexpr u16 ACTUAL_NORMALIZED_DIFFERENCE_VEGETATIVE_INDEX_NDVI = 153;           // 0x0099
        inline constexpr u16 PHYSICAL_OBJECT_LENGTH = 154;                                       // 0x009A
        inline constexpr u16 PHYSICAL_OBJECT_WIDTH = 155;                                        // 0x009B
        inline constexpr u16 PHYSICAL_OBJECT_HEIGHT = 156;                                       // 0x009C
        inline constexpr u16 CONNECTOR_TYPE = 157;                                               // 0x009D
        inline constexpr u16 PRESCRIPTION_CONTROL_STATE = 158;                                   // 0x009E
        inline constexpr u16 NUMBER_OF_SUB_UNITS_PER_SECTION = 159;                              // 0x009F
        inline constexpr u16 SECTION_CONTROL_STATE = 160;                                        // 0x00A0
        inline constexpr u16 ACTUAL_CONDENSED_WORK_STATE_1_16 = 161;                             // 0x00A1
        inline constexpr u16 ACTUAL_CONDENSED_WORK_STATE_17_32 = 162;                            // 0x00A2
        inline constexpr u16 ACTUAL_CONDENSED_WORK_STATE_33_48 = 163;                            // 0x00A3
        inline constexpr u16 ACTUAL_CONDENSED_WORK_STATE_49_64 = 164;                            // 0x00A4
        inline constexpr u16 ACTUAL_CONDENSED_WORK_STATE_65_80 = 165;                            // 0x00A5
        inline constexpr u16 ACTUAL_CONDENSED_WORK_STATE_81_96 = 166;                            // 0x00A6
        inline constexpr u16 ACTUAL_CONDENSED_WORK_STATE_97_112 = 167;                           // 0x00A7
        inline constexpr u16 ACTUAL_CONDENSED_WORK_STATE_113_128 = 168;                          // 0x00A8
        inline constexpr u16 ACTUAL_CONDENSED_WORK_STATE_129_144 = 169;                          // 0x00A9
        inline constexpr u16 ACTUAL_CONDENSED_WORK_STATE_145_160 = 170;                          // 0x00AA
        inline constexpr u16 ACTUAL_CONDENSED_WORK_STATE_161_176 = 171;                          // 0x00AB
        inline constexpr u16 ACTUAL_CONDENSED_WORK_STATE_177_192 = 172;                          // 0x00AC
        inline constexpr u16 ACTUAL_CONDENSED_WORK_STATE_193_208 = 173;                          // 0x00AD
        inline constexpr u16 ACTUAL_CONDENSED_WORK_STATE_209_224 = 174;                          // 0x00AE
        inline constexpr u16 ACTUAL_CONDENSED_WORK_STATE_225_240 = 175;                          // 0x00AF
        inline constexpr u16 ACTUAL_CONDENSED_WORK_STATE_241_256 = 176;                          // 0x00B0
        inline constexpr u16 ACTUAL_LENGTH_OF_CUT = 177;                                         // 0x00B1
        inline constexpr u16 ELEMENT_TYPE_INSTANCE = 178;                                        // 0x00B2
        inline constexpr u16 ACTUAL_CULTURAL_PRACTICE = 179;                                     // 0x00B3
        inline constexpr u16 DEVICE_REFERENCE_POINT_DRP_TO_GROUND_DISTANCE = 180;                // 0x00B4
        inline constexpr u16 DRY_MASS_PER_AREA_YIELD = 181;                                      // 0x00B5
        inline constexpr u16 DRY_MASS_PER_TIME_YIELD = 182;                                      // 0x00B6
        inline constexpr u16 YIELD_TOTAL_DRY_MASS = 183;                                         // 0x00B7
        inline constexpr u16 REFERENCE_MOISTURE_FOR_DRY_MASS = 184;                              // 0x00B8
        inline constexpr u16 SEED_COTTON_MASS_PER_AREA_YIELD = 185;                              // 0x00B9
        inline constexpr u16 LINT_COTTON_MASS_PER_AREA_YIELD = 186;                              // 0x00BA
        inline constexpr u16 SEED_COTTON_MASS_PER_TIME_YIELD = 187;                              // 0x00BB
        inline constexpr u16 LINT_COTTON_MASS_PER_TIME_YIELD = 188;                              // 0x00BC
        inline constexpr u16 YIELD_TOTAL_SEED_COTTON_MASS = 189;                                 // 0x00BD
        inline constexpr u16 YIELD_TOTAL_LINT_COTTON_MASS = 190;                                 // 0x00BE
        inline constexpr u16 LINT_TURNOUT_PERCENTAGE = 191;                                      // 0x00BF
        inline constexpr u16 AMBIENT_TEMPERATURE = 192;                                          // 0x00C0
        inline constexpr u16 SETPOINT_PRODUCT_PRESSURE = 193;                                    // 0x00C1
        inline constexpr u16 ACTUAL_PRODUCT_PRESSURE = 194;                                      // 0x00C2
        inline constexpr u16 MINIMUM_PRODUCT_PRESSURE = 195;                                     // 0x00C3
        inline constexpr u16 MAXIMUM_PRODUCT_PRESSURE = 196;                                     // 0x00C4
        inline constexpr u16 SETPOINT_PUMP_OUTPUT_PRESSURE = 197;                                // 0x00C5
        inline constexpr u16 ACTUAL_PUMP_OUTPUT_PRESSURE = 198;                                  // 0x00C6
        inline constexpr u16 MINIMUM_PUMP_OUTPUT_PRESSURE = 199;                                 // 0x00C7
        inline constexpr u16 MAXIMUM_PUMP_OUTPUT_PRESSURE = 200;                                 // 0x00C8
        inline constexpr u16 SETPOINT_TANK_AGITATION_PRESSURE = 201;                             // 0x00C9
        inline constexpr u16 ACTUAL_TANK_AGITATION_PRESSURE = 202;                               // 0x00CA
        inline constexpr u16 MINIMUM_TANK_AGITATION_PRESSURE = 203;                              // 0x00CB
        inline constexpr u16 MAXIMUM_TANK_AGITATION_PRESSURE = 204;                              // 0x00CC
        inline constexpr u16 SC_SETPOINT_TURN_ON_TIME = 205;                                     // 0x00CD
        inline constexpr u16 SC_SETPOINT_TURN_OFF_TIME = 206;                                    // 0x00CE
        inline constexpr u16 WIND_SPEED = 207;                                                   // 0x00CF
        inline constexpr u16 WIND_DIRECTION = 208;                                               // 0x00D0
        inline constexpr u16 RELATIVE_HUMIDITY = 209;                                            // 0x00D1
        inline constexpr u16 SKY_CONDITIONS = 210;                                               // 0x00D2
        inline constexpr u16 LAST_BALE_FLAKES_PER_BALE = 211;                                    // 0x00D3
        inline constexpr u16 LAST_BALE_AVERAGE_MOISTURE = 212;                                   // 0x00D4
        inline constexpr u16 LAST_BALE_AVERAGE_STROKES_PER_FLAKE = 213;                          // 0x00D5
        inline constexpr u16 LIFETIME_BALE_COUNT = 214;                                          // 0x00D6
        inline constexpr u16 LIFETIME_WORKING_HOURS = 215;                                       // 0x00D7
        inline constexpr u16 ACTUAL_BALE_HYDRAULIC_PRESSURE = 216;                               // 0x00D8
        inline constexpr u16 LAST_BALE_AVERAGE_HYDRAULIC_PRESSURE = 217;                         // 0x00D9
        inline constexpr u16 SETPOINT_BALE_COMPRESSION_PLUNGER_LOAD = 218;                       // 0x00DA
        inline constexpr u16 ACTUAL_BALE_COMPRESSION_PLUNGER_LOAD = 219;                         // 0x00DB
        inline constexpr u16 LAST_BALE_AVERAGE_BALE_COMPRESSION_PLUNGER_LOAD = 220;              // 0x00DC
        inline constexpr u16 LAST_BALE_APPLIED_PRESERVATIVE = 221;                               // 0x00DD
        inline constexpr u16 LAST_BALE_TAG_NUMBER = 222;                                         // 0x00DE
        inline constexpr u16 LAST_BALE_MASS = 223;                                               // 0x00DF
        inline constexpr u16 DELTA_T = 224;                                                      // 0x00E0
        inline constexpr u16 SETPOINT_WORKING_LENGTH = 225;                                      // 0x00E1
        inline constexpr u16 ACTUAL_WORKING_LENGTH = 226;                                        // 0x00E2
        inline constexpr u16 MINIMUM_WORKING_LENGTH = 227;                                       // 0x00E3
        inline constexpr u16 MAXIMUM_WORKING_LENGTH = 228;                                       // 0x00E4
        inline constexpr u16 ACTUAL_NET_WEIGHT = 229;                                            // 0x00E5
        inline constexpr u16 NET_WEIGHT_STATE = 230;                                             // 0x00E6
        inline constexpr u16 SETPOINT_NET_WEIGHT = 231;                                          // 0x00E7
        inline constexpr u16 ACTUAL_GROSS_WEIGHT = 232;                                          // 0x00E8
        inline constexpr u16 GROSS_WEIGHT_STATE = 233;                                           // 0x00E9
        inline constexpr u16 MINIMUM_GROSS_WEIGHT = 234;                                         // 0x00EA
        inline constexpr u16 MAXIMUM_GROSS_WEIGHT = 235;                                         // 0x00EB
        inline constexpr u16 THRESHER_ENGAGEMENT_TOTAL_TIME = 236;                               // 0x00EC
        inline constexpr u16 ACTUAL_HEADER_WORKING_HEIGHT_STATUS = 237;                          // 0x00ED
        inline constexpr u16 ACTUAL_HEADER_ROTATIONAL_SPEED_STATUS = 238;                        // 0x00EE
        inline constexpr u16 YIELD_HOLD_STATUS = 239;                                            // 0x00EF
        inline constexpr u16 ACTUAL_UN_LOADING_SYSTEM_STATUS = 240;                              // 0x00F0
        inline constexpr u16 CROP_TEMPERATURE = 241;                                             // 0x00F1
        inline constexpr u16 SETPOINT_SIEVE_CLEARANCE = 242;                                     // 0x00F2
        inline constexpr u16 ACTUAL_SIEVE_CLEARANCE = 243;                                       // 0x00F3
        inline constexpr u16 MINIMUM_SIEVE_CLEARANCE = 244;                                      // 0x00F4
        inline constexpr u16 MAXIMUM_SIEVE_CLEARANCE = 245;                                      // 0x00F5
        inline constexpr u16 SETPOINT_CHAFFER_CLEARANCE = 246;                                   // 0x00F6
        inline constexpr u16 ACTUAL_CHAFFER_CLEARANCE = 247;                                     // 0x00F7
        inline constexpr u16 MINIMUM_CHAFFER_CLEARANCE = 248;                                    // 0x00F8
        inline constexpr u16 MAXIMUM_CHAFFER_CLEARANCE = 249;                                    // 0x00F9
        inline constexpr u16 SETPOINT_CONCAVE_CLEARANCE = 250;                                   // 0x00FA
        inline constexpr u16 ACTUAL_CONCAVE_CLEARANCE = 251;                                     // 0x00FB
        inline constexpr u16 MINIMUM_CONCAVE_CLEARANCE = 252;                                    // 0x00FC
        inline constexpr u16 MAXIMUM_CONCAVE_CLEARANCE = 253;                                    // 0x00FD
        inline constexpr u16 SETPOINT_SEPARATION_FAN_ROTATIONAL_SPEED = 254;                     // 0x00FE
        inline constexpr u16 ACTUAL_SEPARATION_FAN_ROTATIONAL_SPEED = 255;                       // 0x00FF
        inline constexpr u16 MINIMUM_SEPARATION_FAN_ROTATIONAL_SPEED = 256;                      // 0x0100
        inline constexpr u16 MAXIMUM_SEPARATION_FAN_ROTATIONAL_SPEED = 257;                      // 0x0101
        inline constexpr u16 HYDRAULIC_OIL_TEMPERATURE = 258;                                    // 0x0102
        inline constexpr u16 YIELD_LAG_IGNORE_TIME = 259;                                        // 0x0103
        inline constexpr u16 YIELD_LEAD_IGNORE_TIME = 260;                                       // 0x0104
        inline constexpr u16 AVERAGE_YIELD_MASS_PER_TIME = 261;                                  // 0x0105
        inline constexpr u16 AVERAGE_CROP_MOISTURE = 262;                                        // 0x0106
        inline constexpr u16 AVERAGE_YIELD_MASS_PER_AREA = 263;                                  // 0x0107
        inline constexpr u16 CONNECTOR_PIVOT_X_OFFSET = 264;                                     // 0x0108
        inline constexpr u16 REMAINING_AREA = 265;                                               // 0x0109
        inline constexpr u16 LIFETIME_APPLICATION_TOTAL_MASS = 266;                              // 0x010A
        inline constexpr u16 LIFETIME_APPLICATION_TOTAL_COUNT = 267;                             // 0x010B
        inline constexpr u16 LIFETIME_YIELD_TOTAL_VOLUME = 268;                                  // 0x010C
        inline constexpr u16 LIFETIME_YIELD_TOTAL_MASS = 269;                                    // 0x010D
        inline constexpr u16 LIFETIME_YIELD_TOTAL_COUNT = 270;                                   // 0x010E
        inline constexpr u16 LIFETIME_TOTAL_AREA = 271;                                          // 0x010F
        inline constexpr u16 LIFETIME_EFFECTIVE_TOTAL_DISTANCE = 272;                            // 0x0110
        inline constexpr u16 LIFETIME_INEFFECTIVE_TOTAL_DISTANCE = 273;                          // 0x0111
        inline constexpr u16 LIFETIME_EFFECTIVE_TOTAL_TIME = 274;                                // 0x0112
        inline constexpr u16 LIFETIME_INEFFECTIVE_TOTAL_TIME = 275;                              // 0x0113
        inline constexpr u16 LIFETIME_FUEL_CONSUMPTION = 276;                                    // 0x0114
        inline constexpr u16 LIFETIME_AVERAGE_FUEL_CONSUMPTION_PER_TIME = 277;                   // 0x0115
        inline constexpr u16 LIFETIME_AVERAGE_FUEL_CONSUMPTION_PER_AREA = 278;                   // 0x0116
        inline constexpr u16 LIFETIME_YIELD_TOTAL_DRY_MASS = 279;                                // 0x0117
        inline constexpr u16 LIFETIME_YIELD_TOTAL_SEED_COTTON_MASS = 280;                        // 0x0118
        inline constexpr u16 LIFETIME_YIELD_TOTAL_LINT_COTTON_MASS = 281;                        // 0x0119
        inline constexpr u16 LIFETIME_THRESHING_ENGAGEMENT_TOTAL_TIME = 282;                     // 0x011A
        inline constexpr u16 PRECUT_TOTAL_COUNT = 283;                                           // 0x011B
        inline constexpr u16 UNCUT_TOTAL_COUNT = 284;                                            // 0x011C
        inline constexpr u16 LIFETIME_PRECUT_TOTAL_COUNT = 285;                                  // 0x011D
        inline constexpr u16 LIFETIME_UNCUT_TOTAL_COUNT = 286;                                   // 0x011E
        inline constexpr u16 SETPOINT_PRESCRIPTION_MODE = 287;                                   // 0x011F
        inline constexpr u16 ACTUAL_PRESCRIPTION_MODE = 288;                                     // 0x0120
        inline constexpr u16 SETPOINT_WORK_STATE = 289;                                          // 0x0121
        inline constexpr u16 SETPOINT_CONDENSED_WORK_STATE_1_16 = 290;                           // 0x0122
        inline constexpr u16 SETPOINT_CONDENSED_WORK_STATE_17_32 = 291;                          // 0x0123
        inline constexpr u16 SETPOINT_CONDENSED_WORK_STATE_33_48 = 292;                          // 0x0124
        inline constexpr u16 SETPOINT_CONDENSED_WORK_STATE_49_64 = 293;                          // 0x0125
        inline constexpr u16 SETPOINT_CONDENSED_WORK_STATE_65_80 = 294;                          // 0x0126
        inline constexpr u16 SETPOINT_CONDENSED_WORK_STATE_81_96 = 295;                          // 0x0127
        inline constexpr u16 SETPOINT_CONDENSED_WORK_STATE_97_112 = 296;                         // 0x0128
        inline constexpr u16 SETPOINT_CONDENSED_WORK_STATE_113_128 = 297;                        // 0x0129
        inline constexpr u16 SETPOINT_CONDENSED_WORK_STATE_129_144 = 298;                        // 0x012A
        inline constexpr u16 SETPOINT_CONDENSED_WORK_STATE_145_160 = 299;                        // 0x012B
        inline constexpr u16 SETPOINT_CONDENSED_WORK_STATE_161_176 = 300;                        // 0x012C
        inline constexpr u16 SETPOINT_CONDENSED_WORK_STATE_177_192 = 301;                        // 0x012D
        inline constexpr u16 SETPOINT_CONDENSED_WORK_STATE_193_208 = 302;                        // 0x012E
        inline constexpr u16 SETPOINT_CONDENSED_WORK_STATE_209_224 = 303;                        // 0x012F
        inline constexpr u16 SETPOINT_CONDENSED_WORK_STATE_225_240 = 304;                        // 0x0130
        inline constexpr u16 SETPOINT_CONDENSED_WORK_STATE_241_256 = 305;                        // 0x0131
        inline constexpr u16 TRUE_ROTATION_POINT_X_OFFSET = 306;                                 // 0x0132
        inline constexpr u16 TRUE_ROTATION_POINT_Y_OFFSET = 307;                                 // 0x0133
        inline constexpr u16 ACTUAL_PERCENTAGE_APPLICATION_RATE = 308;                           // 0x0134
        inline constexpr u16 MINIMUM_PERCENTAGE_APPLICATION_RATE = 309;                          // 0x0135
        inline constexpr u16 MAXIMUM_PERCENTAGE_APPLICATION_RATE = 310;                          // 0x0136
        inline constexpr u16 RELATIVE_YIELD_POTENTIAL = 311;                                     // 0x0137
        inline constexpr u16 MINIMUM_RELATIVE_YIELD_POTENTIAL = 312;                             // 0x0138
        inline constexpr u16 MAXIMUM_RELATIVE_YIELD_POTENTIAL = 313;                             // 0x0139
        inline constexpr u16 ACTUAL_PERCENTAGE_CROP_DRY_MATTER = 314;                            // 0x013A
        inline constexpr u16 AVERAGE_PERCENTAGE_CROP_DRY_MATTER = 315;                           // 0x013B
        inline constexpr u16 EFFECTIVE_TOTAL_FUEL_CONSUMPTION = 316;                             // 0x013C
        inline constexpr u16 INEFFECTIVE_TOTAL_FUEL_CONSUMPTION = 317;                           // 0x013D
        inline constexpr u16 EFFECTIVE_TOTAL_DIESEL_EXHAUST_FLUID_CONSUMPTION = 318;             // 0x013E
        inline constexpr u16 INEFFECTIVE_TOTAL_DIESEL_EXHAUST_FLUID_CONSUMPTION = 319;           // 0x013F
        inline constexpr u16 LAST_LOADED_WEIGHT = 320;                                           // 0x0140
        inline constexpr u16 LAST_UNLOADED_WEIGHT = 321;                                         // 0x0141
        inline constexpr u16 LOAD_IDENTIFICATION_NUMBER = 322;                                   // 0x0142
        inline constexpr u16 UNLOAD_IDENTIFICATION_NUMBER = 323;                                 // 0x0143
        inline constexpr u16 CHOPPER_ENGAGEMENT_TOTAL_TIME = 324;                                // 0x0144
        inline constexpr u16 LIFETIME_APPLICATION_TOTAL_VOLUME = 325;                            // 0x0145
        inline constexpr u16 SETPOINT_HEADER_SPEED = 326;                                        // 0x0146
        inline constexpr u16 ACTUAL_HEADER_SPEED = 327;                                          // 0x0147
        inline constexpr u16 MINIMUM_HEADER_SPEED = 328;                                         // 0x0148
        inline constexpr u16 MAXIMUM_HEADER_SPEED = 329;                                         // 0x0149
        inline constexpr u16 SETPOINT_CUTTING_DRUM_SPEED = 330;                                  // 0x014A
        inline constexpr u16 ACTUAL_CUTTING_DRUM_SPEED = 331;                                    // 0x014B
        inline constexpr u16 MINIMUM_CUTTING_DRUM_SPEED = 332;                                   // 0x014C
        inline constexpr u16 MAXIMUM_CUTTING_DRUM_SPEED = 333;                                   // 0x014D
        inline constexpr u16 OPERATING_HOURS_SINCE_LAST_SHARPENING = 334;                        // 0x014E
        inline constexpr u16 FRONT_PTO_HOURS = 335;                                              // 0x014F
        inline constexpr u16 REAR_PTO_HOURS = 336;                                               // 0x0150
        inline constexpr u16 LIFETIME_FRONT_PTO_HOURS = 337;                                     // 0x0151
        inline constexpr u16 LIFETIME_REAR_PTO_HOURS = 338;                                      // 0x0152
        inline constexpr u16 TOTAL_LOADING_TIME = 339;                                           // 0x0153
        inline constexpr u16 TOTAL_UNLOADING_TIME = 340;                                         // 0x0154
        inline constexpr u16 SETPOINT_GRAIN_KERNEL_CRACKER_GAP = 341;                            // 0x0155
        inline constexpr u16 ACTUAL_GRAIN_KERNEL_CRACKER_GAP = 342;                              // 0x0156
        inline constexpr u16 MINIMUM_GRAIN_KERNEL_CRACKER_GAP = 343;                             // 0x0157
        inline constexpr u16 MAXIMUM_GRAIN_KERNEL_CRACKER_GAP = 344;                             // 0x0158
        inline constexpr u16 SETPOINT_SWATHING_WIDTH = 345;                                      // 0x0159
        inline constexpr u16 ACTUAL_SWATHING_WIDTH = 346;                                        // 0x015A
        inline constexpr u16 MINIMUM_SWATHING_WIDTH = 347;                                       // 0x015B
        inline constexpr u16 MAXIMUM_SWATHING_WIDTH = 348;                                       // 0x015C
        inline constexpr u16 NOZZLE_DRIFT_REDUCTION = 349;                                       // 0x015D
        inline constexpr u16 FUNCTION_OR_OPERATION_TECHNIQUE = 350;                              // 0x015E
        inline constexpr u16 APPLICATION_TOTAL_VOLUME_351 = 351;                                 // 0x015F
        inline constexpr u16 APPLICATION_TOTAL_MASS_IN_GRAM_G = 352;                             // 0x0160
        inline constexpr u16 TOTAL_APPLICATION_OF_NITROGEN_N2 = 353;                             // 0x0161
        inline constexpr u16 TOTAL_APPLICATION_OF_AMMONIUM = 354;                                // 0x0162
        inline constexpr u16 TOTAL_APPLICATION_OF_PHOSPHOR = 355;                                // 0x0163
        inline constexpr u16 TOTAL_APPLICATION_OF_POTASSIUM = 356;                               // 0x0164
        inline constexpr u16 TOTAL_APPLICATION_OF_DRY_MATTER = 357;                              // 0x0165
        inline constexpr u16 AVERAGE_DRY_YIELD_MASS_PER_TIME = 358;                              // 0x0166
        inline constexpr u16 AVERAGE_DRY_YIELD_MASS_PER_AREA = 359;                              // 0x0167
        inline constexpr u16 LAST_BALE_SIZE = 360;                                               // 0x0168
        inline constexpr u16 LAST_BALE_DENSITY = 361;                                            // 0x0169
        inline constexpr u16 TOTAL_BALE_LENGTH = 362;                                            // 0x016A
        inline constexpr u16 LAST_BALE_DRY_MASS = 363;                                           // 0x016B
        inline constexpr u16 ACTUAL_FLAKE_SIZE = 364;                                            // 0x016C
        inline constexpr u16 SETPOINT_DOWNFORCE_PRESSURE = 365;                                  // 0x016D
        inline constexpr u16 ACTUAL_DOWNFORCE_PRESSURE = 366;                                    // 0x016E
        inline constexpr u16 CONDENSED_SECTION_OVERRIDE_STATE_1_16 = 367;                        // 0x016F
        inline constexpr u16 CONDENSED_SECTION_OVERRIDE_STATE_17_32 = 368;                       // 0x0170
        inline constexpr u16 CONDENSED_SECTION_OVERRIDE_STATE_33_48 = 369;                       // 0x0171
        inline constexpr u16 CONDENSED_SECTION_OVERRIDE_STATE_49_64 = 370;                       // 0x0172
        inline constexpr u16 CONDENSED_SECTION_OVERRIDE_STATE_65_80 = 371;                       // 0x0173
        inline constexpr u16 CONDENSED_SECTION_OVERRIDE_STATE_81_96 = 372;                       // 0x0174
        inline constexpr u16 CONDENSED_SECTION_OVERRIDE_STATE_97_112 = 373;                      // 0x0175
        inline constexpr u16 CONDENSED_SECTION_OVERRIDE_STATE_113_128 = 374;                     // 0x0176
        inline constexpr u16 CONDENSED_SECTION_OVERRIDE_STATE_129_144 = 375;                     // 0x0177
        inline constexpr u16 CONDENSED_SECTION_OVERRIDE_STATE_145_160 = 376;                     // 0x0178
        inline constexpr u16 CONDENSED_SECTION_OVERRIDE_STATE_161_176 = 377;                     // 0x0179
        inline constexpr u16 CONDENSED_SECTION_OVERRIDE_STATE_177_192 = 378;                     // 0x017A
        inline constexpr u16 CONDENSED_SECTION_OVERRIDE_STATE_193_208 = 379;                     // 0x017B
        inline constexpr u16 CONDENSED_SECTION_OVERRIDE_STATE_209_224 = 380;                     // 0x017C
        inline constexpr u16 CONDENSED_SECTION_OVERRIDE_STATE_225_240 = 381;                     // 0x017D
        inline constexpr u16 CONDENSED_SECTION_OVERRIDE_STATE_241_256 = 382;                     // 0x017E
        inline constexpr u16 APPARENT_WIND_DIRECTION = 383;                                      // 0x017F
        inline constexpr u16 APPARENT_WIND_SPEED = 384;                                          // 0x0180
        inline constexpr u16 MSL_ATMOSPHERIC_PRESSURE = 385;                                     // 0x0181
        inline constexpr u16 ACTUAL_ATMOSPHERIC_PRESSURE = 386;                                  // 0x0182
        inline constexpr u16 TOTAL_REVOLUTIONS_IN_FRACTIONAL_REVOLUTIONS = 387;                  // 0x0183
        inline constexpr u16 TOTAL_REVOLUTIONS_IN_COMPLETE_REVOLUTIONS = 388;                    // 0x0184
        inline constexpr u16 SETPOINT_REVOLUTIONS_SPECIFIED_AS_COUNT_PER_TIME = 389;             // 0x0185
        inline constexpr u16 ACTUAL_REVOLUTIONS_PER_TIME = 390;                                  // 0x0186
        inline constexpr u16 DEFAULT_REVOLUTIONS_PER_TIME = 391;                                 // 0x0187
        inline constexpr u16 MINIMUM_REVOLUTIONS_PER_TIME = 392;                                 // 0x0188
        inline constexpr u16 MAXIMUM_REVOLUTIONS_PER_TIME = 393;                                 // 0x0189
        inline constexpr u16 ACTUAL_FUEL_TANK_CONTENT = 394;                                     // 0x018A
        inline constexpr u16 ACTUAL_DIESEL_EXHAUST_FLUID_TANK_CONTENT = 395;                     // 0x018B
        inline constexpr u16 SETPOINT_SPEED = 396;                                               // 0x018C
        inline constexpr u16 ACTUAL_SPEED = 397;                                                 // 0x018D
        inline constexpr u16 MINIMUM_SPEED = 398;                                                // 0x018E
        inline constexpr u16 MAXIMUM_SPEED = 399;                                                // 0x018F
        inline constexpr u16 SPEED_SOURCE = 400;                                                 // 0x0190
        inline constexpr u16 ACTUAL_APPLICATION_OF_NITROGEN_N2 = 401;                            // 0x0191
        inline constexpr u16 ACTUAL_APPLICATION_OF_AMMONIUM = 402;                               // 0x0192
        inline constexpr u16 ACTUAL_APPLICATION_OF_PHOSPHOR = 403;                               // 0x0193
        inline constexpr u16 ACTUAL_APPLICATION_OF_POTASSIUM = 404;                              // 0x0194
        inline constexpr u16 ACTUAL_APPLICATION_OF_DRY_MATTER = 405;                             // 0x0195
        inline constexpr u16 ACTUAL_CRUDE_PROTEIN_CONTENT = 406;                                 // 0x0196
        inline constexpr u16 AVERAGE_CRUDE_PROTEIN_CONTENT = 407;                                // 0x0197
        inline constexpr u16 AVERAGE_CROP_CONTAMINATION = 408;                                   // 0x0198
        inline constexpr u16 TOTAL_DIESEL_EXHAUST_FLUID_CONSUMPTION = 409;                       // 0x0199
        inline constexpr u16 INSTANTANEOUS_DIESEL_EXHAUST_FLUID_CONSUMPTION_PER_TIME = 410;      // 0x019A
        inline constexpr u16 INSTANTANEOUS_DIESEL_EXHAUST_FLUID_CONSUMPTION_PER_AREA = 411;      // 0x019B
        inline constexpr u16 LIFETIME_DIESEL_EXHAUST_FLUID_CONSUMPTION = 412;                    // 0x019C
        inline constexpr u16 LIFETIME_AVERAGE_DIESEL_EXHAUST_FLUID_CONSUMPTION_PER_TIME = 413;   // 0x019D
        inline constexpr u16 LIFETIME_AVERAGE_DIESEL_EXHAUST_FLUID_CONSUMPTION_PER_AREA = 414;   // 0x019E
        inline constexpr u16 ACTUAL_SEED_SINGULATION_PERCENTAGE = 415;                           // 0x019F
        inline constexpr u16 AVERAGE_SEED_SINGULATION_PERCENTAGE = 416;                          // 0x01A0
        inline constexpr u16 ACTUAL_SEED_SKIP_PERCENTAGE = 417;                                  // 0x01A1
        inline constexpr u16 AVERAGE_SEED_SKIP_PERCENTAGE = 418;                                 // 0x01A2
        inline constexpr u16 ACTUAL_SEED_MULTIPLE_PERCENTAGE = 419;                              // 0x01A3
        inline constexpr u16 AVERAGE_SEED_MULTIPLE_PERCENTAGE = 420;                             // 0x01A4
        inline constexpr u16 ACTUAL_SEED_SPACING_DEVIATION = 421;                                // 0x01A5
        inline constexpr u16 AVERAGE_SEED_SPACING_DEVIATION = 422;                               // 0x01A6
        inline constexpr u16 ACTUAL_COEFFICIENT_OF_VARIATION_OF_SEED_SPACING_PERCENTAGE = 423;   // 0x01A7
        inline constexpr u16 AVERAGE_COEFFICIENT_OF_VARIATION_OF_SEED_SPACING_PERCENTAGE = 424;  // 0x01A8
        inline constexpr u16 SETPOINT_MAXIMUM_ALLOWED_SEED_SPACING_DEVIATION = 425;              // 0x01A9
        inline constexpr u16 SETPOINT_DOWNFORCE_AS_FORCE = 426;                                  // 0x01AA
        inline constexpr u16 ACTUAL_DOWNFORCE_AS_FORCE = 427;                                    // 0x01AB
        inline constexpr u16 LOADED_TOTAL_MASS = 428;                                            // 0x01AC
        inline constexpr u16 UNLOADED_TOTAL_MASS = 429;                                          // 0x01AD
        inline constexpr u16 LIFETIME_LOADED_TOTAL_MASS = 430;                                   // 0x01AE
        inline constexpr u16 LIFETIME_UNLOADED_TOTAL_MASS = 431;                                 // 0x01AF
        inline constexpr u16 SETPOINT_APPLICATION_RATE_OF_NITROGEN_N2 = 432;                     // 0x01B0
        inline constexpr u16 ACTUAL_APPLICATION_RATE_OF_NITROGEN_N2 = 433;                       // 0x01B1
        inline constexpr u16 MINIMUM_APPLICATION_RATE_OF_NITROGEN_N2 = 434;                      // 0x01B2
        inline constexpr u16 MAXIMUM_APPLICATION_RATE_OF_NITROGEN_N2 = 435;                      // 0x01B3
        inline constexpr u16 SETPOINT_APPLICATION_RATE_OF_AMMONIUM = 436;                        // 0x01B4
        inline constexpr u16 ACTUAL_APPLICATION_RATE_OF_AMMONIUM = 437;                          // 0x01B5
        inline constexpr u16 MINIMUM_APPLICATION_RATE_OF_AMMONIUM = 438;                         // 0x01B6
        inline constexpr u16 MAXIMUM_APPLICATION_RATE_OF_AMMONIUM = 439;                         // 0x01B7
        inline constexpr u16 SETPOINT_APPLICATION_RATE_OF_PHOSPHOR = 440;                        // 0x01B8
        inline constexpr u16 ACTUAL_APPLICATION_RATE_OF_PHOSPHOR = 441;                          // 0x01B9
        inline constexpr u16 MINIMUM_APPLICATION_RATE_OF_PHOSPHOR = 442;                         // 0x01BA
        inline constexpr u16 MAXIMUM_APPLICATION_RATE_OF_PHOSPHOR = 443;                         // 0x01BB
        inline constexpr u16 SETPOINT_APPLICATION_RATE_OF_POTASSIUM = 444;                       // 0x01BC
        inline constexpr u16 ACTUAL_APPLICATION_RATE_OF_POTASSIUM = 445;                         // 0x01BD
        inline constexpr u16 MINIMUM_APPLICATION_RATE_OF_POTASSIUM = 446;                        // 0x01BE
        inline constexpr u16 MAXIMUM_APPLICATION_RATE_OF_POTASSIUM = 447;                        // 0x01BF
        inline constexpr u16 SETPOINT_APPLICATION_RATE_OF_DRY_MATTER = 448;                      // 0x01C0
        inline constexpr u16 ACTUAL_APPLICATION_RATE_OF_DRY_MATTER = 449;                        // 0x01C1
        inline constexpr u16 MINIMUM_APPLICATION_RATE_OF_DRY_MATTER = 450;                       // 0x01C2
        inline constexpr u16 MAXIMUM_APPLICATION_RATE_OF_DRY_MATTER = 451;                       // 0x01C3
        inline constexpr u16 LOADED_TOTAL_VOLUME = 452;                                          // 0x01C4
        inline constexpr u16 UNLOADED_TOTAL_VOLUME = 453;                                        // 0x01C5
        inline constexpr u16 LIFETIME_LOADED_TOTAL_VOLUME = 454;                                 // 0x01C6
        inline constexpr u16 LIFETIME_UNLOADED_TOTAL_VOLUME = 455;                               // 0x01C7
        inline constexpr u16 LAST_LOADED_VOLUME = 456;                                           // 0x01C8
        inline constexpr u16 LAST_UNLOADED_VOLUME = 457;                                         // 0x01C9
        inline constexpr u16 LOADED_TOTAL_COUNT = 458;                                           // 0x01CA
        inline constexpr u16 UNLOADED_TOTAL_COUNT = 459;                                         // 0x01CB
        inline constexpr u16 LIFETIME_LOADED_TOTAL_COUNT = 460;                                  // 0x01CC
        inline constexpr u16 LIFETIME_UNLOADED_TOTAL_COUNT = 461;                                // 0x01CD
        inline constexpr u16 LAST_LOADED_COUNT = 462;                                            // 0x01CE
        inline constexpr u16 LAST_UNLOADED_COUNT = 463;                                          // 0x01CF
        inline constexpr u16 HAUL_COUNTER = 464;                                                 // 0x01D0
        inline constexpr u16 LIFETIME_HAUL_COUNTER = 465;                                        // 0x01D1
        inline constexpr u16 ACTUAL_RELATIVE_CONNECTOR_ANGLE = 466;                              // 0x01D2
        inline constexpr u16 ACTUAL_PERCENTAGE_CONTENT = 467;                                    // 0x01D3
        inline constexpr u16 SOIL_SNOW_FROZEN_CONDTION = 468;                                    // 0x01D4
        inline constexpr u16 ESTIMATED_SOIL_WATER_CONDTION = 469;                                // 0x01D5
        inline constexpr u16 SOIL_COMPACTION = 470;                                              // 0x01D6
        inline constexpr u16 SETPOINT_CULTURAL_PRACTICE = 471;                                   // 0x01D7
        inline constexpr u16 SETPOINT_LENGTH_OF_CUT = 472;                                       // 0x01D8
        inline constexpr u16 MINIMUM_LENGTH_OF_CUT = 473;                                        // 0x01D9
        inline constexpr u16 MAXIMUM_LENGTH_OF_CUT = 474;                                        // 0x01DA
        inline constexpr u16 SETPOINT_BALE_HYDRAULIC_PRESSURE = 475;                             // 0x01DB
        inline constexpr u16 MINIMUM_BALE_HYDRAULIC_PRESSURE = 476;                              // 0x01DC
        inline constexpr u16 MAXIMUM_BALE_HYDRAULIC_PRESSURE = 477;                              // 0x01DD
        inline constexpr u16 SETPOINT_FLAKE_SIZE = 478;                                          // 0x01DE
        inline constexpr u16 MINIMUM_FLAKE_SIZE = 479;                                           // 0x01DF
        inline constexpr u16 MAXIMUM_FLAKE_SIZE = 480;                                           // 0x01E0
        inline constexpr u16 SETPOINT_NUMBER_OF_SUBBALES = 481;                                  // 0x01E1
        inline constexpr u16 LAST_BALE_NUMBER_OF_SUBBALES = 482;                                 // 0x01E2
        inline constexpr u16 SETPOINT_ENGINE_SPEED = 483;                                        // 0x01E3
        inline constexpr u16 ACTUAL_ENGINE_SPEED = 484;                                          // 0x01E4
        inline constexpr u16 MINIMUM_ENGINE_SPEED = 485;                                         // 0x01E5
        inline constexpr u16 MAXIMUM_ENGINE_SPEED = 486;                                         // 0x01E6
        inline constexpr u16 DIESEL_EXHAUST_FLUID_TANK_PERCENTAGE_LEVEL = 488;                   // 0x01E8
        inline constexpr u16 MAXIMUM_DIESEL_EXHAUST_FLUID_TANK_CONTENT = 489;                    // 0x01E9
        inline constexpr u16 MAXIMUM_FUEL_TANK_CONTENT = 490;                                    // 0x01EA
        inline constexpr u16 FUEL_PERCENTAGE_LEVEL = 491;                                        // 0x01EB
        inline constexpr u16 TOTAL_ENGINE_HOURS = 492;                                           // 0x01EC
        inline constexpr u16 LIFETIME_ENGINE_HOURS = 493;                                        // 0x01ED
        inline constexpr u16 LAST_EVENT_PARTNER_ID_BYTE_1_4 = 494;                               // 0x01EE
        inline constexpr u16 LAST_EVENT_PARTNER_ID_BYTE_5_8 = 495;                               // 0x01EF
        inline constexpr u16 LAST_EVENT_PARTNER_ID_BYTE_9_12 = 496;                              // 0x01F0
        inline constexpr u16 LAST_EVENT_PARTNER_ID_BYTE_13_16 = 497;                             // 0x01F1
        inline constexpr u16 LAST_EVENT_PARTNER_ID_TYPE = 498;                                   // 0x01F2
        inline constexpr u16 LAST_EVENT_PARTNER_ID_MANUFACTURER_ID_CODE = 499;                   // 0x01F3
        inline constexpr u16 LAST_EVENT_PARTNER_ID_DEVICE_CLASS = 500;                           // 0x01F4
        inline constexpr u16 SETPOINT_ENGINE_TORQUE = 501;                                       // 0x01F5
        inline constexpr u16 ACTUAL_ENGINE_TORQUE = 502;                                         // 0x01F6
        inline constexpr u16 MINIMUM_ENGINE_TORQUE = 503;                                        // 0x01F7
        inline constexpr u16 MAXIMUM_ENGINE_TORQUE = 504;                                        // 0x01F8
        inline constexpr u16 TRAMLINE_CONTROL_LEVEL = 505;                                       // 0x01F9
        inline constexpr u16 SETPOINT_TRAMLINE_CONTROL_LEVEL = 506;                              // 0x01FA
        inline constexpr u16 TRAMLINE_SEQUENCE_NUMBER = 507;                                     // 0x01FB
        inline constexpr u16 UNIQUE_A_B_GUIDANCE_REFERENCE_LINE_ID = 508;                        // 0x01FC
        inline constexpr u16 ACTUAL_TRACK_NUMBER = 509;                                          // 0x01FD
        inline constexpr u16 TRACK_NUMBER_TO_THE_RIGHT = 510;                                    // 0x01FE
        inline constexpr u16 TRACK_NUMBER_TO_THE_LEFT = 511;                                     // 0x01FF
        inline constexpr u16 GUIDANCE_LINE_SWATH_WIDTH = 512;                                    // 0x0200
        inline constexpr u16 GUIDANCE_LINE_DEVIATION = 513;                                      // 0x0201
        inline constexpr u16 GNSS_QUALITY = 514;                                                 // 0x0202
        inline constexpr u16 TRAMLINE_CONTROL_STATE = 515;                                       // 0x0203
        inline constexpr u16 TRAMLINE_OVERDOSING_RATE = 516;                                     // 0x0204
        inline constexpr u16 SETPOINT_TRAMLINE_CONDENSED_WORK_STATE_1_16 = 517;                  // 0x0205
        inline constexpr u16 ACTUAL_TRAMLINE_CONDENSED_WORK_STATE_1_16 = 518;                    // 0x0206
        inline constexpr u16 LAST_BALE_LIFETIME_COUNT = 519;                                     // 0x0207
        inline constexpr u16 ACTUAL_CANOPY_HEIGHT = 520;                                         // 0x0208
        inline constexpr u16 GNSS_INSTALLATION_TYPE = 521;                                       // 0x0209
        inline constexpr u16 TWINE_BALE_TOTAL_COUNT_DEPRECATED = 522;                            // 0x020A
        inline constexpr u16 MESH_BALE_TOTAL_COUNT_DEPRECATED = 523;                             // 0x020B
        inline constexpr u16 LIFETIME_TWINE_BALE_TOTAL_COUNT_DEPRECATED = 524;                   // 0x020C
        inline constexpr u16 LIFETIME_MESH_BALE_TOTAL_COUNT_DEPRECATED = 525;                    // 0x020D
        inline constexpr u16 ACTUAL_COOLING_FLUID_TEMPERATURE = 526;                             // 0x020E
        inline constexpr u16 LAST_BALE_CAPACITY = 528;                                           // 0x0210
        inline constexpr u16 SETPOINT_TILLAGE_DISC_GANG_ANGLE = 529;                             // 0x0211
        inline constexpr u16 ACTUAL_TILLAGE_DISC_GANG_ANGLE = 530;                               // 0x0212
        inline constexpr u16 ACTUAL_APPLIED_PRESERVATIVE_PER_YIELD_MASS = 531;                   // 0x0213
        inline constexpr u16 SETPOINT_APPLIED_PRESERVATIVE_PER_YIELD_MASS = 532;                 // 0x0214
        inline constexpr u16 DEFAULT_APPLIED_PRESERVATIVE_PER_YIELD_MASS = 533;                  // 0x0215
        inline constexpr u16 MINIMUM_APPLIED_PRESERVATIVE_PER_YIELD_MASS = 534;                  // 0x0216
        inline constexpr u16 MAXIMUM_APPLIED_PRESERVATIVE_PER_YIELD_MASS = 535;                  // 0x0217
        inline constexpr u16 TOTAL_APPLIED_PRESERVATIVE = 536;                                   // 0x0218
        inline constexpr u16 LIFETIME_APPLIED_PRESERVATIVE = 537;                                // 0x0219
        inline constexpr u16 AVERAGE_APPLIED_PRESERVATIVE_PER_YIELD_MASS = 538;                  // 0x021A
        inline constexpr u16 ACTUAL_PRESERVATIVE_TANK_VOLUME = 539;                              // 0x021B
        inline constexpr u16 ACTUAL_PRESERVATIVE_TANK_LEVEL = 540;                               // 0x021C
        inline constexpr u16 ACTUAL_PTO_SPEED = 541;                                             // 0x021D
        inline constexpr u16 SETPOINT_PTO_SPEED = 542;                                           // 0x021E
        inline constexpr u16 DEFAULT_PTO_SPEED = 543;                                            // 0x021F
        inline constexpr u16 MINIMUM_PTO_SPEED = 544;                                            // 0x0220
        inline constexpr u16 MAXIMUM_PTO_SPEED = 545;                                            // 0x0221
        inline constexpr u16 LIFETIME_CHOPPING_ENGAGEMENT_TOTAL_TIME = 546;                      // 0x0222
        inline constexpr u16 SETPOINT_BALE_COMPRESSION_PLUNGER_LOAD_N = 547;                     // 0x0223
        inline constexpr u16 ACTUAL_BALE_COMPRESSION_PLUNGER_LOAD_N = 548;                       // 0x0224
        inline constexpr u16 LAST_BALE_AVERAGE_BALE_COMPRESSION_PLUNGER_LOAD_N = 549;            // 0x0225
        inline constexpr u16 GROUND_COVER = 550;                                                 // 0x0226
        inline constexpr u16 ACTUAL_PTO_TORQUE = 551;                                            // 0x0227
        inline constexpr u16 SETPOINT_PTO_TORQUE = 552;                                          // 0x0228
        inline constexpr u16 DEFAULT_PTO_TORQUE = 553;                                           // 0x0229
        inline constexpr u16 MINIMUM_PTO_TORQUE = 554;                                           // 0x022A
        inline constexpr u16 MAXIMUM_PTO_TORQUE = 555;                                           // 0x022B
        inline constexpr u16 PRESENT_WEATHER_CONDITIONS = 556;                                   // 0x022C
        inline constexpr u16 SETPOINT_ELECTRICAL_CURRENT = 557;                                  // 0x022D
        inline constexpr u16 ACTUAL_ELECTRICAL_CURRENT = 558;                                    // 0x022E
        inline constexpr u16 MINIMUM_ELECTRICAL_CURRENT = 559;                                   // 0x022F
        inline constexpr u16 MAXIMUM_ELECTRICAL_CURRENT = 560;                                   // 0x0230
        inline constexpr u16 DEFAULT_ELECTRICAL_CURRENT = 561;                                   // 0x0231
        inline constexpr u16 SETPOINT_VOLTAGE = 562;                                             // 0x0232
        inline constexpr u16 DEFAULT_VOLTAGE = 563;                                              // 0x0233
        inline constexpr u16 ACTUAL_VOLTAGE = 564;                                               // 0x0234
        inline constexpr u16 MINIMUM_VOLTAGE = 565;                                              // 0x0235
        inline constexpr u16 MAXIMUM_VOLTAGE = 566;                                              // 0x0236
        inline constexpr u16 ACTUAL_ELECTRICAL_RESISTANCE = 567;                                 // 0x0237
        inline constexpr u16 SETPOINT_ELECTRICAL_POWER = 568;                                    // 0x0238
        inline constexpr u16 ACTUAL_ELECTRICAL_POWER = 569;                                      // 0x0239
        inline constexpr u16 DEFAULT_ELECTRICAL_POWER = 570;                                     // 0x023A
        inline constexpr u16 MAXIMUM_ELECTRICAL_POWER = 571;                                     // 0x023B
        inline constexpr u16 MINIMUM_ELECTRICAL_POWER = 572;                                     // 0x023C
        inline constexpr u16 TOTAL_ELECTRICAL_ENERGY = 573;                                      // 0x023D
        inline constexpr u16 SETPOINT_ELECTRICAL_ENERGY_PER_AREA_APPLICATION_RATE = 574;         // 0x023E
        inline constexpr u16 ACTUAL_ELECTRICAL_ENERGY_PER_AREA_APPLICATION_RATE = 575;           // 0x023F
        inline constexpr u16 MAXIMUM_ELECTRICAL_ENERGY_PER_AREA_APPLICATION_RATE = 576;          // 0x0240
        inline constexpr u16 MINIMUM_ELECTRICAL_ENERGY_PER_AREA_APPLICATION_RATE = 577;          // 0x0241
        inline constexpr u16 SETPOINT_TEMPERATURE = 578;                                         // 0x0242
        inline constexpr u16 ACTUAL_TEMPERATURE = 579;                                           // 0x0243
        inline constexpr u16 MINIMUM_TEMPERATURE = 580;                                          // 0x0244
        inline constexpr u16 MAXIMUM_TEMPERATURE = 581;                                          // 0x0245
        inline constexpr u16 DEFAULT_TEMPERATURE = 582;                                          // 0x0246
        inline constexpr u16 SETPOINT_FREQUENCY = 583;                                           // 0x0247
        inline constexpr u16 ACTUAL_FREQUENCY = 584;                                             // 0x0248
        inline constexpr u16 MINIMUM_FREQUENCY = 585;                                            // 0x0249
        inline constexpr u16 MAXIMUM_FREQUENCY = 586;                                            // 0x024A
        inline constexpr u16 PREVIOUS_RAINFALL = 587;                                            // 0x024B
        inline constexpr u16 SETPOINT_VOLUME_PER_AREA_APPLICATION_RATE_588 = 588;                // 0x024C
        inline constexpr u16 ACTUAL_VOLUME_PER_AREA_APPLICATION_RATE_589 = 589;                  // 0x024D
        inline constexpr u16 MINIMUM_VOLUME_PER_AREA_APPLICATION_RATE_590 = 590;                 // 0x024E
        inline constexpr u16 MAXIMUM_VOLUME_PER_AREA_APPLICATION_RATE_591 = 591;                 // 0x024F
        inline constexpr u16 DEFAULT_VOLUME_PER_AREA_APPLICATION_RATE_592 = 592;                 // 0x0250
        inline constexpr u16 TRACTION_TYPE = 593;                                                // 0x0251
        inline constexpr u16 STEERING_TYPE = 594;                                                // 0x0252
        inline constexpr u16 MACHINE_MODE = 595;                                                 // 0x0253
        inline constexpr u16 CARGO_AREA_COVER_STATE = 596;                                       // 0x0254
        inline constexpr u16 TOTAL_DISTANCE = 597;                                               // 0x0255
        inline constexpr u16 LIFETIME_TOTAL_DISTANCE = 598;                                      // 0x0256
        inline constexpr u16 TOTAL_DISTANCE_FIELD = 599;                                         // 0x0257
        inline constexpr u16 LIFETIME_TOTAL_DISTANCE_FIELD = 600;                                // 0x0258
        inline constexpr u16 TOTAL_DISTANCE_STREET = 601;                                        // 0x0259
        inline constexpr u16 LIFETIME_TOTAL_DISTANCE_STREET = 602;                               // 0x025A
        inline constexpr u16 ACTUAL_TRAMLINE_CONDENSED_WORK_STATE_17_32 = 603;                   // 0x025B
        inline constexpr u16 ACTUAL_TRAMLINE_CONDENSED_WORK_STATE_33_48 = 604;                   // 0x025C
        inline constexpr u16 ACTUAL_TRAMLINE_CONDENSED_WORK_STATE_49_64 = 605;                   // 0x025D
        inline constexpr u16 ACTUAL_TRAMLINE_CONDENSED_WORK_STATE_65_80 = 606;                   // 0x025E
        inline constexpr u16 ACTUAL_TRAMLINE_CONDENSED_WORK_STATE_81_96 = 607;                   // 0x025F
        inline constexpr u16 ACTUAL_TRAMLINE_CONDENSED_WORK_STATE_97_112 = 608;                  // 0x0260
        inline constexpr u16 ACTUAL_TRAMLINE_CONDENSED_WORK_STATE_113_128 = 609;                 // 0x0261
        inline constexpr u16 ACTUAL_TRAMLINE_CONDENSED_WORK_STATE_129_144 = 610;                 // 0x0262
        inline constexpr u16 ACTUAL_TRAMLINE_CONDENSED_WORK_STATE_145_160 = 611;                 // 0x0263
        inline constexpr u16 ACTUAL_TRAMLINE_CONDENSED_WORK_STATE_161_176 = 612;                 // 0x0264
        inline constexpr u16 ACTUAL_TRAMLINE_CONDENSED_WORK_STATE_177_192 = 613;                 // 0x0265
        inline constexpr u16 ACTUAL_TRAMLINE_CONDENSED_WORK_STATE_193_208 = 614;                 // 0x0266
        inline constexpr u16 ACTUAL_TRAMLINE_CONDENSED_WORK_STATE_209_224 = 615;                 // 0x0267
        inline constexpr u16 ACTUAL_TRAMLINE_CONDENSED_WORK_STATE_225_240 = 616;                 // 0x0268
        inline constexpr u16 ACTUAL_TRAMLINE_CONDENSED_WORK_STATE_241_256 = 617;                 // 0x0269
        inline constexpr u16 SETPOINT_TRAMLINE_CONDENSED_WORK_STATE_17_32 = 618;                 // 0x026A
        inline constexpr u16 SETPOINT_TRAMLINE_CONDENSED_WORK_STATE_33_48 = 619;                 // 0x026B
        inline constexpr u16 SETPOINT_TRAMLINE_CONDENSED_WORK_STATE_49_64 = 620;                 // 0x026C
        inline constexpr u16 SETPOINT_TRAMLINE_CONDENSED_WORK_STATE_65_80 = 621;                 // 0x026D
        inline constexpr u16 SETPOINT_TRAMLINE_CONDENSED_WORK_STATE_81_96 = 622;                 // 0x026E
        inline constexpr u16 SETPOINT_TRAMLINE_CONDENSED_WORK_STATE_97_112 = 623;                // 0x026F
        inline constexpr u16 SETPOINT_TRAMLINE_CONDENSED_WORK_STATE_113_128 = 624;               // 0x0270
        inline constexpr u16 SETPOINT_TRAMLINE_CONDENSED_WORK_STATE_129_144 = 625;               // 0x0271
        inline constexpr u16 SETPOINT_TRAMLINE_CONDENSED_WORK_STATE_145_160 = 626;               // 0x0272
        inline constexpr u16 SETPOINT_TRAMLINE_CONDENSED_WORK_STATE_161_176 = 627;               // 0x0273
        inline constexpr u16 SETPOINT_TRAMLINE_CONDENSED_WORK_STATE_177_192 = 628;               // 0x0274
        inline constexpr u16 SETPOINT_TRAMLINE_CONDENSED_WORK_STATE_193_208 = 629;               // 0x0275
        inline constexpr u16 SETPOINT_TRAMLINE_CONDENSED_WORK_STATE_209_224 = 630;               // 0x0276
        inline constexpr u16 SETPOINT_TRAMLINE_CONDENSED_WORK_STATE_225_240 = 631;               // 0x0277
        inline constexpr u16 SETPOINT_TRAMLINE_CONDENSED_WORK_STATE_241_256 = 632;               // 0x0278
        inline constexpr u16 SETPOINT_VOLUME_PER_DISTANCE_APPLICATION_RATE = 633;                // 0x0279
        inline constexpr u16 ACTUAL_VOLUME_PER_DISTANCE_APPLICATION_RATE = 634;                  // 0x027A
        inline constexpr u16 DEFAULT_VOLUME_PER_DISTANCE_APPLICATION_RATE = 635;                 // 0x027B
        inline constexpr u16 MINIMUM_VOLUME_PER_DISTANCE_APPLICATION_RATE = 636;                 // 0x027C
        inline constexpr u16 MAXIMUM_VOLUME_PER_DISTANCE_APPLICATION_RATE = 637;                 // 0x027D
        inline constexpr u16 SETPOINT_TIRE_PRESSURE = 638;                                       // 0x027E
        inline constexpr u16 ACTUAL_TIRE_PRESSURE = 639;                                         // 0x027F
        inline constexpr u16 DEFAULT_TIRE_PRESSURE = 640;                                        // 0x0280
        inline constexpr u16 MINIMUM_TIRE_PRESSURE = 641;                                        // 0x0281
        inline constexpr u16 MAXIMUM_TIRE_PRESSURE = 642;                                        // 0x0282
        inline constexpr u16 ACTUAL_TIRE_TEMPERATURE = 643;                                      // 0x0283
        inline constexpr u16 BINDING_METHOD = 644;                                               // 0x0284
        inline constexpr u16 LAST_BALE_NUMBER_OF_KNIVES = 645;                                   // 0x0285
        inline constexpr u16 LAST_BALE_BINDING_TWINE_CONSUMPTION = 646;                          // 0x0286
        inline constexpr u16 LAST_BALE_BINDING_MESH_CONSUMPTION = 647;                           // 0x0287
        inline constexpr u16 LAST_BALE_BINDING_FILM_CONSUMPTION = 648;                           // 0x0288
        inline constexpr u16 LAST_BALE_BINDING_FILM_STRETCHING = 649;                            // 0x0289
        inline constexpr u16 LAST_BALE_WRAPPING_FILM_WIDTH = 650;                                // 0x028A
        inline constexpr u16 LAST_BALE_WRAPPING_FILM_CONSUMPTION = 651;                          // 0x028B
        inline constexpr u16 LAST_BALE_WRAPPING_FILM_STRETCHING = 652;                           // 0x028C
        inline constexpr u16 LAST_BALE_WRAPPING_FILM_OVERLAP_PERCENTAGE = 653;                   // 0x028D
        inline constexpr u16 LAST_BALE_WRAPPING_FILM_LAYERS = 654;                               // 0x028E
        inline constexpr u16 ELECTRICAL_APPARENT_SOIL_CONDUCTIVITY = 655;                        // 0x028F
        inline constexpr u16 SC_ACTUAL_TURN_ON_TIME = 656;                                       // 0x0290
        inline constexpr u16 SC_ACTUAL_TURN_OFF_TIME = 657;                                      // 0x0291
        inline constexpr u16 ACTUAL_CO2_EQUIVALENT_SPECIFIED_AS_MASS_PER_AREA = 658;             // 0x0292
        inline constexpr u16 ACTUAL_CO2_EQUIVALENT_SPECIFIED_AS_MASS_PER_TIME = 659;             // 0x0293
        inline constexpr u16 ACTUAL_CO2_EQUIVALENT_SPECIFIED_AS_MASS_PER_MASS = 660;             // 0x0294
        inline constexpr u16 ACTUAL_CO2_EQUIVALENT_SPECIFIED_AS_MASS_PER_YIELD = 661;            // 0x0295
        inline constexpr u16 ACTUAL_CO2_EQUIVALENT_SPECIFIED_AS_MASS_PER_VOLUME = 662;           // 0x0296
        inline constexpr u16 ACTUAL_CO2_EQUIVALENT_SPECIFIED_AS_MASS_PER_COUNT = 663;            // 0x0297
        inline constexpr u16 TOTAL_CO2_EQUIVALENT = 664;                                         // 0x0298
        inline constexpr u16 LIFETIME_TOTAL_CO2_EQUIVALENT = 665;                                // 0x0299
        inline constexpr u16 WORKING_DIRECTION = 666;                                            // 0x029A
        inline constexpr u16 DISTANCE_BETWEEN_GUIDANCE_TRACK_NUMBER_0R_AND_1 = 667;              // 0x029B
        inline constexpr u16 DISTANCE_BETWEEN_GUIDANCE_TRACK_NUMBER_0R_AND_0L = 668;             // 0x029C
        inline constexpr u16 BOUT_TRACK_NUMBER_SHIFT = 669;                                      // 0x029D
        inline constexpr u16 TRAMLINE_PRIMARY_WORKING_WIDTH = 670;                               // 0x029E
        inline constexpr u16 TRAMLINE_PRIMARY_TIRE_WIDTH = 671;                                  // 0x029F
        inline constexpr u16 TRAMLINE_PRIMARY_WHEEL_DISTANCE = 672;                              // 0x02A0
        inline constexpr u16 TRAMLINE_SECONDARY_WORKING_WIDTH = 673;                             // 0x02A1
        inline constexpr u16 TRAMLINE_SECONDARY_TIRE_WIDTH = 674;                                // 0x02A2
        inline constexpr u16 TRAMLINE_SECONDARY_WHEEL_DISTANCE = 675;                            // 0x02A3
        inline constexpr u16 LAST_BALE_BINDING_MESH_LAYERS = 676;                                // 0x02A4
        inline constexpr u16 LAST_BALE_BINDING_FILM_LAYERS = 677;                                // 0x02A5
        inline constexpr u16 LAST_BALE_BINDING_TWINE_LAYERS = 678;                               // 0x02A6
        inline constexpr u16 CROP_CONTAMINATION_TOTAL_MASS = 679;                                // 0x02A7
        inline constexpr u16 CROP_CONTAMINATION_LIFETIME_TOTAL_MASS = 680;                       // 0x02A8
        inline constexpr u16 FILM_BALE_TOTAL_COUNT = 681;                                        // 0x02A9
        inline constexpr u16 MESH_BALE_TOTAL_COUNT = 682;                                        // 0x02AA
        inline constexpr u16 TWINE_BALE_TOTAL_COUNT = 683;                                       // 0x02AB
        inline constexpr u16 WRAPPING_FILM_BALE_TOTAL_COUNT = 684;                               // 0x02AC
        inline constexpr u16 LIFETIME_FILM_BALE_TOTAL_COUNT = 685;                               // 0x02AD
        inline constexpr u16 LIFETIME_MESH_BALE_TOTAL_COUNT = 686;                               // 0x02AE
        inline constexpr u16 LIFETIME_TWINE_BALE_TOTAL_COUNT = 687;                              // 0x02AF
        inline constexpr u16 LIFETIME_WRAPPING_FILM_BALE_TOTAL_COUNT = 688;                      // 0x02B0
        inline constexpr u16 EFFECTIVE_TOTAL_ELECTRICAL_BATTERY_ENERGY_CONSUMPTION = 689;        // 0x02B1
        inline constexpr u16 INEFFECTIVE_TOTAL_ELECTRICAL_BATTERY_ENERGY_CONSUMPTION = 690;      // 0x02B2
        inline constexpr u16 INSTANTANEOUS_ELECTRICAL_BATTERY_ENERGY_CONSUMPTION_PER_TIME = 691; // 0x02B3
        inline constexpr u16 INSTANTANEOUS_ELECTRICAL_BATTERY_ENERGY_CONSUMPTION_PER_AREA = 692; // 0x02B4
        inline constexpr u16 LIFETIME_TOTAL_LOADING_TIME = 693;                                  // 0x02B5
        inline constexpr u16 LIFETIME_TOTAL_UNLOADING_TIME = 694;                                // 0x02B6
        inline constexpr u16 MESH_TOTAL_USED = 695;                                              // 0x02B7
        inline constexpr u16 FILM_TOTAL_USED = 696;                                              // 0x02B8
        inline constexpr u16 TWINE_TOTAL_USED = 697;                                             // 0x02B9
        inline constexpr u16 AVERAGE_MASS_PER_BALE = 698;                                        // 0x02BA
        inline constexpr u16 AVERAGE_MASS_PER_LOAD = 699;                                        // 0x02BB
        inline constexpr u16 NUTRIENT_ANALYSIS_METHOD = 700;                                     // 0x02BC
        inline constexpr u16 ACTUAL_STARCH_CONTENT = 701;                                        // 0x02BD
        inline constexpr u16 AVERAGE_STARCH_CONTENT = 702;                                       // 0x02BE
        inline constexpr u16 ACTUAL_ASH_CONTENT = 703;                                           // 0x02BF
        inline constexpr u16 AVERAGE_ASH_CONTENT = 704;                                          // 0x02C0
        inline constexpr u16 ACTUAL_SUGAR_CONTENT = 705;                                         // 0x02C1
        inline constexpr u16 AVERAGE_SUGAR_CONTENT = 706;                                        // 0x02C2
        inline constexpr u16 ACTUAL_LIPIDS_CONTENT = 707;                                        // 0x02C3
        inline constexpr u16 AVERAGE_LIPIDS_CONTENT = 708;                                       // 0x02C4
        inline constexpr u16 ACTUAL_CRUDE_FAT_CONTENT = 709;                                     // 0x02C5
        inline constexpr u16 AVERAGE_CRUDE_FAT_CONTENT = 710;                                    // 0x02C6
        inline constexpr u16 ACTUAL_CRUDE_FIBER_CONTENT = 711;                                   // 0x02C7
        inline constexpr u16 AVERAGE_CRUDE_FIBER_CONTENT = 712;                                  // 0x02C8
        inline constexpr u16 ACTUAL_NITROGEN_TKN_CONTENT = 713;                                  // 0x02C9
        inline constexpr u16 AVERAGE_NITROGEN_TKN_CONTENT = 714;                                 // 0x02CA
        inline constexpr u16 ACTUAL_PHOSPHATES_CONTENT = 715;                                    // 0x02CB
        inline constexpr u16 AVERAGE_PHOSPHATES_CONTENT = 716;                                   // 0x02CC
        inline constexpr u16 ACTUAL_POTASSIUM_CONTENT = 717;                                     // 0x02CD
        inline constexpr u16 AVERAGE_POTASSIUM_CONTENT = 718;                                    // 0x02CE
        inline constexpr u16 ACTUAL_ADL_CONTENT = 719;                                           // 0x02CF
        inline constexpr u16 AVERAGE_ADL_CONTENT = 720;                                          // 0x02D0
        inline constexpr u16 ACTUAL_ADF_CONTENT = 721;                                           // 0x02D1
        inline constexpr u16 AVERAGE_ADF_CONTENT = 722;                                          // 0x02D2
        inline constexpr u16 ACTUAL_NDF_CONTENT = 723;                                           // 0x02D3
        inline constexpr u16 AVERAGE_NDF_CONTENT = 724;                                          // 0x02D4
        inline constexpr u16 ACTUAL_UNDFOM240_CONTENT = 725;                                     // 0x02D5
        inline constexpr u16 AVERAGE_UNDFOM240_CONTENT = 726;                                    // 0x02D6
        inline constexpr u16 ACTUAL_BIOMETHANATION_PRODUCTION_POTENTIAL = 727;                   // 0x02D7
        inline constexpr u16 AVERAGE_BIOMETHANATION_PRODUCTION_POTENTIAL = 728;                  // 0x02D8
        inline constexpr u16 MAXIMUM_DROPLET_SIZE = 32768;                                       // 0x8000
        inline constexpr u16 MAXIMUM_CROP_GRADE_DIAMETER = 32769;                                // 0x8001
        inline constexpr u16 MAXIMUM_CROP_GRADE_LENGTH = 32770;                                  // 0x8002
        inline constexpr u16 MAXIMUM_CROP_CONTAMINATION_MASS_PER_AREA = 32771;                   // 0x8003
        inline constexpr u16 MAXIMUM_CROP_CONTAMINATION_MASS_PER_TIME = 32772;                   // 0x8004
        inline constexpr u16 MAXIMUM_CROP_CONDITIONING_INTENSITY = 32773;                        // 0x8005
        inline constexpr u16 MINIMUM_DROPLET_SIZE = 36864;                                       // 0x9000
        inline constexpr u16 MINIMUM_CROP_GRADE_DIAMETER = 36865;                                // 0x9001
        inline constexpr u16 MINIMUM_CROP_GRADE_LENGTH = 36866;                                  // 0x9002
        inline constexpr u16 MINIMUM_CROP_CONTAMINATION_MASS_PER_AREA = 36867;                   // 0x9003
        inline constexpr u16 MINIMUM_CROP_CONTAMINATION_MASS_PER_TIME = 36868;                   // 0x9004
        inline constexpr u16 MINIMUM_CROP_CONDITIONING_INTENSITY = 36869;                        // 0x9005
        inline constexpr u16 DEFAULT_DROPLET_SIZE = 40960;                                       // 0xA000
        inline constexpr u16 DEFAULT_CROP_GRADE_DIAMETER = 40961;                                // 0xA001
        inline constexpr u16 DEFAULT_CROP_GRADE_LENGTH = 40962;                                  // 0xA002
        inline constexpr u16 DEFAULT_CROP_CONTAMINATION_MASS_PER_AREA = 40963;                   // 0xA003
        inline constexpr u16 DEFAULT_CROP_CONTAMINATION_MASS_PER_TIME = 40964;                   // 0xA004
        inline constexpr u16 DEFAULT_CROP_CONDITIONING_INTENSITY = 40965;                        // 0xA005
        inline constexpr u16 ACTUAL_DROPLET_SIZE = 45056;                                        // 0xB000
        inline constexpr u16 ACTUAL_CROP_GRADE_DIAMETER = 45057;                                 // 0xB001
        inline constexpr u16 ACTUAL_CROP_GRADE_LENGTH = 45058;                                   // 0xB002
        inline constexpr u16 ACTUAL_CROP_CONTAMINATION_MASS_PER_AREA = 45059;                    // 0xB003
        inline constexpr u16 ACTUAL_CROP_CONTAMINATION_MASS_PER_TIME = 45060;                    // 0xB004
        inline constexpr u16 ACTUAL_CROP_CONDITIONING_INTENSITY = 45061;                         // 0xB005
        inline constexpr u16 SETPOINT_DROPLET_SIZE = 49152;                                      // 0xC000
        inline constexpr u16 SETPOINT_CROP_GRADE_DIAMETER = 49153;                               // 0xC001
        inline constexpr u16 SETPOINT_CROP_GRADE_LENGTH = 49154;                                 // 0xC002
        inline constexpr u16 SETPOINT_CROP_CONTAMINATION_MASS_PER_AREA = 49155;                  // 0xC003
        inline constexpr u16 SETPOINT_CROP_CONTAMINATION_MASS_PER_TIME = 49156;                  // 0xC004
        inline constexpr u16 SETPOINT_CROP_CONDITIONING_INTENSITY = 49157;                       // 0xC005
        inline constexpr u16 PGN_BASED_DATA = 57342;                                             // 0xDFFE
        inline constexpr u16 REQUEST_DEFAULT_PROCESS_DATA = 57343;                               // 0xDFFF
        inline constexpr u16 N65534_PROPRIETARY_DDI_RANGE = 57344;                               // 0xE000
        inline constexpr u16 RESERVED = 65535;                                                   // 0xFFFF

        // ─── Convenience aliases ────────────────────────────────────────────────
        inline constexpr u16 SETPOINT_VOLUME_PER_AREA = SETPOINT_VOLUME_PER_AREA_APPLICATION_RATE;
        inline constexpr u16 ACTUAL_VOLUME_PER_AREA = ACTUAL_VOLUME_PER_AREA_APPLICATION_RATE;
        inline constexpr u16 EFFECTIVE_TOTAL_AREA = TOTAL_AREA;
        inline constexpr u16 SETPOINT_SECTION_CONTROL_STATE = SECTION_CONTROL_STATE;
        inline constexpr u16 WORKING_WIDTH = ACTUAL_WORKING_WIDTH;

    } // namespace ddi

    // ─── Full DDI Database (760 entries) ─────────────────────────────────
    inline constexpr DDIDefinition DDI_DATABASE[] = {
        {0, "Internal Data Base DDI", "", 1.0, 0, 2147483647},
        {1, "Setpoint Volume Per Area Application Rate", "mm3/m2", 0.01, 0, 2147483647},
        {2, "Actual Volume Per Area Application Rate", "mm3/m2", 0.01, 0, 2147483647},
        {3, "Default Volume Per Area Application Rate", "mm3/m2", 0.01, 0, 2147483647},
        {4, "Minimum Volume Per Area Application Rate", "mm3/m2", 0.01, 0, 2147483647},
        {5, "Maximum Volume Per Area Application Rate", "mm3/m2", 0.01, 0, 2147483647},
        {6, "Setpoint Mass Per Area Application Rate", "mg/m2", 1.0, 0, 2147483647},
        {7, "Actual Mass Per Area Application Rate", "mg/m2", 1.0, 0, 2147483647},
        {8, "Default Mass Per Area Application Rate", "mg/m2", 1.0, 0, 2147483647},
        {9, "Minimum Mass Per Area Application Rate", "mg/m2", 1.0, 0, 2147483647},
        {10, "Maximum Mass Per Area Application Rate", "mg/m2", 1.0, 0, 2147483647},
        {11, "Setpoint Count Per Area Application Rate", "/m2", 0.001, 0, 2147483647},
        {12, "Actual Count Per Area Application Rate", "/m2", 0.001, 0, 2147483647},
        {13, "Default Count Per Area Application Rate", "/m2", 0.001, 0, 2147483647},
        {14, "Minimum Count Per Area Application Rate", "/m2", 0.001, 0, 2147483647},
        {15, "Maximum Count Per Area Application Rate", "/m2", 0.001, 0, 2147483647},
        {16, "Setpoint Spacing Application Rate", "mm", 1.0, 0, 2147483647},
        {17, "Actual Spacing Application Rate", "mm", 1.0, 0, 2147483647},
        {18, "Default Spacing Application Rate", "mm", 1.0, 0, 2147483647},
        {19, "Minimum Spacing Application Rate", "mm", 1.0, 0, 2147483647},
        {20, "Maximum Spacing Application Rate", "mm", 1.0, 0, 2147483647},
        {21, "Setpoint Volume Per Volume Application Rate", "mm3/m3", 1.0, 0, 2147483647},
        {22, "Actual Volume Per Volume Application Rate", "mm3/m3", 1.0, 0, 2147483647},
        {23, "Default Volume Per Volume Application Rate", "mm3/m3", 1.0, 0, 2147483647},
        {24, "Minimum Volume Per Volume Application Rate", "mm3/m3", 1.0, 0, 2147483647},
        {25, "Maximum Volume Per Volume Application Rate", "mm3/m3", 1.0, 0, 2147483647},
        {26, "Setpoint Mass Per Mass Application Rate", "mg/kg", 1.0, 0, 2147483647},
        {27, "Actual Mass Per Mass Application Rate", "mg/kg", 1.0, 0, 2147483647},
        {28, "Default Mass Per Mass Application Rate", "mg/kg", 1.0, 0, 2147483647},
        {29, "Minimum Mass Per Mass Application Rate", "mg/kg", 1.0, 0, 2147483647},
        {30, "MaximumMass Per Mass Application Rate", "mg/kg", 1.0, 0, 2147483647},
        {31, "Setpoint Volume Per Mass Application Rate", "mm3/kg", 1.0, 0, 2147483647},
        {32, "Actual Volume Per Mass Application Rate", "mm3/kg", 1.0, 0, 2147483647},
        {33, "Default Volume Per Mass Application Rate", "mm3/kg", 1.0, 0, 2147483647},
        {34, "Minimum Volume Per Mass Application Rate", "mm3/kg", 1.0, 0, 2147483647},
        {35, "Maximum Volume Per Mass Application Rate", "mm3/kg", 1.0, 0, 2147483647},
        {36, "Setpoint Volume Per Time Application Rate", "mm3/s", 1.0, 0, 2147483647},
        {37, "Actual Volume Per Time Application Rate", "mm3/s", 1.0, 0, 2147483647},
        {38, "Default Volume Per Time Application Rate", "mm3/s", 1.0, 0, 2147483647},
        {39, "Minimum Volume Per Time Application Rate", "mm3/s", 1.0, 0, 2147483647},
        {40, "Maximum Volume Per Time Application Rate", "mm3/s", 1.0, 0, 2147483647},
        {41, "Setpoint Mass Per Time Application Rate", "mg/s", 1.0, 0, 2147483647},
        {42, "Actual Mass Per Time Application Rate", "mg/s", 1.0, 0, 2147483647},
        {43, "Default Mass Per Time Application Rate", "mg/s", 1.0, 0, 2147483647},
        {44, "Minimum Mass Per Time Application Rate", "mg/s", 1.0, 0, 2147483647},
        {45, "Maximum Mass Per Time Application Rate", "mg/s", 1.0, 0, 2147483647},
        {46, "Setpoint Count Per Time Application Rate", "/s", 0.001, 0, 2147483647},
        {47, "Actual Count Per Time Application Rate", "/s", 0.001, 0, 2147483647},
        {48, "Default Count Per Time Application Rate", "/s", 0.001, 0, 2147483647},
        {49, "Minimum Count Per Time Application Rate", "/s", 0.001, 0, 2147483647},
        {50, "Maximum Count Per Time Application Rate", "/s", 0.001, 0, 2147483647},
        {51, "Setpoint Tillage Depth", "mm", 1.0, -2147483648, 2147483647},
        {52, "Actual Tillage Depth", "mm", 1.0, -2147483648, 2147483647},
        {53, "Default Tillage Depth", "mm", 1.0, -2147483648, 2147483647},
        {54, "Minimum Tillage Depth", "mm", 1.0, -2147483648, 2147483647},
        {55, "Maximum Tillage Depth", "mm", 1.0, -2147483648, 2147483647},
        {56, "Setpoint Seeding Depth", "mm", 1.0, 0, 2147483647},
        {57, "Actual Seeding Depth", "mm", 1.0, 0, 2147483647},
        {58, "Default Seeding Depth", "mm", 1.0, 0, 2147483647},
        {59, "Minimum Seeding Depth", "mm", 1.0, 0, 2147483647},
        {60, "Maximum Seeding Depth", "mm", 1.0, 0, 2147483647},
        {61, "Setpoint Working Height", "mm", 1.0, 0, 2147483647},
        {62, "Actual Working Height", "mm", 1.0, 0, 2147483647},
        {63, "Default Working Height", "mm", 1.0, 0, 2147483647},
        {64, "Minimum Working Height", "mm", 1.0, 0, 2147483647},
        {65, "Maximum Working Height", "mm", 1.0, 0, 2147483647},
        {66, "Setpoint Working Width", "mm", 1.0, 0, 2147483647},
        {67, "Actual Working Width", "mm", 1.0, 0, 2147483647},
        {68, "Default Working Width", "mm", 1.0, 0, 2147483647},
        {69, "Minimum Working Width", "mm", 1.0, 0, 2147483647},
        {70, "Maximum Working Width", "mm", 1.0, 0, 2147483647},
        {71, "Setpoint Volume Content", "ml", 1.0, 0, 2147483647},
        {72, "Actual Volume Content", "ml", 1.0, 0, 2147483647},
        {73, "Maximum Volume Content", "ml", 1.0, 0, 2147483647},
        {74, "Setpoint Mass Content", "g", 1.0, 0, 2147483647},
        {75, "Actual Mass Content", "g", 1.0, -2147483648, 2147483647},
        {76, "Maximum Mass Content", "g", 1.0, 0, 2147483647},
        {77, "Setpoint Count Content", "#", 1.0, 0, 2147483647},
        {78, "Actual Count Content", "#", 1.0, 0, 2147483647},
        {79, "Maximum Count Content", "#", 1.0, 0, 2147483647},
        {80, "Application Total Volume", "L", 1.0, 0, 2147483647},
        {81, "Application Total Mass", "kg", 1.0, 0, 2147483647},
        {82, "Application Total Count", "#", 1.0, 0, 2147483647},
        {83, "Volume Per Area Yield", "ml/m2", 1.0, 0, 2147483647},
        {84, "Mass Per Area Yield", "mg/m2", 1.0, 0, 2147483647},
        {85, "Count Per Area Yield", "/m2", 0.001, 0, 2147483647},
        {86, "Volume Per Time Yield", "ml/s", 1.0, 0, 2147483647},
        {87, "Mass Per Time Yield", "mg/s", 1.0, 0, 2147483647},
        {88, "Count Per Time Yield", "/s", 0.001, 0, 2147483647},
        {89, "Yield Total Volume", "L", 1.0, 0, 2147483647},
        {90, "Yield Total Mass", "kg", 1.0, 0, 2147483647},
        {91, "Yield Total Count", "#", 1.0, 0, 2147483647},
        {92, "Volume Per Area Crop Loss", "ml/m2", 1.0, 0, 2147483647},
        {93, "Mass Per Area Crop Loss", "mg/m2", 1.0, 0, 2147483647},
        {94, "Count Per Area Crop Loss", "/m2", 0.001, 0, 2147483647},
        {95, "Volume Per Time Crop Loss", "ml/s", 1.0, 0, 2147483647},
        {96, "Mass Per Time Crop Loss", "mg/s", 1.0, 0, 2147483647},
        {97, "Count Per Time Crop Loss", "/s", 0.001, 0, 2147483647},
        {98, "Percentage Crop Loss", "ppm", 1.0, 0, 2147483647},
        {99, "Crop Moisture", "ppm", 1.0, 0, 2147483647},
        {100, "Crop Contamination", "ppm", 1.0, 0, 2147483647},
        {101, "Setpoint Bale Width", "mm", 1.0, 0, 2147483647},
        {102, "Actual Bale Width", "mm", 1.0, 0, 2147483647},
        {103, "Default Bale Width", "mm", 1.0, 0, 2147483647},
        {104, "Minimum Bale Width", "mm", 1.0, 0, 2147483647},
        {105, "Maximum Bale Width", "mm", 1.0, 0, 2147483647},
        {106, "Setpoint Bale Height", "mm", 1.0, 0, 2147483647},
        {107, "Actual Bale Height", "mm", 1.0, 0, 2147483647},
        {108, "Default Bale Height", "mm", 1.0, 0, 2147483647},
        {109, "Minimum Bale Height", "mm", 1.0, 0, 2147483647},
        {110, "Maximum Bale Height", "mm", 1.0, 0, 2147483647},
        {111, "Setpoint Bale Size", "mm", 1.0, 0, 2147483647},
        {112, "Actual Bale Size", "mm", 1.0, 0, 2147483647},
        {113, "Default Bale Size", "mm", 1.0, 0, 2147483647},
        {114, "Minimum Bale Size", "mm", 1.0, 0, 2147483647},
        {115, "Maximum Bale Size", "mm", 1.0, 0, 2147483647},
        {116, "Total Area", "m2", 1.0, 0, 2147483647},
        {117, "Effective Total Distance", "mm", 1.0, 0, 2147483647},
        {118, "Ineffective Total Distance", "mm", 1.0, 0, 2147483647},
        {119, "Effective Total Time", "s", 1.0, 0, 2147483647},
        {120, "Ineffective Total Time", "s", 1.0, 0, 2147483647},
        {121, "Product Density Mass Per Volume", "mg/l", 1.0, 0, 2147483647},
        {122, "Product Density Mass PerCount", "mg/1000", 1.0, 0, 2147483647},
        {123, "Product Density Volume Per Count", "ml/1000", 1.0, 0, 2147483647},
        {124, "Auxiliary Valve Scaling Extend", "%", 0.1, -2147483648, 2147483647},
        {125, "Auxiliary Valve Scaling Retract", "%", 0.1, -2147483648, 2147483647},
        {126, "Auxiliary Valve Ramp Extend Up", "ms", 1.0, -2147483648, 2147483647},
        {127, "Auxiliary Valve Ramp Extend Down", "ms", 1.0, -2147483648, 2147483647},
        {128, "Auxiliary Valve Ramp Retract Up", "ms", 1.0, -2147483648, 2147483647},
        {129, "Auxiliary Valve Ramp Retract Down", "ms", 1.0, -2147483648, 2147483647},
        {130, "Auxiliary Valve Float Threshold", "%", 0.1, -2147483648, 2147483647},
        {131, "Auxiliary Valve Progressivity Extend", "n.a. -", 1.0, -2147483648, 2147483647},
        {132, "Auxiliary Valve Progressivity Retract", "n.a. -", 1.0, -2147483648, 2147483647},
        {133, "Auxiliary Valve Invert Ports", "n.a. -", 1.0, -2147483648, 2147483647},
        {134, "Device Element Offset X", "mm", 1.0, -2147483648, 2147483647},
        {135, "Device Element Offset Y", "mm", 1.0, -2147483648, 2147483647},
        {136, "Device Element Offset Z", "mm", 1.0, -2147483648, 2147483647},
        {137, "Device Volume Capacity (Deprecated)", "ml", 1.0, 0, 2147483647},
        {138, "Device Mass Capacity (Deprecated)", "g", 1.0, 0, 2147483647},
        {139, "Device Count Capacity (Deprecated)", "#", 1.0, 0, 2147483647},
        {140, "Setpoint Percentage Application Rate", "ppm", 1.0, 0, 2147483647},
        {141, "Actual Work State", "n.a. -", 1.0, 0, 3},
        {142, "Physical Setpoint Time Latency", "ms", 1.0, 0, 2147483647},
        {143, "Physical Actual Value Time Latency", "ms", 1.0, -2147483648, 2147483647},
        {144, "Yaw Angle", "deg", 0.001, -180000, 180000},
        {145, "Roll Angle", "deg", 0.001, -180000, 180000},
        {146, "Pitch Angle", "deg", 0.001, -180000, 180000},
        {147, "Log Count", "n.a. -", 1.0, 0, 2147483647},
        {148, "Total Fuel Consumption", "ml", 1.0, 0, 2147483647},
        {149, "Instantaneous Fuel Consumption per Time", "mm3/s", 1.0, 0, 2147483647},
        {150, "Instantaneous Fuel Consumption per Area", "mm3/m2", 1.0, 0, 2147483647},
        {151, "Instantaneous Area Per Time Capacity", "mm2/s", 1.0, 0, 2147483647},
        {153, "Actual Normalized Difference Vegetative Index (NDVI)", "n.a. -", 0.001, -1000, 1000},
        {154, "Physical Object Length", "mm", 1.0, 0, 2147483647},
        {155, "Physical Object Width", "mm", 1.0, 0, 2147483647},
        {156, "Physical Object Height", "mm", 1.0, 0, 2147483647},
        {157, "Connector Type", "n.a. -", 1.0, -1, 10},
        {158, "Prescription Control State", "n.a. -", 1.0, 0, 3},
        {159, "Number of Sub-Units per Section", "#", 1.0, 0, 2147483647},
        {160, "Section Control State", "n.a. -", 1.0, 0, 3},
        {161, "Actual Condensed Work State (1-16)", "n.a. -", 1.0, 0, 2147483647},
        {162, "Actual Condensed Work State (17-32)", "n.a. -", 1.0, 0, 2147483647},
        {163, "Actual Condensed Work State (33-48)", "n.a. -", 1.0, 0, 2147483647},
        {164, "Actual Condensed Work State (49-64)", "n.a. -", 1.0, 0, 2147483647},
        {165, "Actual Condensed Work State (65-80)", "n.a. -", 1.0, 0, 2147483647},
        {166, "Actual Condensed Work State (81-96)", "n.a. -", 1.0, 0, 2147483647},
        {167, "Actual Condensed Work State (97-112)", "n.a. -", 1.0, 0, 2147483647},
        {168, "Actual Condensed Work State (113-128)", "n.a. -", 1.0, 0, 2147483647},
        {169, "Actual Condensed Work State (129-144)", "n.a. -", 1.0, 0, 2147483647},
        {170, "Actual Condensed Work State (145-160)", "n.a. -", 1.0, 0, 2147483647},
        {171, "Actual Condensed Work State (161-176)", "n.a. -", 1.0, 0, 2147483647},
        {172, "Actual Condensed Work State (177-192)", "n.a. -", 1.0, 0, 2147483647},
        {173, "Actual Condensed Work State (193-208)", "n.a. -", 1.0, 0, 2147483647},
        {174, "Actual Condensed Work State (209-224)", "n.a. -", 1.0, 0, 2147483647},
        {175, "Actual Condensed Work State (225-240)", "n.a. -", 1.0, 0, 2147483647},
        {176, "Actual Condensed Work State (241-256)", "n.a. -", 1.0, 0, 2147483647},
        {177, "Actual length of cut", "mm", 0.001, 0, 2147483647},
        {178, "Element Type Instance", "n.a. -", 1.0, 0, 65533},
        {179, "Actual Cultural Practice", "n.a. -", 1.0, 0, 2147483647},
        {180, "Device Reference Point (DRP) to Ground distance", "mm", 1.0, -2147483648, 2147483647},
        {181, "Dry Mass Per Area Yield", "mg/m2", 1.0, 0, 2147483647},
        {182, "Dry Mass Per Time Yield", "mg/s", 1.0, 0, 2147483647},
        {183, "Yield Total Dry Mass", "kg", 1.0, 0, 2147483647},
        {184, "Reference Moisture For Dry Mass", "ppm", 1.0, 0, 2147483647},
        {185, "Seed Cotton Mass Per Area Yield", "mg/m2", 1.0, 0, 2147483647},
        {186, "Lint Cotton Mass Per Area Yield", "mg/m2", 1.0, 0, 2147483647},
        {187, "Seed Cotton Mass Per Time Yield", "mg/s", 1.0, 0, 2147483647},
        {188, "Lint Cotton Mass Per Time Yield", "mg/s", 1.0, 0, 2147483647},
        {189, "Yield Total Seed Cotton Mass", "kg", 1.0, 0, 2147483647},
        {190, "Yield Total Lint Cotton Mass", "kg", 1.0, 0, 2147483647},
        {191, "Lint Turnout Percentage", "ppm", 1.0, 0, 2147483647},
        {192, "Ambient temperature", "mK", 1.0, 0, 1000000},
        {193, "Setpoint Product Pressure", "Pa", 0.1, -2147483648, 2147483647},
        {194, "Actual Product Pressure", "Pa", 0.1, -2147483648, 2147483647},
        {195, "Minimum Product Pressure", "Pa", 0.1, -2147483648, 2147483647},
        {196, "Maximum Product Pressure", "Pa", 0.1, -2147483648, 2147483647},
        {197, "Setpoint Pump Output Pressure", "Pa", 0.1, -2147483648, 2147483647},
        {198, "Actual Pump Output Pressure", "Pa", 0.1, -2147483648, 2147483647},
        {199, "Minimum Pump Output Pressure", "Pa", 0.1, -2147483648, 2147483647},
        {200, "Maximum Pump Output Pressure", "Pa", 0.1, -2147483648, 2147483647},
        {201, "Setpoint Tank Agitation Pressure", "Pa", 0.1, -2147483648, 2147483647},
        {202, "Actual Tank Agitation Pressure", "Pa", 0.1, -2147483648, 2147483647},
        {203, "Minimum Tank Agitation Pressure", "Pa", 0.1, -2147483648, 2147483647},
        {204, "Maximum Tank Agitation Pressure", "Pa", 0.1, -2147483648, 2147483647},
        {205, "SC Setpoint Turn On Time", "ms", 1.0, -2147483648, 2147483647},
        {206, "SC Setpoint Turn Off Time", "ms", 1.0, -2147483648, 2147483647},
        {207, "Wind speed", "mm/s", 1.0, 0, 100000000},
        {208, "Wind direction", "deg", 1.0, 0, 359},
        {209, "Relative Humidity", "%", 1.0, 0, 100},
        {210, "Sky conditions", "n.a. -", 1.0, 0, 2147483647},
        {211, "Last Bale Flakes per Bale", "#", 1.0, 0, 1000},
        {212, "Last Bale Average Moisture", "ppm", 1.0, 0, 100000000},
        {213, "Last Bale Average Strokes per Flake", "#", 1.0, 0, 1000},
        {214, "Lifetime Bale Count", "#", 1.0, 0, 2147483647},
        {215, "Lifetime Working Hours", "h", 0.05, 0, 2147483647},
        {216, "Actual Bale Hydraulic Pressure", "Pa", 1.0, 0, 2147483647},
        {217, "Last Bale Average Hydraulic Pressure", "Pa", 1.0, 0, 2147483647},
        {218, "Setpoint Bale Compression Plunger Load", "ppm", 1.0, 0, 2147483647},
        {219, "Actual Bale Compression Plunger Load", "ppm", 1.0, 0, 2147483647},
        {220, "Last Bale Average Bale Compression Plunger Load", "ppm", 1.0, 0, 2147483647},
        {221, "Last Bale Applied Preservative", "ml", 1.0, 0, 2147483647},
        {222, "Last Bale Tag Number", "n.a. -", 1.0, -2147483648, 2147483647},
        {223, "Last Bale Mass", "g", 1.0, 0, 2147483647},
        {224, "Delta T", "mK", 1.0, 0, 1000000},
        {225, "Setpoint Working Length", "mm", 1.0, 0, 2147483647},
        {226, "Actual Working Length", "mm", 1.0, 0, 2147483647},
        {227, "Minimum Working Length", "mm", 1.0, 0, 2147483647},
        {228, "Maximum Working Length", "mm", 1.0, 0, 2147483647},
        {229, "Actual Net Weight", "g", 1.0, -2147483648, 2147483647},
        {230, "Net Weight State", "n.a. -", 1.0, 0, 3},
        {231, "Setpoint Net Weight", "g", 1.0, -2147483648, 2147483647},
        {232, "Actual Gross Weight", "g", 1.0, -2147483648, 2147483647},
        {233, "Gross Weight State", "n.a. -", 1.0, 0, 3},
        {234, "Minimum Gross Weight", "g", 1.0, -2147483648, 2147483647},
        {235, "Maximum Gross Weight", "g", 1.0, -2147483648, 2147483647},
        {236, "Thresher Engagement Total Time", "s", 1.0, 0, 2147483647},
        {237, "Actual Header Working Height Status", "n.a. -", 1.0, 0, 3},
        {238, "Actual Header Rotational Speed Status", "n.a. -", 1.0, 0, 3},
        {239, "Yield Hold Status", "n.a. -", 1.0, 0, 3},
        {240, "Actual (Un)Loading System Status", "n.a. -", 1.0, -2147483648, 2147483647},
        {241, "Crop Temperature", "mK", 1.0, 0, 1000000},
        {242, "Setpoint Sieve Clearance", "mm", 1.0, 0, 2147483647},
        {243, "Actual Sieve Clearance", "mm", 1.0, 0, 2147483647},
        {244, "Minimum Sieve Clearance", "mm", 1.0, 0, 2147483647},
        {245, "Maximum Sieve Clearance", "mm", 1.0, 0, 2147483647},
        {246, "Setpoint Chaffer Clearance", "mm", 1.0, 0, 2147483647},
        {247, "Actual Chaffer Clearance", "mm", 1.0, 0, 2147483647},
        {248, "Minimum Chaffer Clearance", "mm", 1.0, 0, 2147483647},
        {249, "Maximum Chaffer Clearance", "mm", 1.0, 0, 2147483647},
        {250, "Setpoint Concave Clearance", "mm", 1.0, 0, 2147483647},
        {251, "Actual Concave Clearance", "mm", 1.0, 0, 2147483647},
        {252, "Minimum Concave Clearance", "mm", 1.0, 0, 2147483647},
        {253, "Maximum Concave Clearance", "mm", 1.0, 0, 2147483647},
        {254, "Setpoint Separation Fan Rotational Speed", "/s", 0.001, 0, 2147483647},
        {255, "Actual Separation Fan Rotational Speed", "/s", 0.001, 0, 2147483647},
        {256, "Minimum Separation Fan Rotational Speed", "/s", 0.001, 0, 2147483647},
        {257, "Maximum Separation Fan Rotational Speed", "/s", 0.001, 0, 2147483647},
        {258, "Hydraulic Oil Temperature", "mK", 1.0, 0, 2000000},
        {259, "Yield Lag Ignore Time", "ms", 1.0, 0, 2147483647},
        {260, "Yield Lead Ignore Time", "ms", 1.0, 0, 2147483647},
        {261, "Average Yield Mass Per Time", "mg/s", 1.0, 0, 2147483647},
        {262, "Average Crop Moisture", "ppm", 1.0, 0, 2147483647},
        {263, "Average Yield Mass Per Area", "mg/m2", 1.0, 0, 2147483647},
        {264, "Connector Pivot X-Offset", "mm", 1.0, -2147483648, 2147483647},
        {265, "Remaining Area", "m2", 1.0, 0, 2147483647},
        {266, "Lifetime Application Total Mass", "kg", 1.0, 0, 2147483647},
        {267, "Lifetime Application Total Count", "#", 1.0, 0, 2147483647},
        {268, "Lifetime Yield Total Volume", "L", 1.0, 0, 2147483647},
        {269, "Lifetime Yield Total Mass", "kg", 1.0, 0, 2147483647},
        {270, "Lifetime Yield Total Count", "#", 1.0, 0, 2147483647},
        {271, "Lifetime Total Area", "m2", 1.0, 0, 2147483647},
        {272, "Lifetime Effective Total Distance", "m", 1.0, 0, 2147483647},
        {273, "Lifetime Ineffective Total Distance", "m", 1.0, 0, 2147483647},
        {274, "Lifetime Effective Total Time", "s", 1.0, 0, 2147483647},
        {275, "Lifetime Ineffective Total Time", "s", 1.0, 0, 2147483647},
        {276, "Lifetime Fuel Consumption", "L", 0.5, 0, 2147483647},
        {277, "Lifetime Average Fuel Consumption per Time", "mm3/s", 1.0, 0, 2147483647},
        {278, "Lifetime Average Fuel Consumption per Area", "mm3/m2", 1.0, 0, 2147483647},
        {279, "Lifetime Yield Total Dry Mass", "kg", 1.0, 0, 2147483647},
        {280, "Lifetime Yield Total Seed Cotton Mass", "kg", 1.0, 0, 2147483647},
        {281, "Lifetime Yield Total Lint Cotton Mass", "kg", 1.0, 0, 2147483647},
        {282, "Lifetime Threshing Engagement Total Time", "s", 1.0, 0, 2147483647},
        {283, "Precut Total Count", "#", 1.0, 0, 2147483647},
        {284, "Uncut Total Count", "#", 1.0, 0, 2147483647},
        {285, "Lifetime Precut Total Count", "#", 1.0, 0, 2147483647},
        {286, "Lifetime Uncut Total Count", "#", 1.0, 0, 2147483647},
        {287, "Setpoint Prescription Mode", "n.a. -", 1.0, 0, 6},
        {288, "Actual Prescription Mode", "n.a. -", 1.0, 0, 5},
        {289, "Setpoint Work State", "n.a. -", 1.0, 0, 3},
        {290, "Setpoint Condensed Work State (1-16)", "n.a. -", 1.0, 0, 2147483647},
        {291, "Setpoint Condensed Work State (17-32)", "n.a. -", 1.0, 0, 2147483647},
        {292, "Setpoint Condensed Work State (33-48)", "n.a. -", 1.0, 0, 2147483647},
        {293, "Setpoint Condensed Work State (49-64)", "n.a. -", 1.0, 0, 2147483647},
        {294, "Setpoint Condensed Work State (65-80)", "n.a. -", 1.0, 0, 2147483647},
        {295, "Setpoint Condensed Work State (81-96)", "n.a. -", 1.0, 0, 2147483647},
        {296, "Setpoint Condensed Work State (97-112)", "n.a. -", 1.0, 0, 2147483647},
        {297, "Setpoint Condensed Work State (113-128)", "n.a. -", 1.0, 0, 2147483647},
        {298, "Setpoint Condensed Work State (129-144)", "n.a. -", 1.0, 0, 2147483647},
        {299, "Setpoint Condensed Work State (145-160)", "n.a. -", 1.0, 0, 2147483647},
        {300, "Setpoint Condensed Work State (161-176)", "n.a. -", 1.0, 0, 2147483647},
        {301, "Setpoint Condensed Work State (177-192)", "n.a. -", 1.0, 0, 2147483647},
        {302, "Setpoint Condensed Work State (193-208)", "n.a. -", 1.0, 0, 2147483647},
        {303, "Setpoint Condensed Work State (209-224)", "n.a. -", 1.0, 0, 2147483647},
        {304, "Setpoint Condensed Work State (225-240)", "n.a. -", 1.0, 0, 2147483647},
        {305, "Setpoint Condensed Work State (241-256)", "n.a. -", 1.0, 0, 2147483647},
        {306, "True Rotation Point  X-Offset", "mm", 1.0, -2147483648, 2147483647},
        {307, "True Rotation Point Y-Offset", "mm", 1.0, -2147483648, 2147483647},
        {308, "Actual Percentage Application Rate", "ppm", 1.0, 0, 2147483647},
        {309, "Minimum Percentage Application Rate", "ppm", 1.0, 0, 2147483647},
        {310, "Maximum Percentage Application Rate", "ppm", 1.0, 0, 2147483647},
        {311, "Relative Yield Potential", "ppm", 1.0, 0, 2147483647},
        {312, "Minimum Relative Yield Potential", "ppm", 1.0, 0, 2147483647},
        {313, "Maximum Relative Yield Potential", "ppm", 1.0, 0, 2147483647},
        {314, "Actual Percentage Crop Dry Matter", "ppm", 1.0, 0, 2147483647},
        {315, "Average Percentage Crop Dry Matter", "ppm", 1.0, 0, 2147483647},
        {316, "Effective Total Fuel Consumption", "ml", 1.0, 0, 2147483647},
        {317, "Ineffective Total Fuel Consumption", "ml", 1.0, 0, 2147483647},
        {318, "Effective Total Diesel Exhaust Fluid Consumption", "ml", 1.0, 0, 2147483647},
        {319, "Ineffective Total Diesel Exhaust Fluid Consumption", "ml", 1.0, 0, 2147483647},
        {320, "Last loaded Weight", "g", 1.0, 0, 2147483647},
        {321, "Last unloaded Weight", "g", 1.0, 0, 2147483647},
        {322, "Load Identification Number", "#", 1.0, 0, 2147483647},
        {323, "Unload Identification Number", "#", 1.0, 0, 2147483647},
        {324, "Chopper Engagement Total Time", "s", 1.0, 0, 2147483647},
        {325, "Lifetime Application Total Volume", "L", 1.0, 0, 2147483647},
        {326, "Setpoint Header Speed", "/s", 0.001, 0, 2147483647},
        {327, "Actual Header Speed", "/s", 0.001, 0, 2147483647},
        {328, "Minimum Header Speed", "/s", 0.001, 0, 2147483647},
        {329, "Maximum Header Speed", "/s", 0.001, 0, 2147483647},
        {330, "Setpoint Cutting drum speed", "/s", 0.001, 0, 2147483647},
        {331, "Actual Cutting drum speed", "/s", 0.001, 0, 2147483647},
        {332, "Minimum Cutting drum speed", "/s", 0.001, 0, 2147483647},
        {333, "Maximum Cutting drum speed", "/s", 0.001, 0, 2147483647},
        {334, "Operating Hours Since Last Sharpening", "s", 1.0, 0, 2147483647},
        {335, "Front PTO hours", "s", 1.0, 0, 2147483647},
        {336, "Rear PTO hours", "s", 1.0, 0, 2147483647},
        {337, "Lifetime Front PTO hours", "h", 0.1, 0, 2147483647},
        {338, "Lifetime Rear PTO Hours", "h", 0.1, 0, 2147483647},
        {339, "Total Loading Time", "s", 1.0, 0, 2147483647},
        {340, "Total Unloading Time", "s", 1.0, 0, 2147483647},
        {341, "Setpoint Grain Kernel Cracker Gap", "mm", 0.001, 0, 2147483647},
        {342, "Actual Grain Kernel Cracker Gap", "mm", 0.001, 0, 2147483647},
        {343, "Minimum Grain Kernel Cracker Gap", "mm", 0.001, 0, 2147483647},
        {344, "Maximum Grain Kernel Cracker Gap", "mm", 0.001, 0, 2147483647},
        {345, "Setpoint Swathing Width", "mm", 1.0, 0, 2147483647},
        {346, "Actual Swathing Width", "mm", 1.0, 0, 2147483647},
        {347, "Minimum Swathing Width", "mm", 1.0, 0, 2147483647},
        {348, "Maximum Swathing Width", "mm", 1.0, 0, 2147483647},
        {349, "Nozzle Drift Reduction", "ppm", 1.0, 0, 100},
        {350, "Function or Operation Technique", "n.a. -", 1.0, 0, 2147483647},
        {351, "Application Total Volume", "ml", 1.0, 0, 2147483647},
        {352, "Application Total Mass in gram [g]", "g", 1.0, 0, 2147483647},
        {353, "Total Application of Nitrogen [N2]", "g", 1.0, 0, 2147483647},
        {354, "Total Application of Ammonium", "g", 1.0, 0, 2147483647},
        {355, "Total Application of Phosphor", "g", 1.0, 0, 2147483647},
        {356, "Total Application of Potassium", "g", 1.0, 0, 2147483647},
        {357, "Total Application of Dry Matter", "kg", 1.0, 0, 2147483647},
        {358, "Average Dry Yield Mass Per Time", "mg/s", 1.0, 0, 2147483647},
        {359, "Average Dry Yield Mass Per Area", "mg/m2", 1.0, 0, 2147483647},
        {360, "Last Bale Size", "mm", 1.0, 0, 2147483647},
        {361, "Last Bale Density", "mg/l (mass per unit volume)", 1.0, 0, 2147483647},
        {362, "Total Bale Length", "mm", 1.0, 0, 2147483647},
        {363, "Last Bale Dry Mass", "g", 1.0, 0, 2147483647},
        {364, "Actual Flake Size", "mm", 1.0, 0, 1000},
        {365, "Setpoint Downforce Pressure", "Pa", 1.0, -2147483648, 2147483647},
        {366, "Actual Downforce Pressure", "Pa", 1.0, -2147483648, 2147483647},
        {367, "Condensed Section Override State (1-16)", "", 1.0, 0, 2147483647},
        {368, "Condensed Section Override State (17-32)", "", 1.0, 0, 2147483647},
        {369, "Condensed Section Override State (33-48)", "", 1.0, 0, 2147483647},
        {370, "Condensed Section Override State (49-64)", "", 1.0, 0, 2147483647},
        {371, "Condensed Section Override State (65-80)", "", 1.0, 0, 2147483647},
        {372, "Condensed Section Override State (81-96)", "", 1.0, 0, 2147483647},
        {373, "Condensed Section Override State (97-112)", "", 1.0, 0, 2147483647},
        {374, "Condensed Section Override State (113-128)", "", 1.0, 0, 2147483647},
        {375, "Condensed Section Override State (129-144)", "", 1.0, 0, 2147483647},
        {376, "Condensed Section Override State (145-160)", "", 1.0, 0, 2147483647},
        {377, "Condensed Section Override State (161-176)", "", 1.0, 0, 2147483647},
        {378, "Condensed Section Override State (177-192)", "", 1.0, 0, 2147483647},
        {379, "Condensed Section Override State (193-208)", "", 1.0, 0, 2147483647},
        {380, "Condensed Section Override State (209-224)", "", 1.0, 0, 2147483647},
        {381, "Condensed Section Override State (225-240)", "", 1.0, 0, 2147483647},
        {382, "Condensed Section Override State (241-256)", "", 1.0, 0, 2147483647},
        {383, "Apparent Wind Direction", "deg", 1.0, 0, 359},
        {384, "Apparent Wind Speed", "mm/s", 1.0, 0, 100000000},
        {385, "MSL Atmospheric Pressure", "Pa", 0.1, 0, 2000000},
        {386, "Actual Atmospheric Pressure", "Pa", 0.1, 0, 2000000},
        {387, "Total Revolutions in Fractional Revolutions", "#", 0.0001, -2147483648, 2147483647},
        {388, "Total Revolutions in Complete Revolutions", "#", 1.0, -2147483648, 2147483647},
        {389, "Setpoint Revolutions specified as count per time", "r/min", 0.0001, -2147483648, 2147483647},
        {390, "Actual Revolutions Per Time", "r/min", 0.0001, -2147483648, 2147483647},
        {391, "Default Revolutions Per Time", "r/min", 0.0001, -2147483648, 2147483647},
        {392, "Minimum Revolutions Per Time", "r/min", 0.0001, -2147483648, 2147483647},
        {393, "Maximum Revolutions Per Time", "r/min", 0.0001, -2147483648, 2147483647},
        {394, "Actual Fuel Tank Content", "ml", 1.0, 0, 2147483647},
        {395, "Actual Diesel Exhaust Fluid Tank Content", "ml", 1.0, 0, 2147483647},
        {396, "Setpoint Speed", "mm/s", 1.0, -2147483648, 2147483647},
        {397, "Actual Speed", "mm/s", 1.0, -2147483648, 2147483647},
        {398, "Minimum Speed", "mm/s", 1.0, -2147483648, 2147483647},
        {399, "Maximum Speed", "mm/s", 1.0, -2147483648, 2147483647},
        {400, "Speed Source", "n.a. -", 1.0, 0, 255},
        {401, "Actual Application of Nitrogen [N2]", "mg/l", 1.0, 0, 2147483647},
        {402, "Actual application of Ammonium", "mg/l", 1.0, 0, 2147483647},
        {403, "Actual application of Phosphor", "mg/l", 1.0, 0, 2147483647},
        {404, "Actual application of Potassium", "mg/l", 1.0, 0, 2147483647},
        {405, "Actual application of Dry Matter", "mg/l", 1.0, 0, 2147483647},
        {406, "Actual Crude Protein Content", "ppm", 1.0, 0, 2147483647},
        {407, "Average Crude Protein Content", "ppm", 1.0, 0, 2147483647},
        {408, "Average Crop Contamination", "ppm", 1.0, 0, 2147483647},
        {409, "Total Diesel Exhaust Fluid Consumption", "ml", 1.0, 0, 2147483647},
        {410, "Instantaneous Diesel Exhaust Fluid Consumption per Time", "mm3/s", 1.0, 0, 2147483647},
        {411, "Instantaneous Diesel Exhaust Fluid Consumption per Area", "mm3/m2", 1.0, 0, 2147483647},
        {412, "Lifetime Diesel Exhaust Fluid Consumption", "L", 0.5, 0, 2147483647},
        {413, "Lifetime Average Diesel Exhaust Fluid Consumption per Time", "mm3/s", 1.0, 0, 2147483647},
        {414, "Lifetime Average Diesel Exhaust Fluid Consumption per Area", "mm3/m2", 1.0, 0, 2147483647},
        {415, "Actual Seed Singulation Percentage", "ppm", 1.0, 0, 1000000},
        {416, "Average Seed Singulation Percentage", "ppm", 1.0, 0, 1000000},
        {417, "Actual Seed Skip Percentage", "ppm", 1.0, 0, 1000000},
        {418, "Average Seed Skip Percentage", "ppm", 1.0, 0, 1000000},
        {419, "Actual Seed Multiple Percentage", "ppm", 1.0, 0, 1000000},
        {420, "Average Seed Multiple Percentage", "ppm", 1.0, 0, 1000000},
        {421, "Actual Seed Spacing Deviation", "mm", 1.0, 0, 2147483647},
        {422, "Average Seed Spacing Deviation", "mm", 1.0, 0, 2147483647},
        {423, "Actual Coefficient of Variation of Seed Spacing Percentage", "ppm", 1.0, 0, 1000000},
        {424, "Average Coefficient of Variation of Seed Spacing Percentage", "ppm", 1.0, 0, 1000000},
        {425, "Setpoint Maximum Allowed Seed Spacing Deviation", "mm", 1.0, 0, 2147483647},
        {426, "Setpoint Downforce as Force", "N", 1.0, -2147483648, 2147483647},
        {427, "Actual Downforce as Force", "N", 1.0, -2147483648, 2147483647},
        {428, "Loaded Total Mass", "kg", 1.0, 0, 2147483647},
        {429, "Unloaded Total Mass", "kg", 1.0, 0, 2147483647},
        {430, "Lifetime Loaded Total Mass", "kg", 1.0, 0, 2147483647},
        {431, "Lifetime Unloaded Total Mass", "kg", 1.0, 0, 2147483647},
        {432, "Setpoint Application Rate of Nitrogen [N2]", "mg/m2", 1.0, 0, 2147483647},
        {433, "Actual  Application Rate of Nitrogen [N2]", "mg/m2", 1.0, 0, 2147483647},
        {434, "Minimum Application Rate of Nitrogen [N2]", "mg/m2", 1.0, 0, 2147483647},
        {435, "Maximum  Application Rate of Nitrogen [N2]", "mg/m2", 1.0, 0, 2147483647},
        {436, "Setpoint  Application Rate of Ammonium", "mg/m2", 1.0, 0, 2147483647},
        {437, "Actual  Application Rate of Ammonium", "mg/m2", 1.0, 0, 2147483647},
        {438, "Minimum  Application Rate of Ammonium", "mg/m2", 1.0, 0, 2147483647},
        {439, "Maximum  Application Rate of Ammonium", "mg/m2", 1.0, 0, 2147483647},
        {440, "Setpoint  Application Rate of Phosphor", "mg/m2", 1.0, 0, 2147483647},
        {441, "Actual  Application Rate of Phosphor", "mg/m2", 1.0, 0, 2147483647},
        {442, "Minimum  Application Rate of Phosphor", "mg/m2", 1.0, 0, 2147483647},
        {443, "Maximum  Application Rate of Phosphor", "mg/m2", 1.0, 0, 2147483647},
        {444, "Setpoint  Application Rate of Potassium", "mg/m2", 1.0, 0, 2147483647},
        {445, "Actual  Application Rate of Potassium", "mg/m2", 1.0, 0, 2147483647},
        {446, "Minimum Application Rate of Potassium", "mg/m2", 1.0, 0, 2147483647},
        {447, "Maximum Application Rate of Potassium", "mg/m2", 1.0, 0, 2147483647},
        {448, "Setpoint Application Rate of Dry Matter", "ppm", 1.0, 0, 2147483647},
        {449, "Actual  Application Rate of Dry Matter", "ppm", 1.0, 0, 2147483647},
        {450, "Minimum Application Rate of Dry Matter", "ppm", 1.0, 0, 2147483647},
        {451, "Maximum Application Rate of Dry Matter", "ppm", 1.0, 0, 2147483647},
        {452, "Loaded Total Volume", "ml", 1.0, 0, 2147483647},
        {453, "Unloaded Total Volume", "ml", 1.0, 0, 2147483647},
        {454, "Lifetime loaded Total Volume", "L", 1.0, 0, 2147483647},
        {455, "Lifetime Unloaded Total Volume", "L", 1.0, 0, 2147483647},
        {456, "Last loaded Volume", "ml", 1.0, 0, 2147483647},
        {457, "Last unloaded Volume", "ml", 1.0, 0, 2147483647},
        {458, "Loaded Total Count", "#", 1.0, 0, 2147483647},
        {459, "Unloaded Total Count", "#", 1.0, 0, 2147483647},
        {460, "Lifetime Loaded Total Count", "#", 1.0, 0, 2147483647},
        {461, "Lifetime Unloaded Total Count", "#", 1.0, 0, 2147483647},
        {462, "Last loaded Count", "#", 1.0, 0, 2147483647},
        {463, "Last unloaded Count", "#", 1.0, 0, 2147483647},
        {464, "Haul Counter", "#", 1.0, 0, 2147483647},
        {465, "Lifetime Haul Counter", "#", 1.0, 0, 2147483647},
        {466, "Actual relative connector angle", "deg", 0.001, -180000, 180000},
        {467, "Actual Percentage Content", "%", 0.01, 0, 10000},
        {468, "Soil Snow/Frozen Condtion", "n.a. -", 1.0, 0, 3},
        {469, "Estimated Soil Water Condtion", "n.a. -", 1.0, 0, 6},
        {470, "Soil Compaction", "n.a. -", 1.0, 0, 4},
        {471, "Setpoint Cultural Practice", "n.a. -", 1.0, 0, 2147483647},
        {472, "Setpoint Length of Cut", "mm", 0.001, 0, 2147483647},
        {473, "Minimum length of cut", "mm", 0.001, 0, 2147483647},
        {474, "Maximum Length of Cut", "mm", 0.001, 0, 2147483647},
        {475, "Setpoint Bale Hydraulic Pressure", "Pa", 1.0, 0, 2147483647},
        {476, "Minimum Bale Hydraulic Pressure", "Pa", 1.0, 0, 2147483647},
        {477, "Maximum Bale Hydraulic Pressure", "Pa", 1.0, 0, 2147483647},
        {478, "Setpoint Flake Size", "mm", 1.0, 0, 1000},
        {479, "Minimum Flake Size", "mm", 1.0, 0, 1000},
        {480, "Maximum Flake Size", "mm", 1.0, 0, 1000},
        {481, "Setpoint Number of Subbales", "", 1.0, 0, 2147483647},
        {482, "Last Bale Number of Subbales", "", 1.0, 0, 2147483647},
        {483, "Setpoint Engine Speed", "r/min", 0.0001, 0, 2147483647},
        {484, "Actual Engine Speed", "r/min", 0.0001, 0, 2147483647},
        {485, "Minimum Engine Speed", "r/min", 0.0001, 0, 2147483647},
        {486, "Maximum Engine Speed", "r/min", 0.0001, 0, 2147483647},
        {488, "Diesel Exhaust Fluid Tank Percentage Level", "%", 0.01, 0, 10000},
        {489, "Maximum Diesel Exhaust Fluid Tank Content", "ml", 1.0, 0, 2147483647},
        {490, "Maximum Fuel Tank Content", "ml", 1.0, 0, 2147483647},
        {491, "Fuel Percentage Level", "%", 0.01, 0, 2147483647},
        {492, "Total Engine Hours", "h", 0.05, 0, 2147483647},
        {493, "Lifetime Engine Hours", "h", 0.1, 0, 2147483647},
        {494, "Last Event Partner ID (Byte 1-4)", "", 1.0, 0, 0},
        {495, "Last Event Partner ID (Byte 5-8)", "n.a. -", 1.0, 0, 2147483647},
        {496, "Last Event Partner ID (Byte 9-12)", "n.a. -", 1.0, 0, 2147483647},
        {497, "Last Event Partner ID (Byte 13-16)", "", 1.0, 0, 2147483647},
        {498, "Last Event Partner ID Type", "n.a. -", 1.0, 0, 2147483647},
        {499, "Last Event Partner ID Manufacturer ID Code", "", 1.0, 0, 2147483647},
        {500, "Last Event Partner ID Device Class", "n.a. -", 1.0, 0, 2147483647},
        {501, "Setpoint Engine Torque", "%", 0.001, 0, 2147483647},
        {502, "Actual Engine Torque", "%", 0.001, 0, 2147483647},
        {503, "Minimum Engine Torque", "%", 0.001, 0, 2147483647},
        {504, "Maximum Engine Torque", "%", 0.001, 0, 2147483647},
        {505, "Tramline Control Level", "n.a. -", 1.0, 0, 7},
        {506, "Setpoint Tramline Control Level", "n.a. -", 1.0, 0, 3},
        {507, "Tramline Sequence Number", "n.a. -", 1.0, 0, 2147483647},
        {508, "Unique A-B Guidance Reference Line ID", "n.a. -", 1.0, 0, 2147483647},
        {509, "Actual Track Number", "n.a. -", 1.0, -2147483648, 2147483647},
        {510, "Track Number to the right", "n.a. -", 1.0, -2147483648, 2147483647},
        {511, "Track Number to the left", "n.a. -", 1.0, -2147483648, 2147483647},
        {512, "Guidance Line Swath Width", "mm", 1.0, 0, 2147483647},
        {513, "Guidance Line Deviation", "mm", 1.0, -2147483648, 2147483647},
        {514, "GNSS Quality", "n.a. -", 1.0, 0, 2147483647},
        {515, "Tramline Control State", "n.a. -", 1.0, 0, 3},
        {516, "Tramline Overdosing Rate", "ppm", 1.0, 0, 2147483647},
        {517, "Setpoint Tramline Condensed Work State (1-16)", "n.a. -", 1.0, 0, 2147483647},
        {518, "Actual Tramline Condensed Work State (1-16)", "n.a. -", 1.0, 0, 2147483647},
        {519, "Last Bale Lifetime Count", "n.a. -", 1.0, 0, 2147483647},
        {520, "Actual Canopy Height", "mm", 1.0, 0, 2147483647},
        {521, "GNSS Installation Type", "n.a. -", 1.0, 0, 100},
        {522, "Twine Bale Total Count (Deprecated)", "#", 1.0, 0, 2147483647},
        {523, "Mesh Bale Total Count (Deprecated)", "#", 1.0, 0, 2147483647},
        {524, "Lifetime Twine Bale Total Count (Deprecated)", "#", 1.0, 0, 2147483647},
        {525, "Lifetime Mesh Bale Total Count (Deprecated)", "#", 1.0, 0, 2147483647},
        {526, "Actual Cooling Fluid Temperature", "mK", 1.0, 0, 2147483647},
        {528, "Last Bale Capacity", "kg/h", 1.0, 0, 2147483647},
        {529, "Setpoint Tillage Disc Gang Angle", "deg", 0.001, -180000, 180000},
        {530, "Actual Tillage Disc Gang Angle", "deg", 0.001, -180000, 180000},
        {531, "Actual Applied Preservative Per Yield Mass", "mm3/kg", 1.0, 0, 2147483647},
        {532, "Setpoint Applied Preservative Per Yield Mass", "mm3/kg", 1.0, 0, 2147483647},
        {533, "Default Applied Preservative Per Yield Mass", "mm3/kg", 1.0, 0, 2147483647},
        {534, "Minimum Applied Preservative Per Yield Mass", "mm3/kg", 1.0, 0, 2147483647},
        {535, "Maximum Applied Preservative Per Yield Mass", "mm3/kg", 1.0, 0, 2147483647},
        {536, "Total Applied Preservative", "ml", 1.0, 0, 2147483647},
        {537, "Lifetime Applied Preservative", "ml", 1.0, 0, 2147483647},
        {538, "Average Applied Preservative Per Yield Mass", "mm3/kg", 1.0, 0, 2147483647},
        {539, "Actual Preservative Tank Volume", "ml", 1.0, 0, 2147483647},
        {540, "Actual Preservative Tank Level", "ppm", 1.0, 0, 2147483647},
        {541, "Actual PTO Speed", "r/min", 0.0001, 0, 2147483647},
        {542, "Setpoint PTO Speed", "r/min", 0.0001, 0, 2147483647},
        {543, "Default PTO Speed", "r/min", 0.0001, 0, 2147483647},
        {544, "Minimum PTO Speed", "r/min", 0.0001, 0, 2147483647},
        {545, "Maximum PTO Speed", "r/min", 0.0001, 0, 2147483647},
        {546, "Lifetime Chopping Engagement Total Time", "s", 1.0, 0, 2147483647},
        {547, "Setpoint Bale Compression Plunger Load (N)", "N", 1.0, 0, 2147483647},
        {548, "Actual Bale Compression Plunger Load (N)", "N", 1.0, 0, 2147483647},
        {549, "Last Bale Average Bale Compression Plunger Load (N)", "N", 1.0, 0, 2147483647},
        {550, "Ground Cover", "%", 0.1, 0, 1000},
        {551, "Actual PTO Torque", "N*m", 0.0001, 0, 2147483647},
        {552, "Setpoint PTO Torque", "N*m", 0.0001, 0, 2147483647},
        {553, "Default PTO Torque", "N*m", 0.0001, 0, 2147483647},
        {554, "Minimum PTO Torque", "N*m", 0.0001, 0, 2147483647},
        {555, "Maximum PTO Torque", "N*m", 0.0001, 0, 2147483647},
        {556, "Present Weather Conditions", "", 1.0, 1, 6},
        {557, "Setpoint Electrical Current", "A", 0.005, 0, 2147483647},
        {558, "Actual Electrical Current", "A", 0.005, 0, 2147483647},
        {559, "Minimum Electrical Current", "A", 0.005, 0, 2147483647},
        {560, "Maximum Electrical Current", "A", 0.005, 0, 2147483647},
        {561, "Default Electrical Current", "A", 0.005, 0, 2147483647},
        {562, "Setpoint Voltage", "V", 0.001, -2147483648, 2147483647},
        {563, "Default Voltage", "V", 0.001, -2147483648, 2147483647},
        {564, "Actual Voltage", "V", 0.001, -2147483648, 2147483647},
        {565, "Minimum Voltage", "V", 0.001, -2147483648, 2147483647},
        {566, "Maximum Voltage", "V", 0.001, -2147483648, 2147483647},
        {567, "Actual Electrical Resistance", "Ohm", 0.01, 0, 2147483647},
        {568, "Setpoint Electrical Power", "W", 0.001, 0, 2147483647},
        {569, "Actual Electrical Power", "W", 0.001, 0, 2147483647},
        {570, "Default Electrical Power", "W", 0.001, 0, 2147483647},
        {571, "Maximum Electrical Power", "W", 0.001, 0, 2147483647},
        {572, "Minimum Electrical Power", "W", 0.001, 0, 2147483647},
        {573, "Total Electrical Energy", "kWh", 0.001, 0, 2147483647},
        {574, "Setpoint Electrical Energy per Area Application Rate", "kWh/m2", 1.0, 0, 2147483647},
        {575, "Actual  Electrical Energy per Area Application Rate", "kWh/m2", 1.0, 0, 2147483647},
        {576, "Maximum  Electrical Energy  per Area Application Rate", "kWh/m2", 1.0, 0, 2147483647},
        {577, "Minimum  Electrical Energy per Area Application Rate", "kWh/m2", 1.0, 0, 2147483647},
        {578, "Setpoint Temperature", "mK", 1.0, 0, 1000000},
        {579, "Actual Temperature", "mK", 1.0, 0, 1000000},
        {580, "Minimum Temperature", "mK", 1.0, 0, 1000000},
        {581, "Maximum Temperature", "mK", 1.0, 0, 1000000},
        {582, "Default Temperature", "mK", 1.0, 0, 1000000},
        {583, "Setpoint Frequency", "Hz", 0.001, 0, 2147483647},
        {584, "Actual Frequency", "Hz", 0.001, 0, 2147483647},
        {585, "Minimum Frequency", "Hz", 0.001, 0, 2147483647},
        {586, "Maximum Frequency", "Hz", 0.001, 0, 2147483647},
        {587, "Previous Rainfall", "", 1.0, 1, 6},
        {588, "Setpoint Volume Per Area Application Rate", "ml/m2", 0.1, 0, 2147483647},
        {589, "Actual Volume Per Area Application Rate", "ml/m2", 0.1, 0, 2147483647},
        {590, "Minimum Volume Per Area Application Rate", "ml/m2", 0.1, 0, 2147483647},
        {591, "Maximum Volume Per Area Application Rate", "ml/m2", 0.1, 0, 2147483647},
        {592, "Default Volume Per Area Application Rate", "ml/m2", 0.1, 0, 2147483647},
        {593, "Traction Type", "n.a. -", 1.0, 0, 5},
        {594, "Steering Type", "n.a. -", 1.0, 0, 7},
        {595, "Machine Mode", "n.a. -", 1.0, 0, 2147483647},
        {596, "Cargo Area Cover State", "%", 1.0, -1, 100},
        {597, "Total Distance", "mm", 1.0, 0, 2147483647},
        {598, "Lifetime Total Distance", "m", 1.0, 0, 2147483647},
        {599, "Total Distance Field", "mm", 1.0, 0, 2147483647},
        {600, "Lifetime Total Distance Field", "m", 1.0, 0, 2147483647},
        {601, "Total Distance Street", "mm", 1.0, 0, 2147483647},
        {602, "Lifetime Total Distance Street", "m", 1.0, 0, 2147483647},
        {603, "Actual Tramline Condensed Work State (17-32)", "n.a. -", 1.0, 0, 2147483647},
        {604, "Actual Tramline Condensed Work State (33-48)", "n.a. -", 1.0, 0, 2147483647},
        {605, "Actual Tramline Condensed Work State (49-64)", "n.a. -", 1.0, 0, 2147483647},
        {606, "Actual Tramline Condensed Work State (65-80)", "n.a. -", 1.0, 0, 2147483647},
        {607, "Actual Tramline Condensed Work State (81-96)", "n.a. -", 1.0, 0, 2147483647},
        {608, "Actual Tramline Condensed Work State (97-112)", "n.a. -", 1.0, 0, 2147483647},
        {609, "Actual Tramline Condensed Work State (113-128)", "n.a. -", 1.0, 0, 2147483647},
        {610, "Actual Tramline Condensed Work State (129-144)", "n.a. -", 1.0, 0, 2147483647},
        {611, "Actual Tramline Condensed Work State (145-160)", "n.a. -", 1.0, 0, 2147483647},
        {612, "Actual Tramline Condensed Work State (161-176)", "n.a. -", 1.0, 0, 2147483647},
        {613, "Actual Tramline Condensed Work State (177-192)", "n.a. -", 1.0, 0, 2147483647},
        {614, "Actual Tramline Condensed Work State (193-208)", "n.a. -", 1.0, 0, 2147483647},
        {615, "Actual Tramline Condensed Work State (209-224)", "n.a. -", 1.0, 0, 2147483647},
        {616, "Actual Tramline Condensed Work State (225-240)", "n.a. -", 1.0, 0, 2147483647},
        {617, "Actual Tramline Condensed Work State (241-256)", "n.a. -", 1.0, 0, 2147483647},
        {618, "Setpoint Tramline Condensed Work State (17-32)", "n.a. -", 1.0, 0, 2147483647},
        {619, "Setpoint Tramline Condensed Work State (33-48)", "n.a. -", 1.0, 0, 2147483647},
        {620, "Setpoint Tramline Condensed Work State (49-64)", "n.a. -", 1.0, 0, 2147483647},
        {621, "Setpoint Tramline Condensed Work State (65-80)", "n.a. -", 1.0, 0, 2147483647},
        {622, "Setpoint Tramline Condensed Work State (81-96)", "n.a. -", 1.0, 0, 2147483647},
        {623, "Setpoint Tramline Condensed Work State (97-112)", "n.a. -", 1.0, 0, 2147483647},
        {624, "Setpoint Tramline Condensed Work State (113-128)", "n.a. -", 1.0, 0, 2147483647},
        {625, "Setpoint Tramline Condensed Work State (129-144)", "n.a. -", 1.0, 0, 2147483647},
        {626, "Setpoint Tramline Condensed Work State (145-160)", "n.a. -", 1.0, 0, 2147483647},
        {627, "Setpoint Tramline Condensed Work State (161-176)", "n.a. -", 1.0, 0, 2147483647},
        {628, "Setpoint Tramline Condensed Work State (177-192)", "n.a. -", 1.0, 0, 2147483647},
        {629, "Setpoint Tramline Condensed Work State (193-208)", "n.a. -", 1.0, 0, 2147483647},
        {630, "Setpoint Tramline Condensed Work State (209-224)", "n.a. -", 1.0, 0, 2147483647},
        {631, "Setpoint Tramline Condensed Work State (225-240)", "n.a. -", 1.0, 0, 2147483647},
        {632, "Setpoint Tramline Condensed Work State (241-256)", "n.a. -", 1.0, 0, 2147483647},
        {633, "Setpoint Volume per distance Application Rate", "ml/m", 0.001, 0, 2147483647},
        {634, "Actual Volume per distance Application Rate", "ml/m", 0.001, 0, 2147483647},
        {635, "Default Volume per distance Application Rate", "ml/m", 0.001, 0, 2147483647},
        {636, "Minimum Volume per distance Application Rate", "ml/m", 0.001, 0, 2147483647},
        {637, "Maximum Volume per distance Application Rate", "ml/m", 0.001, 0, 2147483647},
        {638, "Setpoint Tire Pressure", "Pa", 0.1, 0, 2147483647},
        {639, "Actual Tire Pressure", "Pa", 0.1, 0, 2147483647},
        {640, "Default Tire Pressure", "Pa", 0.1, 0, 2147483647},
        {641, "Minimum Tire Pressure", "Pa", 0.1, 0, 2147483647},
        {642, "Maximum Tire Pressure", "Pa", 0.1, 0, 2147483647},
        {643, "Actual Tire Temperature", "mK", 1.0, 0, 1000000},
        {644, "Binding Method", "n.a. -", 1.0, 0, 5},
        {645, "Last Bale Number of Knives", "#", 1.0, 0, 2147483647},
        {646, "Last Bale Binding Twine Consumption", "mm", 1.0, 0, 2147483647},
        {647, "Last Bale Binding Mesh Consumption", "mm", 1.0, 0, 2147483647},
        {648, "Last Bale Binding Film Consumption", "mm", 1.0, 0, 2147483647},
        {649, "Last Bale Binding Film Stretching", "%", 0.001, 0, 2147483647},
        {650, "Last Bale Wrapping Film Width", "mm", 1.0, 0, 2147483647},
        {651, "Last Bale Wrapping Film Consumption", "mm", 1.0, 0, 2147483647},
        {652, "Last Bale Wrapping Film Stretching", "%", 0.001, 0, 2147483647},
        {653, "Last Bale Wrapping Film Overlap Percentage", "%", 0.001, 0, 2147483647},
        {654, "Last Bale Wrapping Film Layers", "#", 1.0, 0, 2147483647},
        {655, "Electrical Apparent Soil Conductivity", "mS/m", 0.1, -2147483648, 2147483647},
        {656, "SC Actual Turn On Time", "", 1.0, -2147483648, 2147483647},
        {657, "SC Actual Turn Off Time", "", 1.0, -2147483648, 2147483647},
        {658, "Actual CO2 equivalent specified as mass per area", "mg/m2", 1.0, 0, 2147483647},
        {659, "Actual CO2 equivalent specified as mass per time", "mg/s", 1.0, 0, 2147483647},
        {660, "Actual CO2 equivalent specified as mass per mass", "mg/kg", 1.0, 0, 2147483647},
        {661, "Actual CO2 equivalent specified as mass per yield", "mg/kg", 1.0, 0, 2147483647},
        {662, "Actual CO2 equivalent specified as mass per volume", "mg/l", 1.0, 0, 2147483647},
        {663, "Actual CO2 equivalent specified as mass per count", "n.a. -", 1.0, 0, 2147483647},
        {664, "Total CO2 equivalent", "kg", 1.0, 0, 2147483647},
        {665, "Lifetime total CO2 equivalent", "kg", 1.0, 0, 2147483647},
        {666, "Working Direction", "", 1.0, 0, 2},
        {667, "Distance between Guidance Track Number 0R and 1", "mm", 1.0, 0, 2147483647},
        {668, "Distance between Guidance Track Number 0R and 0L", "mm", 1.0, 0, 2147483647},
        {669, "Bout Track Number Shift", "", 1.0, -2147483648, 2147483647},
        {670, "Tramline Primary Working Width", "mm", 1.0, 0, 2147483647},
        {671, "Tramline Primary Tire Width", "mm", 1.0, 0, 2147483647},
        {672, "Tramline Primary Wheel Distance", "mm", 1.0, 0, 2147483647},
        {673, "Tramline Secondary Working Width", "mm", 1.0, 0, 2147483647},
        {674, "Tramline Secondary Tire Width", "mm", 1.0, 0, 2147483647},
        {675, "Tramline Secondary Wheel Distance", "mm", 1.0, 0, 2147483647},
        {676, "Last Bale Binding Mesh Layers", "#", 1.0, 0, 2147483647},
        {677, "Last Bale Binding Film Layers", "#", 1.0, 0, 2147483647},
        {678, "Last Bale Binding Twine Layers", "#", 1.0, 0, 2147483647},
        {679, "Crop Contamination Total Mass", "kg", 1.0, 0, 2147483647},
        {680, "Crop Contamination Lifetime Total Mass", "kg", 1.0, 0, 2147483647},
        {681, "Film bale Total Count", "#", 1.0, 0, 2147483647},
        {682, "Mesh bale Total Count", "#", 1.0, 0, 2147483647},
        {683, "Twine bale Total Count", "#", 1.0, 0, 2147483647},
        {684, "Wrapping Film bale Total Count", "#", 1.0, 0, 2147483647},
        {685, "Lifetime Film Bale Total Count", "#", 1.0, 0, 2147483647},
        {686, "Lifetime Mesh Bale Total Count", "#", 1.0, 0, 2147483647},
        {687, "Lifetime Twine Bale Total Count", "#", 1.0, 0, 2147483647},
        {688, "Lifetime Wrapping Film Bale Total Count", "#", 1.0, 0, 2147483647},
        {689, "Effective Total Electrical Battery Energy Consumption", "kWh", 0.001, -2147483648, 2147483647},
        {690, "Ineffective Total Electrical Battery Energy Consumption", "kWh", 0.001, -2147483648, 2147483647},
        {691, "Instantaneous Electrical Battery Energy Consumption per Time", "W", 1.0, -2147483648, 2147483647},
        {692, "Instantaneous Electrical Battery Energy Consumption per Area", "kWh/m2", 1.0, -2147483648, 2147483647},
        {693, "Lifetime Total Loading Time", "s", 1.0, 0, 2147483647},
        {694, "Lifetime Total Unloading Time", "s", 1.0, 0, 2147483647},
        {695, "Mesh Total Used", "mm", 1.0, 0, 2147483647},
        {696, "Film Total Used", "mm", 1.0, 0, 2147483647},
        {697, "Twine Total Used", "mm", 1.0, 0, 2147483647},
        {698, "Average Mass Per Bale", "kg", 1.0, 0, 2147483647},
        {699, "Average Mass Per Load", "kg", 1.0, 0, 2147483647},
        {700, "Nutrient Analysis Method", "n.a. -", 1.0, 0, 2147483647},
        {701, "Actual Starch Content", "ppm", 1.0, 0, 2147483647},
        {702, "Average Starch Content", "ppm", 1.0, 0, 2147483647},
        {703, "Actual Ash Content", "ppm", 1.0, 0, 2147483647},
        {704, "Average Ash Content", "ppm", 1.0, 0, 2147483647},
        {705, "Actual Sugar Content", "ppm", 1.0, 0, 2147483647},
        {706, "Average Sugar Content", "ppm", 1.0, 0, 2147483647},
        {707, "Actual Lipids Content", "ppm", 1.0, 0, 2147483647},
        {708, "Average Lipids Content", "ppm", 1.0, 0, 2147483647},
        {709, "Actual Crude Fat Content", "ppm", 1.0, 0, 2147483647},
        {710, "Average Crude Fat Content", "ppm", 1.0, 0, 2147483647},
        {711, "Actual Crude Fiber Content", "ppm", 1.0, 0, 2147483647},
        {712, "Average Crude Fiber Content", "ppm", 1.0, 0, 2147483647},
        {713, "Actual Nitrogen (TKN) Content", "ppm", 1.0, 0, 2147483647},
        {714, "Average Nitrogen (TKN) Content", "ppm", 1.0, 0, 2147483647},
        {715, "Actual Phosphates Content", "ppm", 1.0, 0, 2147483647},
        {716, "Average Phosphates Content", "ppm", 1.0, 0, 2147483647},
        {717, "Actual Potassium Content", "ppm", 1.0, 0, 2147483647},
        {718, "Average Potassium Content", "ppm", 1.0, 0, 2147483647},
        {719, "Actual ADL Content", "ppm", 1.0, 0, 2147483647},
        {720, "Average ADL Content", "ppm", 1.0, 0, 2147483647},
        {721, "Actual ADF Content", "ppm", 1.0, 0, 2147483647},
        {722, "Average ADF Content", "ppm", 1.0, 0, 2147483647},
        {723, "Actual NDF Content", "ppm", 1.0, 0, 2147483647},
        {724, "Average NDF Content", "ppm", 1.0, 0, 2147483647},
        {725, "Actual uNDFom240 Content", "ppm", 1.0, 0, 2147483647},
        {726, "Average uNDFom240 Content", "ppm", 1.0, 0, 2147483647},
        {727, "Actual Biomethanation Production Potential", "mm3/kg", 1.0, 0, 2147483647},
        {728, "Average Biomethanation Production Potential", "mm3/kg", 1.0, 0, 2147483647},
        {32768, "Maximum Droplet Size", "n.a. -", 1.0, 0, 255},
        {32769, "Maximum Crop Grade Diameter", "mm", 0.001, 0, 2147483647},
        {32770, "Maximum Crop Grade Length", "mm", 0.001, 0, 2147483647},
        {32771, "Maximum Crop Contamination Mass per Area", "mg/m2", 1.0, 0, 2147483647},
        {32772, "Maximum Crop Contamination Mass per Time", "mg/s", 1.0, 0, 2147483647},
        {32773, "Maximum Crop Conditioning Intensity", "%", 0.01, 0, 10000},
        {36864, "Minimum Droplet Size", "n.a. -", 1.0, 0, 255},
        {36865, "Minimum Crop Grade Diameter", "mm", 0.001, 0, 2147483647},
        {36866, "Minimum Crop Grade Length", "mm", 0.001, 0, 2147483647},
        {36867, "Minimum Crop Contamination Mass per Area", "mg/m2", 1.0, 0, 2147483647},
        {36868, "Minimum Crop Contamination Mass per Time", "mg/s", 1.0, 0, 2147483647},
        {36869, "Minimum Crop Conditioning Intensity", "%", 0.01, 0, 10000},
        {40960, "Default Droplet Size", "n.a. -", 1.0, 0, 255},
        {40961, "Default Crop Grade Diameter", "mm", 0.001, 0, 2147483647},
        {40962, "Default Crop Grade Length", "mm", 0.001, 0, 2147483647},
        {40963, "Default Crop Contamination Mass per Area", "mg/m2", 1.0, 0, 2147483647},
        {40964, "Default Crop Contamination Mass per Time", "mg/s", 1.0, 0, 2147483647},
        {40965, "Default Crop Conditioning Intensity", "%", 0.01, 0, 10000},
        {45056, "Actual Droplet Size", "n.a. -", 1.0, 0, 255},
        {45057, "Actual Crop Grade Diameter", "mm", 0.001, 0, 2147483647},
        {45058, "Actual Crop Grade Length", "mm", 0.001, 0, 2147483647},
        {45059, "Actual Crop Contamination Mass per Area", "mg/m2", 1.0, 0, 2147483647},
        {45060, "Actual Crop Contamination Mass per Time", "mg/s", 1.0, 0, 2147483647},
        {45061, "Actual Crop Conditioning Intensity", "%", 0.01, 0, 10000},
        {49152, "Setpoint Droplet Size", "n.a. -", 1.0, 0, 255},
        {49153, "Setpoint Crop Grade Diameter", "mm", 0.001, 0, 2147483647},
        {49154, "Setpoint Crop Grade Length", "mm", 0.001, 0, 2147483647},
        {49155, "Setpoint Crop Contamination Mass per Area", "mg/m2", 1.0, 0, 2147483647},
        {49156, "Setpoint Crop Contamination Mass per Time", "mg/s", 1.0, 0, 2147483647},
        {49157, "Setpoint Crop Conditioning Intensity", "%", 0.01, 0, 10000},
        {57342, "PGN Based Data", "n.a. -", 1.0, -2147483648, 2147483647},
        {57343, "Request Default Process Data", "n.a. -", 1.0, 0, 0},
        {57344, "65534 Proprietary DDI Range", "n.a. -", 0.0, 0, 2147483647},
        {65535, "Reserved", "n.a. -", 0.0, 0, 2147483647}};

    inline constexpr usize DDI_DATABASE_SIZE = 760;

    // ─── DDI Lookup ──────────────────────────────────────────────────────────
    inline const DDIDefinition *ddi_lookup(u16 ddi) {
        for (usize i = 0; i < DDI_DATABASE_SIZE; ++i) {
            if (DDI_DATABASE[i].ddi == ddi)
                return &DDI_DATABASE[i];
        }
        return nullptr;
    }

    inline const char *ddi_name(u16 ddi) {
        auto *def = ddi_lookup(ddi);
        return def ? def->name : "Unknown";
    }

    inline const char *ddi_unit(u16 ddi) {
        auto *def = ddi_lookup(ddi);
        return def ? def->unit : "";
    }

    inline f64 ddi_resolution(u16 ddi) {
        auto *def = ddi_lookup(ddi);
        return def ? def->resolution : 1.0;
    }

    inline f64 ddi_to_engineering(u16 ddi, i32 raw) { return static_cast<f64>(raw) * ddi_resolution(ddi); }

    inline i32 ddi_from_engineering(u16 ddi, f64 eng) {
        f64 res = ddi_resolution(ddi);
        return (res != 0.0) ? static_cast<i32>(eng / res) : static_cast<i32>(eng);
    }

    // ─── DDI Category Helpers ────────────────────────────────────────────────
    inline bool ddi_is_rate(u16 ddi) { return ddi >= 1 && ddi <= 55; }
    inline bool ddi_is_total(u16 ddi) {
        return (ddi >= 116 && ddi <= 123) || (ddi >= 130 && ddi <= 131) || (ddi >= 351 && ddi <= 353);
    }
    inline bool ddi_is_geometry(u16 ddi) { return ddi >= 134 && ddi <= 140; }
    inline bool ddi_is_section_control(u16 ddi) { return ddi >= 153 && ddi <= 161; }
    inline bool ddi_is_speed_distance(u16 ddi) { return (ddi >= 56 && ddi <= 60) || (ddi >= 33 && ddi <= 35); }
    inline bool ddi_is_proprietary(u16 ddi) { return ddi >= 57344 && ddi <= 65534; }

    // ─── Legacy DDIDatabase class (backwards compatibility) ──────────────────
    class DDIDatabase {
      public:
        static dp::Optional<DDIEntry> lookup(u16 ddi) {
            auto *def = ddi_lookup(ddi);
            if (def) {
                DDIEntry entry;
                entry.ddi = def->ddi;
                entry.name = def->name;
                entry.unit = def->unit;
                entry.resolution = def->resolution;
                entry.min_value = def->min_value;
                entry.max_value = def->max_value;
                return entry;
            }
            return dp::nullopt;
        }

        static const char *unit_for(u16 ddi) { return ddi_unit(ddi); }
        static const char *name_for(u16 ddi) { return ddi_name(ddi); }
        static f64 to_engineering(u16 ddi, i32 raw) { return ddi_to_engineering(ddi, raw); }
        static i32 from_engineering(u16 ddi, f64 eng) { return ddi_from_engineering(ddi, eng); }
        static bool is_geometry_ddi(u16 ddi) { return ddi_is_geometry(ddi); }
        static bool is_rate_ddi(u16 ddi) { return ddi_is_rate(ddi); }
        static bool is_total_ddi(u16 ddi) { return ddi_is_total(ddi); }
    };

} // namespace agrobus::isobus::tc
// ─── ISO 11783-11 License Notice ────────────────────────────────────────────
// This file contains data derived from the ISO 11783-11 online database.
//
// Copyright (c) International Organization for Standardization (ISO).
// See: www.iso.org/iso/copyright.htm
//
// No reproduction on networking permitted without license from ISO.
// The data from the online database is supplied without liability.
// This representation is considered uncontrolled and represents a snap-shot
// of the ISO 11783-11 online data base (Version: 2025121001).
// ─────────────────────────────────────────────────────────────────────────────
