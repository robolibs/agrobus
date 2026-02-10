#include <agrobus/nmea/position.hpp>
#include <echo/echo.hpp>
#include <cmath>

using namespace agrobus::nmea;

int main() {
    echo::info("=== GNSS Batch Conversion Demo ===");

    // Simulated field reference point
    dp::Geo field_origin{48.1234, 11.5678, 450.0};
    echo::info("Reference: lat=", field_origin.latitude, " lon=", field_origin.longitude);

    // Generate a simulated path (e.g., a tractor driving north in a field)
    GNSSBatch batch;
    constexpr i32 NUM_POINTS = 50;
    constexpr f64 STEP_M = 2.0; // 2 meters between points

    echo::info("Generating ", NUM_POINTS, " waypoints (", STEP_M, "m spacing)...");

    for (i32 i = 0; i < NUM_POINTS; ++i) {
        GNSSPosition pos;
        // Move north: ~0.000009 deg per meter at 48 deg latitude
        f64 north_offset = i * STEP_M * 0.000009;
        // Slight east drift (simulating imperfect guidance)
        f64 east_offset = 0.5 * std::sin(i * 0.2) * 0.0000134;

        pos.wgs = concord::earth::WGS(
            field_origin.latitude + north_offset,
            field_origin.longitude + east_offset,
            field_origin.altitude
        );
        pos.fix_type = GNSSFixType::RTKFixed;
        pos.satellites_used = 18;
        batch.positions.push_back(pos);
    }

    // Batch convert to ENU
    echo::info("\n--- ENU Batch Conversion ---");
    auto enu_results = batch.to_enu_batch(field_origin);
    echo::info("Converted ", enu_results.size(), " positions to ENU");

    for (usize i = 0; i < enu_results.size(); i += 10) {
        echo::info("  [", i, "] E=", enu_results[i].east(),
                   " N=", enu_results[i].north(), " U=", enu_results[i].up());
    }

    // Batch convert to NED
    echo::info("\n--- NED Batch Conversion ---");
    auto ned_results = batch.to_ned_batch(field_origin);
    echo::info("Converted ", ned_results.size(), " positions to NED");

    for (usize i = 0; i < ned_results.size(); i += 10) {
        echo::info("  [", i, "] N=", ned_results[i].north(),
                   " E=", ned_results[i].east(), " D=", ned_results[i].down());
    }

    // Batch convert to ECEF
    echo::info("\n--- ECEF Batch Conversion ---");
    auto ecf_results = batch.to_ecf_batch();
    echo::info("Converted ", ecf_results.size(), " positions to ECEF");

    // Compute distances between consecutive ECEF points
    echo::info("\n--- Inter-point Distances ---");
    f64 total_distance = 0.0;
    for (usize i = 1; i < ecf_results.size(); ++i) {
        f64 dx = ecf_results[i].x - ecf_results[i - 1].x;
        f64 dy = ecf_results[i].y - ecf_results[i - 1].y;
        f64 dz = ecf_results[i].z - ecf_results[i - 1].z;
        f64 dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        total_distance += dist;
    }
    echo::info("Total path length: ", total_distance, " m");
    echo::info("Average spacing: ", total_distance / (NUM_POINTS - 1), " m");

    // Cross-track error analysis using ENU
    echo::info("\n--- Cross-Track Analysis ---");
    f64 max_cross_track = 0.0;
    f64 sum_cross_track = 0.0;
    for (const auto& pt : enu_results) {
        f64 ct = std::abs(pt.east());
        max_cross_track = std::max(max_cross_track, ct);
        sum_cross_track += ct;
    }
    echo::info("Max cross-track error: ", max_cross_track, " m");
    echo::info("Avg cross-track error: ", sum_cross_track / enu_results.size(), " m");

    echo::info("\nDone.");
    return 0;
}
