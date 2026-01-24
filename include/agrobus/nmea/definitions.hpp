#pragma once

// ─── NMEA2000 Protocol Definitions ─────────────────────────────────────────────
// Implements the protocol-side definitions from NMEA2000 (ISO 11783 compatible).
// Based on the NMEA2000 open-source reference implementation.
// This covers PGN definitions, enumeration types, resolution constants,
// and typed message structures for all supported NMEA2000 messages.
// ─────────────────────────────────────────────────────────────────────────────────

#include <agrobus/net/constants.hpp>
#include <agrobus/net/types.hpp>
#include <datapod/datapod.hpp>

namespace agrobus::nmea {
    using namespace agrobus::net;

    // ═════════════════════════════════════════════════════════════════════════════
    // ENUMERATION TYPES
    // ═════════════════════════════════════════════════════════════════════════════

    // ─── GNSS fix types (PGN 129029) ────────────────────────────────────────────
    enum class GNSSFixType : u8 {
        NoFix = 0,
        GNSSFix = 1,
        DGNSSFix = 2,
        PreciseGNSS = 3,
        RTKFixed = 4,
        RTKFloat = 5,
        DeadReckon = 6,
        Error = 14,
        Unavailable = 15
    };

    // ─── GNSS system types (PGN 129029) ─────────────────────────────────────────
    enum class GNSSSystem : u8 {
        GPS = 0,
        GLONASS = 1,
        GPSAndGLO = 2,
        GPS_SBAS = 3,
        GPS_SBAS_GLO = 4,
        Chayka = 5,
        Integrated = 6,
        Surveyed = 7,
        Galileo = 8
    };

    // ─── GNSS reference station type ────────────────────────────────────────────
    enum class ReferenceStationType : u8 { None = 0, RTCM = 1, CMR = 2, RTCM3 = 3 };

    // ─── GNSS DOP mode (PGN 129539) ────────────────────────────────────────────
    enum class GNSSDOPMode : u8 {
        Mode1D = 0,
        Mode2D = 1,
        Mode3D = 2,
        Auto = 3,
        Reserved1 = 4,
        Reserved2 = 5,
        Error = 6,
        Unavailable = 7
    };

    // ─── Heading reference (PGN 127250, 129026, 129284, etc.) ───────────────────
    enum class HeadingReference : u8 { True = 0, Magnetic = 1, Error = 2, Unavailable = 3 };

    // ─── Wind reference (PGN 130306) ───────────────────────────────────────────
    enum class WindReference : u8 {
        TrueNorth = 0,
        Magnetic = 1,
        Apparent = 2,
        TrueBoat = 3,
        TrueWater = 4,
        Error = 6,
        Unavailable = 7
    };

    // ─── Temperature source (PGN 130311, 130312, 130316) ───────────────────────
    enum class TemperatureSource : u8 {
        Sea = 0,
        Outside = 1,
        Inside = 2,
        EngineRoom = 3,
        MainCabin = 4,
        LiveWell = 5,
        BaitWell = 6,
        Refrigeration = 7,
        HeatingSystem = 8,
        DewPoint = 9,
        ApparentWindChill = 10,
        TheoreticalWindChill = 11,
        HeatIndex = 12,
        Freezer = 13,
        ExhaustGas = 14,
        ShaftSeal = 15
    };

    // ─── Humidity source (PGN 130313) ──────────────────────────────────────────
    enum class HumiditySource : u8 { Inside = 0, Outside = 1, Unavailable = 0xFF };

    // ─── Pressure source (PGN 130314, 130315) ─────────────────────────────────
    enum class PressureSource : u8 {
        Atmospheric = 0,
        Water = 1,
        Steam = 2,
        CompressedAir = 3,
        Hydraulic = 4,
        Filter = 5,
        AltimeterSetting = 6,
        Oil = 7,
        Fuel = 8,
        Reserved = 253,
        Error = 254,
        Unavailable = 255
    };

    // ─── Time source (PGN 126992) ─────────────────────────────────────────────
    enum class TimeSource : u8 {
        GPS = 0,
        GLONASS = 1,
        RadioStation = 2,
        LocalCesiumClock = 3,
        LocalRubidiumClock = 4,
        LocalCrystalClock = 5
    };

    // ─── Fluid type (PGN 127505) ──────────────────────────────────────────────
    enum class FluidType : u8 {
        Fuel = 0,
        Water = 1,
        GrayWater = 2,
        LiveWell = 3,
        Oil = 4,
        BlackWater = 5,
        FuelGasoline = 6,
        Error = 14,
        Unavailable = 15
    };

    // ─── DC type (PGN 127506) ─────────────────────────────────────────────────
    enum class DCType : u8 { Battery = 0, Alternator = 1, Converter = 2, SolarCell = 3, WindGenerator = 4 };

    // ─── Battery type (PGN 127513) ───────────────────────────────────────────
    enum class BatteryType : u8 { Flooded = 0, Gel = 1, AGM = 2 };

    // ─── Battery chemistry (PGN 127513) ─────────────────────────────────────
    enum class BatteryChemistry : u8 { LeadAcid = 0, LiIon = 1, NiCad = 2, ZnO = 3, NiMh = 4 };

    // ─── Battery nominal voltage (PGN 127513) ──────────────────────────────
    enum class BatteryNominalVoltage : u8 { V6 = 0, V12 = 1, V24 = 2, V32 = 3, V62 = 4, V42 = 5, V48 = 6 };

    // ─── Battery equalization support (PGN 127513) ─────────────────────────
    enum class BatteryEqSupport : u8 { No = 0, Yes = 1, Error = 2, Unavailable = 3 };

    // ─── Transmission gear (PGN 127493) ────────────────────────────────────
    enum class TransmissionGear : u8 { Forward = 0, Neutral = 1, Reverse = 2, Unknown = 3 };

    // ─── Rudder direction order (PGN 127237, 127245) ───────────────────────
    enum class RudderDirection : u8 { NoOrder = 0, Starboard = 1, Port = 2, Unavailable = 7 };

    // ─── Steering mode (PGN 127237) ───────────────────────────────────────
    enum class SteeringMode : u8 {
        MainSteering = 0,
        NonFollowUp = 1,
        FollowUp = 2,
        HeadingControlStandalone = 3,
        HeadingControl = 4,
        TrackControl = 5,
        Unavailable = 7
    };

    // ─── Turn mode (PGN 127237) ──────────────────────────────────────────
    enum class TurnMode : u8 {
        RudderLimitControlled = 0,
        TurnRateControlled = 1,
        RadiusControlled = 2,
        Unavailable = 7
    };

    // ─── Speed water reference type (PGN 128259) ──────────────────────────
    enum class SpeedWaterRefType : u8 {
        PaddleWheel = 0,
        PitotTube = 1,
        DopplerLog = 2,
        UltraSound = 3,
        Electromagnetic = 4,
        Error = 254,
        Unavailable = 255
    };

    // ─── Magnetic variation source (PGN 127258) ───────────────────────────
    enum class MagneticVariationSource : u8 {
        Manual = 0,
        Chart = 1,
        Table = 2,
        Calculated = 3,
        WMM2000 = 4,
        WMM2005 = 5,
        WMM2010 = 6,
        WMM2015 = 7,
        WMM2020 = 8,
        WMM2025 = 9
    };

    // ─── Navigation direction (PGN 129285) ─────────────────────────────────
    enum class NavigationDirection : u8 { Forward = 0, Reverse = 1, Error = 6, Unknown = 7 };

    // ─── Distance calculation type (PGN 129284) ────────────────────────────
    enum class DistanceCalculationType : u8 { GreatCircle = 0, RhumbLine = 1 };

    // ─── XTE mode (PGN 129283) ────────────────────────────────────────────
    enum class XTEMode : u8 { Autonomous = 0, Differential = 1, Estimated = 2, Simulator = 3, Manual = 4 };

    // ─── On/Off status (generic) ──────────────────────────────────────────
    enum class OnOff : u8 { Off = 0, On = 1, Error = 2, Unavailable = 3 };

    // ─── Charge state (PGN 127507) ─────────────────────────────────────────
    enum class ChargeState : u8 {
        NotCharging = 0,
        Bulk = 1,
        Absorption = 2,
        Overcharge = 3,
        Equalise = 4,
        Float = 5,
        NoFloat = 6,
        ConstantVI = 7,
        Disabled = 8,
        Fault = 9,
        Error = 14,
        Unavailable = 15
    };

    // ─── Charger mode (PGN 127507) ─────────────────────────────────────────
    enum class ChargerMode : u8 { Standalone = 0, Primary = 1, Secondary = 2, Echo = 3, Unavailable = 15 };

    // ─── Converter/Inverter operating state (PGN 127750) ───────────────────
    enum class ConverterMode : u8 {
        Off = 0,
        LowPower = 1,
        Fault = 2,
        Bulk = 3,
        Absorption = 4,
        Float = 5,
        Storage = 6,
        Equalise = 7,
        Passthru = 8,
        Inverting = 9,
        Assisting = 10,
        PSUMode = 11,
        NotAvailable = 0xFF
    };

    // ─── MOB status (PGN 127233) ──────────────────────────────────────────
    enum class MOBStatus : u8 { EmitterActivated = 0, ManualButtonActivation = 1, TestMode = 2, NotActive = 3 };

    // ─── MOB position source (PGN 127233) ─────────────────────────────────
    enum class MOBPositionSource : u8 { EstimatedByVessel = 0, ReportedByEmitter = 1 };

    // ─── MOB emitter battery status (PGN 127233) ──────────────────────────
    enum class MOBBatteryStatus : u8 { Good = 0, Low = 1 };

    // ─── AIS repeat indicator (PGN 129038, 129039, etc.) ──────────────────
    enum class AISRepeat : u8 { Initial = 0, First = 1, Second = 2, Final = 3 };

    // ─── AIS navigation status (PGN 129038) ──────────────────────────────
    enum class AISNavStatus : u8 {
        UnderWayMotoring = 0,
        AtAnchor = 1,
        NotUnderCommand = 2,
        RestrictedManoeuverability = 3,
        ConstrainedByDraught = 4,
        Moored = 5,
        Aground = 6,
        Fishing = 7,
        UnderWaySailing = 8,
        HazmatHighSpeed = 9,
        HazmatWingInGround = 10,
        AIS_SART = 14
    };

    // ─── AIS unit type (PGN 129039) ──────────────────────────────────────
    enum class AISUnit : u8 { ClassB_SOTDMA = 0, ClassB_CS = 1 };

    // ─── AIS mode (PGN 129039) ──────────────────────────────────────────
    enum class AISMode : u8 { Autonomous = 0, Assigned = 1 };

    // ─── AIS transceiver info (PGN 129039, 129041, 129802) ──────────────
    enum class AISTransceiverInfo : u8 {
        ChannelA_VDL_Rx = 0,
        ChannelB_VDL_Rx = 1,
        ChannelA_VDL_Tx = 2,
        ChannelB_VDL_Tx = 3,
        OwnInfoNotBroadcast = 4
    };

    // ─── AIS DTE (PGN 129794) ───────────────────────────────────────────
    enum class AISDTE : u8 { Ready = 0, NotReady = 1 };

    // ─── Delay source (PGN 127252) ─────────────────────────────────────
    enum class DelaySource : u8 { GPS = 0, GLONASS = 1, GPS_GLONASS = 2, Unavailable = 15 };

    // ─── Data mode (PGN 130577) ────────────────────────────────────────
    enum class DataMode : u8 { Autonomous = 0, Differential = 1, Estimated = 2, Simulator = 3, Manual = 4 };

    // ─── Range residual mode (PGN 129540) ─────────────────────────────
    enum class RangeResidualMode : u8 {
        RangeResiduals_Used = 0,
        RangeResiduals_Calculated = 1,
        Error = 2,
        Unavailable = 3
    };

    // ═════════════════════════════════════════════════════════════════════════════
    // RESOLUTION CONSTANTS
    // ═════════════════════════════════════════════════════════════════════════════

    inline constexpr f64 LAT_LON_RESOLUTION = 1.0e-7;     // degrees per bit
    inline constexpr f64 ALTITUDE_RESOLUTION = 0.01;      // meters per bit
    inline constexpr f64 SPEED_RESOLUTION = 0.01;         // m/s per bit
    inline constexpr f64 HEADING_RESOLUTION = 0.0001;     // radians per bit
    inline constexpr f64 COG_RESOLUTION = 0.0001;         // radians per bit
    inline constexpr f64 ROT_RESOLUTION = 3.125e-5;       // rad/s per bit (1e-3/32)
    inline constexpr f64 TEMPERATURE_RESOLUTION = 0.01;   // Kelvin per bit
    inline constexpr f64 PRESSURE_RESOLUTION = 100.0;     // Pa per bit (hPa)
    inline constexpr f64 WIND_SPEED_RESOLUTION = 0.01;    // m/s per bit
    inline constexpr f64 WIND_DIR_RESOLUTION = 0.0001;    // radians per bit
    inline constexpr f64 RPM_RESOLUTION = 0.25;           // RPM per bit
    inline constexpr f64 DEPTH_RESOLUTION = 0.01;         // meters per bit
    inline constexpr f64 DISTANCE_RESOLUTION = 0.01;      // meters per bit (log)
    inline constexpr f64 VOLTAGE_RESOLUTION = 0.01;       // Volts per bit
    inline constexpr f64 CURRENT_RESOLUTION = 0.1;        // Amps per bit
    inline constexpr f64 ANGLE_RESOLUTION = 0.0001;       // radians per bit
    inline constexpr f64 RUDDER_RESOLUTION = 0.0001;      // radians per bit
    inline constexpr f64 DOP_RESOLUTION = 0.01;           // DOP value per bit
    inline constexpr f64 XTE_RESOLUTION = 0.01;           // meters per bit
    inline constexpr f64 FLUID_LEVEL_RESOLUTION = 0.004;  // percent per bit (0.4% / 100)
    inline constexpr f64 FLUID_CAPACITY_RESOLUTION = 0.1; // liters per bit
    inline constexpr f64 HUMIDITY_RESOLUTION = 0.004;     // percent per bit (0.004%)
    inline constexpr f64 KELVIN_OFFSET = 273.15;          // Kelvin to Celsius offset

    // ═════════════════════════════════════════════════════════════════════════════
    // DATA STRUCTURES
    // ═════════════════════════════════════════════════════════════════════════════

    // ─── Wind ─────────────────────────────────────────────────────────────────
    struct WindData {
        u8 sid = 0xFF;
        f64 speed_mps = 0.0;
        f64 direction_rad = 0.0;
        WindReference reference = WindReference::Unavailable;
    };

    // ─── Temperature ──────────────────────────────────────────────────────────
    struct TemperatureData {
        u8 sid = 0xFF;
        u8 instance = 0;
        TemperatureSource source = TemperatureSource::Sea;
        f64 actual_k = 0.0;
        f64 set_k = 0.0; // For PGN 130312/130316 set temperature
    };

    // ─── Humidity ─────────────────────────────────────────────────────────────
    struct HumidityData {
        u8 sid = 0xFF;
        u8 instance = 0;
        HumiditySource source = HumiditySource::Unavailable;
        f64 actual_pct = 0.0;
        f64 set_pct = 0.0;
    };

    // ─── Pressure ─────────────────────────────────────────────────────────────
    struct PressureData {
        u8 sid = 0xFF;
        u8 instance = 0;
        PressureSource source = PressureSource::Unavailable;
        f64 pressure_pa = 0.0;
    };

    // ─── Outside Environmental (PGN 130310) ──────────────────────────────────
    struct OutsideEnvironmentalData {
        u8 sid = 0xFF;
        f64 water_temperature_k = 0.0;
        f64 outside_temperature_k = 0.0;
        f64 atmospheric_pressure_pa = 0.0;
    };

    // ─── Engine ───────────────────────────────────────────────────────────────
    struct EngineData {
        u8 instance = 0;
        f64 rpm = 0.0;
        f64 boost_pressure_pa = 0.0;
        i8 tilt_trim = 0;
    };

    struct EngineDynamicData {
        u8 instance = 0;
        f64 oil_pressure_pa = 0.0;
        f64 oil_temperature_k = 0.0;
        f64 coolant_temperature_k = 0.0;
        f64 alternator_voltage = 0.0;
        f64 fuel_rate_lph = 0.0; // liters per hour
        f64 engine_hours = 0.0;  // seconds
        f64 coolant_pressure_pa = 0.0;
        f64 fuel_pressure_pa = 0.0;
        i8 load_pct = 0;
        i8 torque_pct = 0;
        u16 status1 = 0;
        u16 status2 = 0;
    };

    // ─── Transmission ─────────────────────────────────────────────────────────
    struct TransmissionData {
        u8 instance = 0;
        TransmissionGear gear = TransmissionGear::Unknown;
        f64 oil_pressure_pa = 0.0;
        f64 oil_temperature_k = 0.0;
        u8 discrete_status = 0;
    };

    // ─── Engine Trip ──────────────────────────────────────────────────────────
    struct EngineTripData {
        u8 instance = 0;
        f64 trip_fuel_used_l = 0.0;
        f64 fuel_rate_avg_lph = 0.0;
        f64 fuel_rate_economy = 0.0;
        f64 instantaneous_economy = 0.0;
    };

    // ─── Water Depth ──────────────────────────────────────────────────────────
    struct WaterDepthData {
        u8 sid = 0xFF;
        f64 depth_m = 0.0;
        f64 offset_m = 0.0;
        f64 range_m = 0.0;
    };

    // ─── Speed, Water Referenced ──────────────────────────────────────────────
    struct SpeedWaterData {
        u8 sid = 0xFF;
        f64 water_speed_mps = 0.0;
        f64 ground_speed_mps = 0.0;
        SpeedWaterRefType reference = SpeedWaterRefType::Unavailable;
    };

    // ─── Distance Log ─────────────────────────────────────────────────────────
    struct DistanceLogData {
        u16 days_since_epoch = 0;
        f64 seconds_since_midnight = 0.0;
        u32 log_m = 0;      // Total log in meters
        u32 trip_log_m = 0; // Trip log in meters
    };

    // ─── Leeway ───────────────────────────────────────────────────────────────
    struct LeewayData {
        u8 sid = 0xFF;
        f64 leeway_rad = 0.0;
    };

    // ─── System Time ──────────────────────────────────────────────────────────
    struct SystemTimeData {
        u8 sid = 0xFF;
        TimeSource source = TimeSource::GPS;
        u16 days_since_epoch = 0;
        f64 seconds_since_midnight = 0.0;
    };

    // ─── Heading ──────────────────────────────────────────────────────────────
    struct HeadingData {
        u8 sid = 0xFF;
        f64 heading_rad = 0.0;
        f64 deviation_rad = 0.0;
        f64 variation_rad = 0.0;
        HeadingReference reference = HeadingReference::Unavailable;
    };

    // ─── Rate of Turn ─────────────────────────────────────────────────────────
    struct RateOfTurnData {
        u8 sid = 0xFF;
        f64 rate_rad_per_s = 0.0;
    };

    // ─── Heave ────────────────────────────────────────────────────────────────
    struct HeaveData {
        u8 sid = 0xFF;
        f64 heave_m = 0.0;
        f64 delay_s = 0.0;
        DelaySource delay_source = DelaySource::Unavailable;
    };

    // ─── Attitude ─────────────────────────────────────────────────────────────
    struct AttitudeData {
        u8 sid = 0xFF;
        f64 yaw_rad = 0.0;
        f64 pitch_rad = 0.0;
        f64 roll_rad = 0.0;
    };

    // ─── Magnetic Variation ───────────────────────────────────────────────────
    struct MagneticVariationData {
        u8 sid = 0xFF;
        MagneticVariationSource source = MagneticVariationSource::Manual;
        u16 days_since_epoch = 0;
        f64 variation_rad = 0.0;
    };

    // ─── Rudder ───────────────────────────────────────────────────────────────
    struct RudderData {
        f64 position_rad = 0.0;
        u8 instance = 0;
        RudderDirection direction = RudderDirection::NoOrder;
        f64 angle_order_rad = 0.0;
    };

    // ─── Heading/Track Control ────────────────────────────────────────────────
    struct HeadingTrackControlData {
        OnOff rudder_limit_exceeded = OnOff::Unavailable;
        OnOff off_heading_limit_exceeded = OnOff::Unavailable;
        OnOff off_track_limit_exceeded = OnOff::Unavailable;
        OnOff override = OnOff::Unavailable;
        SteeringMode steering_mode = SteeringMode::Unavailable;
        TurnMode turn_mode = TurnMode::Unavailable;
        HeadingReference heading_reference = HeadingReference::Unavailable;
        RudderDirection commanded_rudder_direction = RudderDirection::Unavailable;
        f64 commanded_rudder_angle_rad = 0.0;
        f64 heading_to_steer_rad = 0.0;
        f64 track_rad = 0.0;
        f64 rudder_limit_rad = 0.0;
        f64 off_heading_limit_rad = 0.0;
        f64 radius_of_turn_m = 0.0;
        f64 rate_of_turn_rad_per_s = 0.0;
        f64 off_track_limit_m = 0.0;
        f64 vessel_heading_rad = 0.0;
    };

    // ─── Fluid Level ──────────────────────────────────────────────────────────
    struct FluidLevelData {
        u8 instance = 0;
        FluidType type = FluidType::Unavailable;
        f64 level_pct = 0.0;  // 0-100%
        f64 capacity_l = 0.0; // liters
    };

    // ─── DC Detailed Status ───────────────────────────────────────────────────
    struct DCDetailedData {
        u8 sid = 0xFF;
        u8 dc_instance = 0;
        DCType type = DCType::Battery;
        f64 voltage = 0.0;
        f64 current_a = 0.0;
        f64 temperature_k = 0.0;
    };

    // ─── Battery Status ───────────────────────────────────────────────────────
    struct BatteryStatusData {
        u8 instance = 0;
        f64 voltage = 0.0;
        f64 current_a = 0.0;
        u8 state_of_charge_pct = 0xFF; // 0-100, 0xFF = unavailable
        u8 state_of_health_pct = 0xFF;
        f64 time_remaining_s = 0.0;
    };

    // ─── Battery Configuration ────────────────────────────────────────────────
    struct BatteryConfigData {
        u8 instance = 0;
        BatteryType type = BatteryType::Flooded;
        BatteryEqSupport eq_support = BatteryEqSupport::Unavailable;
        BatteryNominalVoltage nominal_voltage = BatteryNominalVoltage::V12;
        BatteryChemistry chemistry = BatteryChemistry::LeadAcid;
        f64 capacity_ah = 0.0;
        u8 temperature_coefficient_pct = 0;
        f64 peukert_exponent = 0.0;
        u8 charge_efficiency_pct = 0;
    };

    // ─── GNSS DOPs ────────────────────────────────────────────────────────────
    struct GNSSDOPData {
        u8 sid = 0xFF;
        GNSSDOPMode desired_mode = GNSSDOPMode::Auto;
        GNSSDOPMode actual_mode = GNSSDOPMode::Unavailable;
        f64 hdop = 0.0;
        f64 vdop = 0.0;
        f64 tdop = 0.0;
    };

    // ─── Satellite info (for PGN 129540) ──────────────────────────────────────
    struct SatelliteInfo {
        u8 prn = 0;
        f64 elevation_rad = 0.0;
        f64 azimuth_rad = 0.0;
        f64 snr_db = 0.0;
        f64 range_residual_m = 0.0;
        GNSSSystem system = GNSSSystem::GPS;
    };

    struct SatellitesInViewData {
        u8 sid = 0xFF;
        RangeResidualMode mode = RangeResidualMode::Unavailable;
        u8 num_satellites = 0;
        dp::Vector<SatelliteInfo> satellites;
    };

    // ─── Local Time Offset ────────────────────────────────────────────────────
    struct LocalTimeOffsetData {
        u16 days_since_epoch = 0;
        f64 seconds_since_midnight = 0.0;
        i16 local_offset_minutes = 0; // offset from UTC in minutes
    };

    // ─── XTE ──────────────────────────────────────────────────────────────────
    struct XTEData {
        u8 sid = 0xFF;
        XTEMode mode = XTEMode::Autonomous;
        bool navigation_terminated = false;
        f64 xte_m = 0.0;
    };

    // ─── Navigation Data ──────────────────────────────────────────────────────
    struct NavigationData {
        u8 sid = 0xFF;
        f64 distance_to_wp_m = 0.0;
        HeadingReference bearing_reference = HeadingReference::Unavailable;
        bool perpendicular_crossed = false;
        bool arrival_circle_entered = false;
        DistanceCalculationType calc_type = DistanceCalculationType::GreatCircle;
        f64 eta_time = 0.0; // seconds since midnight
        i16 eta_date = 0;   // days since 1970
        f64 bearing_origin_to_dest_rad = 0.0;
        f64 bearing_pos_to_dest_rad = 0.0;
        u32 origin_wp_number = 0;
        u32 dest_wp_number = 0;
        f64 dest_latitude = 0.0;
        f64 dest_longitude = 0.0;
        f64 wp_closing_velocity_mps = 0.0;
    };

    // ─── MOB ──────────────────────────────────────────────────────────────────
    struct MOBData {
        u8 sid = 0xFF;
        u32 emitter_id = 0;
        MOBStatus status = MOBStatus::NotActive;
        f64 activation_time = 0.0;
        MOBPositionSource position_source = MOBPositionSource::EstimatedByVessel;
        u16 position_date = 0;
        f64 position_time = 0.0;
        f64 latitude = 0.0;
        f64 longitude = 0.0;
        HeadingReference cog_reference = HeadingReference::Unavailable;
        f64 cog_rad = 0.0;
        f64 sog_mps = 0.0;
        u32 mmsi = 0;
        MOBBatteryStatus battery = MOBBatteryStatus::Good;
    };

    // ─── AIS Class A Position ─────────────────────────────────────────────────
    struct AISClassAPosition {
        u8 message_id = 0;
        AISRepeat repeat = AISRepeat::Initial;
        u32 user_id = 0; // MMSI
        f64 latitude = 0.0;
        f64 longitude = 0.0;
        bool accuracy = false;
        bool raim = false;
        u8 seconds = 0;
        f64 cog_rad = 0.0;
        f64 sog_mps = 0.0;
        AISTransceiverInfo transceiver = AISTransceiverInfo::ChannelA_VDL_Rx;
        f64 heading_rad = 0.0;
        f64 rot_rad_per_s = 0.0;
        AISNavStatus nav_status = AISNavStatus::UnderWayMotoring;
    };

    // ─── AIS Class B Position ─────────────────────────────────────────────────
    struct AISClassBPosition {
        u8 message_id = 0;
        AISRepeat repeat = AISRepeat::Initial;
        u32 user_id = 0;
        f64 latitude = 0.0;
        f64 longitude = 0.0;
        bool accuracy = false;
        bool raim = false;
        u8 seconds = 0;
        f64 cog_rad = 0.0;
        f64 sog_mps = 0.0;
        AISTransceiverInfo transceiver = AISTransceiverInfo::ChannelA_VDL_Rx;
        f64 heading_rad = 0.0;
        AISUnit unit = AISUnit::ClassB_CS;
        AISMode mode = AISMode::Autonomous;
    };

    // ─── AIS Static Data ──────────────────────────────────────────────────────
    struct AISStaticData {
        u8 message_id = 0;
        AISRepeat repeat = AISRepeat::Initial;
        u32 user_id = 0;
        u32 imo_number = 0;
        dp::String callsign;
        dp::String name;
        u8 vessel_type = 0;
        f64 length_m = 0.0;
        f64 beam_m = 0.0;
        f64 pos_ref_starboard_m = 0.0;
        f64 pos_ref_bow_m = 0.0;
        u16 eta_date = 0;
        f64 eta_time = 0.0;
        f64 draught_m = 0.0;
        dp::String destination;
        AISDTE dte = AISDTE::NotReady;
    };

    // ─── Trim Tab ─────────────────────────────────────────────────────────────
    struct TrimTabData {
        i8 port_pct = 0;      // -100 to +100
        i8 starboard_pct = 0; // -100 to +100
    };

    // ─── Direction Data ───────────────────────────────────────────────────────
    struct DirectionData {
        DataMode data_mode = DataMode::Autonomous;
        HeadingReference cog_reference = HeadingReference::Unavailable;
        u8 sid = 0xFF;
        f64 cog_rad = 0.0;
        f64 sog_mps = 0.0;
        f64 heading_rad = 0.0;
        f64 speed_through_water_mps = 0.0;
        f64 set_rad = 0.0;   // current set direction
        f64 drift_mps = 0.0; // current drift speed
    };

    // ─── Meteorological Station Data ──────────────────────────────────────────
    struct MeteorologicalData {
        u8 sid = 0xFF;
        u16 date = 0;
        f64 time = 0.0;
        f64 latitude = 0.0;
        f64 longitude = 0.0;
        f64 wind_speed_mps = 0.0;
        f64 wind_dir_rad = 0.0;
        WindReference wind_reference = WindReference::Unavailable;
        f64 wind_gusts_mps = 0.0;
        f64 atmospheric_pressure_pa = 0.0;
        f64 ambient_temperature_k = 0.0;
        dp::String station_id;
        dp::String station_name;
    };

    // ─── Converter Status ─────────────────────────────────────────────────────
    struct ConverterStatusData {
        u8 sid = 0xFF;
        u8 connection_number = 0;
        ConverterMode operating_state = ConverterMode::NotAvailable;
        u8 charge_mode = 0;
    };

} // namespace agrobus::nmea
