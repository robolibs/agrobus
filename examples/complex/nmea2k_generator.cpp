// NMEA2000 Message Generator Demo
//
// Demonstrates sending various NMEA2000 messages using the NMEAInterface class.
// Simulates a multi-sensor marine/agricultural system generating position, speed,
// wind, temperature, engine, depth, heading, and system time data at realistic rates.

#include <agrobus/net/network_manager.hpp>
#include <agrobus/nmea/interface.hpp>
#include <concord/concord.hpp>
#include <echo/echo.hpp>
#include <csignal>
#include <atomic>
#include <cmath>

using namespace agrobus::net;
using namespace agrobus::nmea;

static std::atomic<bool> running{true};
void signal_handler(int) { running = false; }

int main() {
    echo::info("=== NMEA2000 Message Generator ===");

    // --- Network setup ---
    IsoNet nm;
    Name name = Name::build()
        .set_identity_number(100)
        .set_manufacturer_code(55)
        .set_function_code(130)  // Navigation
        .set_device_class(25)    // Inter/Intra network device
        .set_industry_group(4)   // Marine
        .set_self_configurable(true);

    auto cf_result = nm.create_internal(name, 0, 0x20);
    if (!cf_result.is_ok()) {
        echo::error("Failed to create control function: ", cf_result.error().message);
        return 1;
    }
    auto* cf = cf_result.value();

    // Enable all NMEA2000 message types
    NMEAConfig config = NMEAConfig{}
        .all(true);

    NMEAInterface nmea(nm, cf, config);
    auto init_result = nmea.initialize();
    if (!init_result.is_ok()) {
        echo::error("NMEA interface init failed: ", init_result.error().message);
        return 1;
    }

    echo::info("NMEA2000 interface initialized with all message types enabled");

    // --- Simulation parameters ---

    // Base position: near a field (New York area coordinates)
    constexpr f64 BASE_LAT = 40.7128;
    constexpr f64 BASE_LON = -74.0060;
    constexpr f64 BASE_ALT = 12.0;  // meters above sea level

    // Vehicle dynamics
    f64 heading_rad = 0.0;               // Start heading north
    constexpr f64 HEADING_RATE = 0.005;  // Slow clockwise turn (rad per 100ms tick)
    constexpr f64 VEHICLE_SPEED = 3.0;   // m/s (~10.8 km/h, typical field speed)

    // Position tracking (dead reckoning from base)
    f64 lat_offset = 0.0;
    f64 lon_offset = 0.0;

    // Wind parameters
    constexpr f64 WIND_SPEED = 4.5;       // m/s
    constexpr f64 WIND_DIR = 1.2;         // radians (~69 degrees, ENE)

    // Temperature (ambient, in Kelvin)
    constexpr f64 AMBIENT_TEMP_K = 295.15;  // ~22 C

    // Engine parameters
    constexpr f64 ENGINE_RPM = 2200.0;
    constexpr f64 BOOST_PRESSURE = 120000.0;  // Pa (~1.2 bar)
    constexpr i8 TILT_TRIM = 0;

    // Water depth (for demonstration)
    constexpr f64 WATER_DEPTH = 3.5;   // meters
    constexpr f64 XDUCER_OFFSET = 0.3; // transducer offset below waterline

    // Deviation/variation for heading
    constexpr f64 MAGNETIC_DEVIATION = 0.01;   // radians
    constexpr f64 MAGNETIC_VARIATION = -0.23;  // radians (~-13 degrees, NYC area)

    // System time: days since epoch for 2024-06-15
    constexpr u16 DAYS_SINCE_EPOCH = 19889;
    f64 time_of_day_s = 36000.0;  // Start at 10:00:00 UTC

    // --- Timing counters ---
    u64 tick = 0;
    constexpr u64 TICK_MS = 10;  // 10ms base loop period (100Hz)

    // Message intervals in ticks
    constexpr u64 POSITION_INTERVAL = 10;     // 100ms -> 10Hz
    constexpr u64 COG_SOG_INTERVAL = 10;      // 100ms -> 10Hz
    constexpr u64 HEADING_INTERVAL = 10;      // 100ms -> 10Hz
    constexpr u64 ENGINE_INTERVAL = 10;       // 100ms -> 10Hz
    constexpr u64 WIND_INTERVAL = 100;        // 1000ms -> 1Hz
    constexpr u64 SYSTEM_TIME_INTERVAL = 100; // 1000ms -> 1Hz
    constexpr u64 TEMPERATURE_INTERVAL = 200; // 2000ms -> 0.5Hz
    constexpr u64 DEPTH_INTERVAL = 100;       // 1000ms -> 1Hz

    // SID counter for sequence identification
    u8 sid = 0;

    signal(SIGINT, signal_handler);

    echo::info("Generating NMEA2000 messages:");
    echo::info("  Position:    10 Hz (every 100ms)");
    echo::info("  COG/SOG:     10 Hz (every 100ms)");
    echo::info("  Heading:     10 Hz (every 100ms)");
    echo::info("  Engine:      10 Hz (every 100ms)");
    echo::info("  Wind:         1 Hz (every 1000ms)");
    echo::info("  System Time:  1 Hz (every 1000ms)");
    echo::info("  Temperature:  0.5 Hz (every 2000ms)");
    echo::info("  Water Depth:  1 Hz (every 1000ms)");
    echo::info("");
    echo::info("Press Ctrl+C to stop.");
    echo::info("");

    u64 msg_count = 0;

    while (running) {
        // --- Update simulation state ---

        // Slowly rotate heading
        heading_rad += HEADING_RATE;
        if (heading_rad > 2.0 * M_PI) {
            heading_rad -= 2.0 * M_PI;
        }

        // Update position based on heading and speed
        // Approximate meters to degrees at this latitude
        constexpr f64 DEG_PER_M_LAT = 1.0 / 111320.0;
        f64 deg_per_m_lon = 1.0 / (111320.0 * std::cos(BASE_LAT * M_PI / 180.0));

        f64 dx = VEHICLE_SPEED * std::sin(heading_rad) * (TICK_MS / 1000.0);
        f64 dy = VEHICLE_SPEED * std::cos(heading_rad) * (TICK_MS / 1000.0);
        lon_offset += dx * deg_per_m_lon;
        lat_offset += dy * DEG_PER_M_LAT;

        // Advance time of day
        time_of_day_s += TICK_MS / 1000.0;
        if (time_of_day_s >= 86400.0) {
            time_of_day_s -= 86400.0;
        }

        // --- Send messages at their respective rates ---

        // Position Rapid Update (10Hz)
        if (tick % POSITION_INTERVAL == 0) {
            GNSSPosition pos;
            pos.wgs = concord::earth::WGS(BASE_LAT + lat_offset, BASE_LON + lon_offset);
            pos.altitude_m = BASE_ALT;
            pos.heading_rad = heading_rad;
            pos.speed_mps = VEHICLE_SPEED;
            pos.fix_type = GNSSFixType::RTKFixed;
            pos.satellites_used = 16;

            auto result = nmea.send_position(pos);
            if (result.is_ok()) {
                ++msg_count;
            } else {
                echo::error("send_position failed: ", result.error().message);
            }
        }

        // COG/SOG Rapid Update (10Hz)
        if (tick % COG_SOG_INTERVAL == 0) {
            f64 cog = heading_rad;  // COG matches heading in steady-state
            f64 sog = VEHICLE_SPEED;

            auto result = nmea.send_cog_sog(cog, sog);
            if (result.is_ok()) {
                ++msg_count;
            }
        }

        // Heading (10Hz)
        if (tick % HEADING_INTERVAL == 0) {
            auto result = nmea.send_heading(heading_rad, MAGNETIC_DEVIATION, MAGNETIC_VARIATION);
            if (result.is_ok()) {
                ++msg_count;
            }
        }

        // Engine Parameters Rapid (10Hz)
        if (tick % ENGINE_INTERVAL == 0) {
            // Add slight RPM variation for realism
            f64 rpm_noise = 20.0 * std::sin(tick * 0.01);
            EngineData engine;
            engine.instance = 0;
            engine.rpm = ENGINE_RPM + rpm_noise;
            engine.boost_pressure_pa = BOOST_PRESSURE;
            engine.tilt_trim = TILT_TRIM;

            auto result = nmea.send_engine_params(engine);
            if (result.is_ok()) {
                ++msg_count;
            }
        }

        // Wind Data (1Hz)
        if (tick % WIND_INTERVAL == 0) {
            // Slight wind variation
            f64 gust = 0.5 * std::sin(tick * 0.002);
            WindData wind;
            wind.sid = sid;
            wind.speed_mps = WIND_SPEED + gust;
            wind.direction_rad = WIND_DIR;
            wind.reference = WindReference::TrueNorth;

            auto result = nmea.send_wind(wind);
            if (result.is_ok()) {
                ++msg_count;
            }
        }

        // System Time (1Hz)
        if (tick % SYSTEM_TIME_INTERVAL == 0) {
            SystemTimeData sys_time;
            sys_time.sid = sid;
            sys_time.source = TimeSource::GPS;
            sys_time.days_since_epoch = DAYS_SINCE_EPOCH;
            sys_time.seconds_since_midnight = time_of_day_s;

            auto result = nmea.send_system_time(sys_time);
            if (result.is_ok()) {
                ++msg_count;
            }
        }

        // Temperature (0.5Hz)
        if (tick % TEMPERATURE_INTERVAL == 0) {
            // Slight temperature drift
            f64 temp_drift = 0.1 * std::sin(tick * 0.0005);
            TemperatureData temp;
            temp.sid = sid;
            temp.instance = 0;
            temp.source = TemperatureSource::Outside;
            temp.actual_k = AMBIENT_TEMP_K + temp_drift;

            auto result = nmea.send_temperature(temp);
            if (result.is_ok()) {
                ++msg_count;
            }
        }

        // Water Depth (1Hz)
        if (tick % DEPTH_INTERVAL == 0) {
            // Simulate gentle depth variation
            f64 depth_var = 0.2 * std::sin(tick * 0.003);
            WaterDepthData depth;
            depth.sid = sid;
            depth.depth_m = WATER_DEPTH + depth_var;
            depth.offset_m = XDUCER_OFFSET;

            auto result = nmea.send_depth(depth);
            if (result.is_ok()) {
                ++msg_count;
            }
        }

        // --- Periodic status output (every 5 seconds) ---
        if (tick % 500 == 0 && tick > 0) {
            f64 elapsed_s = static_cast<f64>(tick * TICK_MS) / 1000.0;
            echo::info("[", elapsed_s, "s] msgs=", msg_count,
                       " pos=(", BASE_LAT + lat_offset, ", ", BASE_LON + lon_offset, ")",
                       " hdg=", heading_rad * 180.0 / M_PI, " deg",
                       " rpm=", ENGINE_RPM + 20.0 * std::sin(tick * 0.01));
        }

        // Increment SID (wraps at 252, per NMEA2000 spec)
        if (tick % 10 == 0) {
            sid = (sid >= 252) ? 0 : sid + 1;
        }

        ++tick;
        usleep(TICK_MS * 1000);
    }

    // --- Summary ---
    f64 total_s = static_cast<f64>(tick * TICK_MS) / 1000.0;
    echo::info("");
    echo::info("=== Generator Stopped ===");
    echo::info("Runtime: ", total_s, " seconds");
    echo::info("Messages sent: ", msg_count);
    if (total_s > 0.0) {
        echo::info("Average rate: ", static_cast<f64>(msg_count) / total_s, " msg/s");
    }
    echo::info("Final position: (", BASE_LAT + lat_offset, ", ", BASE_LON + lon_offset, ")");
    echo::info("Final heading: ", heading_rad * 180.0 / M_PI, " degrees");

    return 0;
}
