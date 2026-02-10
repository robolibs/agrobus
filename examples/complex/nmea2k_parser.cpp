// NMEA2000 Message Parser/Listener Example
//
// Comprehensive demonstration of subscribing to all NMEA2000 message types
// through the NMEAInterface class. Shows how to decode position, navigation,
// environmental, and engine data from a CAN bus carrying NMEA2000 traffic.
//
// Key concepts:
//   - Configuring NMEAInterface with NMEAConfig to enable all message listeners
//   - Subscribing to typed events for each PGN category
//   - Converting raw NMEA2000 data to engineering units
//   - Coordinate frame conversions (WGS84 -> ENU/NED)
//   - Polling latest cached position from the interface

#include <agrobus/net/network_manager.hpp>
#include <agrobus/nmea/interface.hpp>
#include <echo/echo.hpp>

using namespace agrobus::net;
using namespace agrobus::nmea;

// ─── Unit Conversion Helpers ─────────────────────────────────────────────────
// These functions convert raw NMEA2000 values (SI units) to more human-readable
// engineering units commonly used in navigation and marine applications.

static constexpr f64 RAD_TO_DEG = 180.0 / 3.14159265358979323846;
static constexpr f64 MPS_TO_KNOTS = 1.94384;
static constexpr f64 MPS_TO_KMH = 3.6;
static constexpr f64 PA_TO_KPA = 0.001;

static f64 rad_to_deg(f64 rad) { return rad * RAD_TO_DEG; }
static f64 mps_to_knots(f64 mps) { return mps * MPS_TO_KNOTS; }
static f64 mps_to_kmh(f64 mps) { return mps * MPS_TO_KMH; }
static f64 kelvin_to_celsius(f64 k) { return k - KELVIN_OFFSET; }
static f64 pa_to_kpa(f64 pa) { return pa * PA_TO_KPA; }

// ─── Fix Type Description ────────────────────────────────────────────────────

static const char* fix_type_str(GNSSFixType type) {
    switch (type) {
        case GNSSFixType::NoFix:      return "No Fix";
        case GNSSFixType::GNSSFix:    return "GNSS";
        case GNSSFixType::DGNSSFix:   return "DGNSS";
        case GNSSFixType::PreciseGNSS: return "Precise";
        case GNSSFixType::RTKFixed:   return "RTK Fixed";
        case GNSSFixType::RTKFloat:   return "RTK Float";
        case GNSSFixType::DeadReckon: return "Dead Reckoning";
        default:                      return "Unknown";
    }
}

// ─── Wind Reference Description ──────────────────────────────────────────────

static const char* wind_reference_str(WindReference ref) {
    switch (ref) {
        case WindReference::TrueNorth:  return "True (ground)";
        case WindReference::Magnetic:   return "Magnetic";
        case WindReference::Apparent:   return "Apparent";
        case WindReference::TrueBoat:   return "True (boat)";
        case WindReference::TrueWater:  return "True (water)";
        default:                        return "Unknown";
    }
}

// ─── Time Source Description ─────────────────────────────────────────────────

static const char* time_source_str(TimeSource src) {
    switch (src) {
        case TimeSource::GPS:           return "GPS";
        case TimeSource::GLONASS:       return "GLONASS";
        case TimeSource::RadioStation:  return "Radio Station";
        case TimeSource::LocalCesiumClock:   return "Local Cesium";
        case TimeSource::LocalRubidiumClock: return "Local Rubidium";
        case TimeSource::LocalCrystalClock:  return "Local Crystal";
        default:                        return "Unknown";
    }
}

// ─── Temperature Source Description ──────────────────────────────────────────

static const char* temperature_source_str(TemperatureSource src) {
    switch (src) {
        case TemperatureSource::Sea:           return "Sea Water";
        case TemperatureSource::Outside:       return "Outside Air";
        case TemperatureSource::Inside:        return "Inside Air";
        case TemperatureSource::EngineRoom:    return "Engine Room";
        case TemperatureSource::MainCabin:     return "Main Cabin";
        case TemperatureSource::LiveWell:      return "Live Well";
        case TemperatureSource::BaitWell:      return "Bait Well";
        case TemperatureSource::Refrigeration: return "Refrigerator";
        case TemperatureSource::HeatingSystem: return "Heating System";
        default:                               return "Unknown";
    }
}

int main() {
    echo::info("=== NMEA2000 Message Parser Demo ===");
    echo::info("Demonstrates subscribing to all NMEA2000 message types");
    echo::info("");

    // ─── Network Setup ───────────────────────────────────────────────────────
    // Create a network manager and an internal control function.
    // In a real application, you would attach a CAN driver (SocketCAN, SPI, etc.)
    // before calling nm.update() in the main loop.

    IsoNet nm;
    Name name = Name::build()
        .set_identity_number(100)
        .set_manufacturer_code(999)
        .set_function_code(130)     // NMEA2000 display/gateway function
        .set_device_class(25)       // Inter/Intra network device
        .set_industry_group(4);     // Marine

    auto cf_result = nm.create_internal(name, 0, 0x50);
    auto* cf = cf_result.value();

    // ─── NMEA Interface Configuration ────────────────────────────────────────
    // Use NMEAConfig::all(true) to enable listeners for every supported PGN.
    // Individual listeners can also be toggled, e.g.:
    //   NMEAConfig{}.rapid_position(true).cog_sog(true).wind(true)

    NMEAConfig config;
    config.all(true);  // Enable: position, COG/SOG, attitude, wind, temperature,
                       //         engine, depth, heading, system time, rate of turn

    NMEAInterface nmea(nm, cf, config);
    nmea.initialize();

    echo::info("NMEAInterface initialized with all listeners enabled.");
    echo::info("");

    // ─── Reference Point for Coordinate Conversions ──────────────────────────
    // Define a geographic reference point for ENU/NED frame conversions.
    // This could be a dock, marina entrance, or field corner.

    dp::Geo reference{47.6062, -122.3321};  // Seattle, WA as example reference
    echo::info("Reference point: lat=", reference.latitude,
               " lon=", reference.longitude);
    echo::info("");

    // ═══════════════════════════════════════════════════════════════════════════
    // EVENT SUBSCRIPTIONS
    // Each on_* event fires when the corresponding PGN is received and decoded.
    // ═══════════════════════════════════════════════════════════════════════════

    // ─── 1. Position Updates (PGN 129025 / 129029) ───────────────────────────
    // GNSSPosition contains WGS84 coordinates, fix quality, and satellite info.
    // The rapid update (129025) provides lat/lon at 10 Hz.
    // The detailed update (129029) adds altitude, HDOP, PDOP, and satellite count.

    nmea.on_position.subscribe([&](const GNSSPosition& pos) {
        echo::info("[POSITION] ──────────────────────────────────────");
        echo::info("  WGS84: lat=", pos.wgs.latitude, " lon=", pos.wgs.longitude);

        if (pos.altitude_m) {
            echo::info("  Altitude: ", *pos.altitude_m, " m");
        }

        echo::info("  Fix type: ", fix_type_str(pos.fix_type),
                   " (", static_cast<int>(pos.fix_type), ")");
        echo::info("  Satellites: ", static_cast<int>(pos.satellites_used));

        if (pos.hdop) {
            echo::info("  HDOP: ", *pos.hdop);
        }
        if (pos.pdop) {
            echo::info("  PDOP: ", *pos.pdop);
        }

        // Quality indicators
        echo::info("  Has fix: ", pos.has_fix() ? "yes" : "no");
        echo::info("  Is RTK:  ", pos.is_rtk() ? "yes" : "no");

        // ── Coordinate Frame Conversions ──
        // Convert WGS84 to local East-North-Up (ENU) frame relative to reference.
        // Useful for local navigation, path planning, and distance calculations.
        auto enu = pos.to_enu(reference);
        echo::info("  ENU (from ref): east=", enu.east(), " m, north=", enu.north(),
                   " m, up=", enu.up(), " m");

        // Convert to North-East-Down (NED) frame, common in aviation/marine.
        // NED is the standard frame for autopilot systems.
        auto ned = pos.to_ned(reference);
        echo::info("  NED (from ref): north=", ned.north(), " m, east=", ned.east(),
                   " m, down=", ned.down(), " m");

        echo::info("");
    });

    // ─── 2. Course Over Ground (PGN 129026) ─────────────────────────────────
    // COG is the direction of travel relative to true north, in radians.
    // This is derived from successive position fixes, not from a compass.

    nmea.on_cog.subscribe([](f64 cog_rad) {
        echo::info("[COG] Course over ground: ", rad_to_deg(cog_rad), " deg (",
                   cog_rad, " rad)");
    });

    // ─── 3. Speed Over Ground (PGN 129026) ──────────────────────────────────
    // SOG is the scalar speed derived from successive GNSS positions, in m/s.

    nmea.on_sog.subscribe([](f64 sog_mps) {
        echo::info("[SOG] Speed over ground: ", sog_mps, " m/s | ",
                   mps_to_kmh(sog_mps), " km/h | ",
                   mps_to_knots(sog_mps), " knots");
    });

    // ─── 4. Attitude (PGN 127257) ───────────────────────────────────────────
    // Yaw, pitch, and roll angles from an IMU or AHRS, in radians.
    // Yaw is relative to true north. Pitch is nose-up positive.
    // Roll is starboard-down positive.

    nmea.on_attitude.subscribe([](f64 yaw, f64 pitch, f64 roll) {
        echo::info("[ATTITUDE] ──────────────────────────────────────");
        echo::info("  Yaw:   ", rad_to_deg(yaw), " deg (", yaw, " rad)");
        echo::info("  Pitch: ", rad_to_deg(pitch), " deg (", pitch, " rad)");
        echo::info("  Roll:  ", rad_to_deg(roll), " deg (", roll, " rad)");
        echo::info("");
    });

    // ─── 5. Wind Data (PGN 130306) ──────────────────────────────────────────
    // Wind speed and direction from an anemometer.
    // Reference field indicates: 0=True (ground ref), 1=Magnetic, 2=Apparent.
    // Apparent wind is relative to the vessel's motion.

    nmea.on_wind.subscribe([](const WindData& wind) {
        echo::info("[WIND] ──────────────────────────────────────────");
        echo::info("  Speed: ", wind.speed_mps, " m/s (",
                   mps_to_knots(wind.speed_mps), " knots)");
        echo::info("  Direction: ", rad_to_deg(wind.direction_rad), " deg");
        echo::info("  Reference: ", wind_reference_str(wind.reference));
        echo::info("");
    });

    // ─── 6. Temperature (PGN 130311) ────────────────────────────────────────
    // Temperature readings from various sensors, reported in Kelvin.
    // The source field identifies what is being measured (sea, air, engine, etc.).
    // Instance distinguishes multiple sensors of the same type.

    nmea.on_temperature.subscribe([](const TemperatureData& temp) {
        echo::info("[TEMPERATURE] ───────────────────────────────────");
        echo::info("  Value: ", kelvin_to_celsius(temp.actual_k), " C (",
                   temp.actual_k, " K)");
        echo::info("  Instance: ", static_cast<int>(temp.instance));
        echo::info("  Source: ", temperature_source_str(temp.source),
                   " (", static_cast<int>(static_cast<u8>(temp.source)), ")");
        echo::info("");
    });

    // ─── 7. Engine Parameters (PGN 127488) ──────────────────────────────────
    // Rapid engine data: RPM, boost pressure, and tilt/trim.
    // Instance field distinguishes port/starboard or multiple engines.

    nmea.on_engine.subscribe([](const EngineData& engine) {
        echo::info("[ENGINE] ────────────────────────────────────────");
        echo::info("  Instance: ", static_cast<int>(engine.instance),
                   (engine.instance == 0 ? " (single/port)" : " (starboard)"));
        echo::info("  RPM: ", engine.rpm);
        echo::info("  Boost pressure: ", pa_to_kpa(engine.boost_pressure_pa), " kPa (",
                   engine.boost_pressure_pa, " Pa)");
        echo::info("");
    });

    // ─── 8. Water Depth (PGN 128267) ────────────────────────────────────────
    // Depth from the transducer to the bottom, in meters.
    // The offset field indicates transducer position relative to the waterline:
    //   positive = below waterline, negative = above (gives depth below keel).

    nmea.on_depth.subscribe([](const WaterDepthData& depth) {
        echo::info("[DEPTH] ─────────────────────────────────────────");
        echo::info("  Depth (transducer): ", depth.depth_m, " m");
        echo::info("  Transducer offset: ", depth.offset_m, " m");

        // Calculate depth below waterline (surface)
        f64 depth_below_surface = depth.depth_m + depth.offset_m;
        echo::info("  Depth (surface): ", depth_below_surface, " m");
        echo::info("");
    });

    // ─── 9. Heading (PGN 127250) ────────────────────────────────────────────
    // Vessel heading from a compass or GNSS compass, in radians relative to
    // true or magnetic north. Unlike COG, heading is the direction the vessel
    // is pointed, not the direction it is moving.

    nmea.on_heading.subscribe([](f64 heading_rad) {
        echo::info("[HEADING] Vessel heading: ", rad_to_deg(heading_rad), " deg (",
                   heading_rad, " rad)");
    });

    // ─── 10. System Time (PGN 126992) ───────────────────────────────────────
    // UTC time from the GNSS receiver or other time source.
    // days_since_epoch: days since 1970-01-01 (Unix epoch).
    // seconds_since_midnight: fractional seconds since 00:00:00 UTC.

    nmea.on_system_time.subscribe([](const SystemTimeData& time) {
        echo::info("[TIME] ──────────────────────────────────────────");
        echo::info("  Source: ", time_source_str(time.source),
                   " (", static_cast<int>(static_cast<u8>(time.source)), ")");
        echo::info("  Days since epoch: ", time.days_since_epoch);

        // Convert seconds_since_midnight to HH:MM:SS
        u32 total_seconds = static_cast<u32>(time.seconds_since_midnight);
        u32 hours = total_seconds / 3600;
        u32 minutes = (total_seconds % 3600) / 60;
        u32 seconds = total_seconds % 60;
        echo::info("  UTC time: ", hours, ":", minutes, ":", seconds,
                   " (", time.seconds_since_midnight, " s since midnight)");
        echo::info("");
    });

    echo::info("All event handlers registered. Ready to receive NMEA2000 data.");
    echo::info("────────────────────────────────────────────────────────────────");
    echo::info("");

    // ═══════════════════════════════════════════════════════════════════════════
    // MONITORING LOOP
    // Periodically poll the cached latest_position() and print a summary.
    // In a real application, nm.update() dispatches incoming CAN frames and
    // triggers the event callbacks above.
    // ═══════════════════════════════════════════════════════════════════════════

    u32 iteration = 0;
    constexpr u32 SUMMARY_INTERVAL = 100;  // Print summary every N iterations
    constexpr u32 MAX_ITERATIONS = 1000;   // Run for a bounded number of cycles

    echo::info("Starting monitoring loop (", MAX_ITERATIONS, " iterations, summary every ",
               SUMMARY_INTERVAL, ")...");
    echo::info("");

    while (iteration < MAX_ITERATIONS) {
        // Process incoming CAN frames. The 10ms timeout means this call blocks
        // for at most 10ms waiting for new data before returning.
        nm.update(10);

        // Periodically print a position summary from the cached value.
        // latest_position() returns the most recent position without waiting
        // for a new event -- useful for control loops that need position data
        // at a fixed rate regardless of GNSS update rate.
        if (iteration % SUMMARY_INTERVAL == 0) {
            echo::info("── Iteration ", iteration, " ── Position Summary ──");

            auto pos_opt = nmea.latest_position();
            if (pos_opt) {
                const auto& pos = *pos_opt;
                echo::info("  Lat: ", pos.wgs.latitude, "  Lon: ", pos.wgs.longitude);

                if (pos.altitude_m) {
                    echo::info("  Alt: ", *pos.altitude_m, " m");
                }

                echo::info("  Fix: ", fix_type_str(pos.fix_type),
                           "  Sats: ", static_cast<int>(pos.satellites_used));

                if (pos.has_fix()) {
                    // Compute distance from reference in ENU frame
                    auto enu = pos.to_enu(reference);
                    f64 horiz_dist = std::sqrt(enu.east() * enu.east() +
                                               enu.north() * enu.north());
                    echo::info("  Distance from reference: ", horiz_dist, " m (horizontal)");
                }

                if (pos.speed_mps) {
                    echo::info("  SOG: ", mps_to_kmh(*pos.speed_mps), " km/h");
                }
                if (pos.cog_rad) {
                    echo::info("  COG: ", rad_to_deg(*pos.cog_rad), " deg");
                }
                if (pos.heading_rad) {
                    echo::info("  Heading: ", rad_to_deg(*pos.heading_rad), " deg");
                }
            } else {
                echo::info("  No position data received yet.");
            }

            echo::info("");
        }

        ++iteration;
    }

    // ─── Final Summary ───────────────────────────────────────────────────────

    echo::info("═══════════════════════════════════════════════════════════════");
    echo::info("Monitoring complete after ", MAX_ITERATIONS, " iterations.");

    auto final_pos = nmea.latest_position();
    if (final_pos) {
        echo::info("Last known position:");
        echo::info("  ", final_pos->wgs.latitude, ", ", final_pos->wgs.longitude);
        echo::info("  Fix: ", fix_type_str(final_pos->fix_type));
        echo::info("  RTK: ", final_pos->is_rtk() ? "yes" : "no");

        if (final_pos->hdop) {
            echo::info("  HDOP: ", *final_pos->hdop,
                       ((*final_pos->hdop < 1.0) ? " (excellent)" :
                        (*final_pos->hdop < 2.0) ? " (good)" :
                        (*final_pos->hdop < 5.0) ? " (moderate)" : " (poor)"));
        }
    } else {
        echo::info("No position data was received during this session.");
    }

    echo::info("═══════════════════════════════════════════════════════════════");
    return 0;
}
