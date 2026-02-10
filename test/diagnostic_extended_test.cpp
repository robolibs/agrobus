#include <agrobus.hpp>
#include <echo/echo.hpp>

using namespace agrobus::net;
using namespace agrobus::j1939;

// ─── Test Framework ──────────────────────────────────────────────────────────
#define TEST(name) void test_##name()
#define RUN_TEST(name)                                                                                                 \
    do {                                                                                                               \
        echo::info("Running test: " #name);                                                                            \
        test_##name();                                                                                                 \
        echo::info("✓ " #name " passed");                                                                              \
    } while (0)

#define ASSERT(condition)                                                                                              \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            echo::error("Assertion failed: " #condition " at line ", __LINE__);                                        \
            std::exit(1);                                                                                              \
        }                                                                                                              \
    } while (0)

#define ASSERT_EQ(a, b)                                                                                                \
    do {                                                                                                               \
        if ((a) != (b)) {                                                                                              \
            echo::error("Assertion failed: " #a " == " #b " at line ", __LINE__);                                     \
            std::exit(1);                                                                                              \
        }                                                                                                              \
    } while (0)

// ─── DM25 Freeze Frame Tests ─────────────────────────────────────────────────

TEST(freeze_frame_automatic_capture) {
    IsoNet net;
    Name name = Name::build().set_identity_number(1).set_manufacturer_code(1).set_function_code(0).set_industry_group(
        2);
    auto cf = net.create_internal(name, 0, 0x20).value();

    DiagnosticConfig config;
    config.auto_capture_freeze_frames_enabled(true);

    DiagnosticProtocol diag(net, cf, config);
    diag.initialize();

    // Activate DTC - should auto-capture freeze frame
    DTC dtc{.spn = 110, .fmi = FMI::AboveNormal};
    diag.set_active(dtc);

    // Verify freeze frame was captured
    auto ff = diag.get_freeze_frame(110, FMI::AboveNormal, 0);
    ASSERT(ff.has_value());
    ASSERT_EQ(ff.value().dtc.spn, 110);
    ASSERT_EQ(static_cast<u32>(ff.value().dtc.fmi), static_cast<u32>(FMI::AboveNormal));
}

TEST(freeze_frame_manual_capture) {
    IsoNet net;
    Name name = Name::build().set_identity_number(2).set_manufacturer_code(1).set_function_code(0).set_industry_group(
        2);
    auto cf = net.create_internal(name, 0, 0x21).value();

    DiagnosticProtocol diag(net, cf);
    diag.initialize();

    // Manual capture with snapshots
    DTC dtc{.spn = 94, .fmi = FMI::BelowNormal};
    dp::Vector<SPNSnapshot> snapshots;
    snapshots.push_back({.spn = 91, .value = 2500});
    snapshots.push_back({.spn = 94, .value = 15000});
    snapshots.push_back({.spn = 110, .value = 95});

    diag.capture_freeze_frame(dtc, snapshots, 12345);

    // Verify freeze frame
    auto ff = diag.get_freeze_frame(94, FMI::BelowNormal, 0);
    ASSERT(ff.has_value());
    ASSERT_EQ(ff.value().snapshots.size(), 3);
    ASSERT_EQ(ff.value().snapshots[0].spn, 91);
    ASSERT_EQ(ff.value().snapshots[0].value, 2500);
    ASSERT_EQ(ff.value().timestamp_ms, 12345);
}

TEST(freeze_frame_depth_limiting) {
    IsoNet net;
    Name name = Name::build().set_identity_number(3).set_manufacturer_code(1).set_function_code(0).set_industry_group(
        2);
    auto cf = net.create_internal(name, 0, 0x22).value();

    DiagnosticConfig config;
    config.freeze_frame_depth(3); // Max 3 frames

    DiagnosticProtocol diag(net, cf, config);
    diag.initialize();

    // Capture 5 frames
    DTC dtc{.spn = 412, .fmi = FMI::AboveNormal};
    for (u32 i = 0; i < 5; ++i) {
        dp::Vector<SPNSnapshot> snapshots;
        snapshots.push_back({.spn = 412, .value = 100 + i});
        diag.capture_freeze_frame(dtc, snapshots, i * 1000);
    }

    // Should have max 3 frames (oldest 2 dropped)
    auto ff0 = diag.get_freeze_frame(412, FMI::AboveNormal, 0);
    auto ff1 = diag.get_freeze_frame(412, FMI::AboveNormal, 1);
    auto ff2 = diag.get_freeze_frame(412, FMI::AboveNormal, 2);
    auto ff3 = diag.get_freeze_frame(412, FMI::AboveNormal, 3);

    ASSERT(ff0.has_value());  // Most recent (index 4)
    ASSERT(ff1.has_value());  // Index 3
    ASSERT(ff2.has_value());  // Index 2
    ASSERT(!ff3.has_value()); // Beyond depth

    // Verify most recent frame
    ASSERT_EQ(ff0.value().snapshots[0].value, 104); // Last captured value
}

TEST(freeze_frame_encode_decode) {
    DTC dtc{.spn = 110, .fmi = FMI::AboveNormal};
    dp::Vector<SPNSnapshot> snapshots;
    snapshots.push_back({.spn = 91, .value = 2500});
    snapshots.push_back({.spn = 110, .value = 95});

    FreezeFrame ff;
    ff.dtc = dtc;
    ff.timestamp_ms = 12345;
    ff.snapshots = snapshots;

    // Encode
    auto encoded = ff.encode();
    ASSERT(encoded.size() >= 9); // Header + 2 snapshots

    // Decode
    FreezeFrame decoded = FreezeFrame::decode(encoded);
    ASSERT_EQ(decoded.dtc.spn, 110);
    ASSERT_EQ(static_cast<u32>(decoded.dtc.fmi), static_cast<u32>(FMI::AboveNormal));
    ASSERT_EQ(decoded.timestamp_ms, 12345);
    ASSERT_EQ(decoded.snapshots.size(), 2);
    ASSERT_EQ(decoded.snapshots[0].spn, 91);
    ASSERT_EQ(decoded.snapshots[0].value, 2500);
}

TEST(freeze_frame_clear) {
    IsoNet net;
    Name name = Name::build().set_identity_number(4).set_manufacturer_code(1).set_function_code(0).set_industry_group(
        2);
    auto cf = net.create_internal(name, 0, 0x23).value();

    DiagnosticProtocol diag(net, cf);
    diag.initialize();

    // Capture freeze frame
    DTC dtc{.spn = 102, .fmi = FMI::AboveNormal};
    dp::Vector<SPNSnapshot> snapshots;
    snapshots.push_back({.spn = 102, .value = 250});
    diag.capture_freeze_frame(dtc, snapshots, 1000);

    // Verify captured
    auto ff1 = diag.get_freeze_frame(102, FMI::AboveNormal, 0);
    ASSERT(ff1.has_value());

    // Clear specific DTC freeze frames
    diag.clear_freeze_frames(102, FMI::AboveNormal);

    // Verify cleared
    auto ff2 = diag.get_freeze_frame(102, FMI::AboveNormal, 0);
    ASSERT(!ff2.has_value());
}

TEST(freeze_frame_clear_all) {
    IsoNet net;
    Name name = Name::build().set_identity_number(5).set_manufacturer_code(1).set_function_code(0).set_industry_group(
        2);
    auto cf = net.create_internal(name, 0, 0x24).value();

    DiagnosticProtocol diag(net, cf);
    diag.initialize();

    // Capture multiple freeze frames for different DTCs
    DTC dtc1{.spn = 110, .fmi = FMI::AboveNormal};
    DTC dtc2{.spn = 94, .fmi = FMI::BelowNormal};

    diag.capture_freeze_frame(dtc1, dp::Vector<SPNSnapshot>(), 1000);
    diag.capture_freeze_frame(dtc2, dp::Vector<SPNSnapshot>(), 2000);

    // Clear all
    diag.clear_all_freeze_frames();

    // Verify all cleared
    auto ff1 = diag.get_freeze_frame(110, FMI::AboveNormal, 0);
    auto ff2 = diag.get_freeze_frame(94, FMI::BelowNormal, 0);
    ASSERT(!ff1.has_value());
    ASSERT(!ff2.has_value());
}

// ─── DM20 Performance Ratio Tests ────────────────────────────────────────────

TEST(performance_ratio_basic) {
    MonitorPerformanceRatio ratio;
    ratio.spn = 3050;
    ratio.numerator = 15;
    ratio.denominator = 20;

    ASSERT_EQ(ratio.percentage(), 75);
    ASSERT(ratio.meets_threshold(75));
    ASSERT(!ratio.meets_threshold(80));
}

TEST(performance_ratio_encode_decode) {
    MonitorPerformanceRatio ratio;
    ratio.spn = 3053;
    ratio.numerator = 18;
    ratio.denominator = 20;

    // Encode
    auto encoded = ratio.encode();
    ASSERT_EQ(encoded.size(), 7);

    // Decode
    MonitorPerformanceRatio decoded = MonitorPerformanceRatio::decode(encoded.data());
    ASSERT_EQ(decoded.spn, 3053);
    ASSERT_EQ(decoded.numerator, 18);
    ASSERT_EQ(decoded.denominator, 20);
}

TEST(performance_ratio_tracking) {
    IsoNet net;
    Name name = Name::build().set_identity_number(6).set_manufacturer_code(1).set_function_code(0).set_industry_group(
        2);
    auto cf = net.create_internal(name, 0, 0x25).value();

    DiagnosticProtocol diag(net, cf);
    diag.initialize();

    // Track monitor execution
    for (u32 i = 0; i < 20; ++i) {
        diag.increment_monitor_opportunity(3050);
        if (i < 15) {
            diag.increment_monitor_execution(3050);
        }
    }

    auto ratio = diag.get_performance_ratio(3050);
    ASSERT(ratio.has_value());
    ASSERT_EQ(ratio.value().numerator, 15);
    ASSERT_EQ(ratio.value().denominator, 20);
    ASSERT_EQ(ratio.value().percentage(), 75);
}

TEST(performance_ratio_ignition_cycles) {
    IsoNet net;
    Name name = Name::build().set_identity_number(7).set_manufacturer_code(1).set_function_code(0).set_industry_group(
        2);
    auto cf = net.create_internal(name, 0, 0x26).value();

    DiagnosticProtocol diag(net, cf);
    diag.initialize();

    // Test ignition cycle tracking
    diag.set_ignition_cycles(10);
    ASSERT_EQ(diag.dm20_data().ignition_cycles, 10);

    diag.increment_ignition_cycles();
    ASSERT_EQ(diag.dm20_data().ignition_cycles, 11);

    // Test OBD conditions
    diag.set_obd_conditions_met(5);
    ASSERT_EQ(diag.dm20_data().obd_monitoring_conditions_met, 5);

    diag.increment_obd_conditions_met();
    ASSERT_EQ(diag.dm20_data().obd_monitoring_conditions_met, 6);
}

TEST(performance_ratio_overflow_protection) {
    IsoNet net;
    Name name = Name::build().set_identity_number(8).set_manufacturer_code(1).set_function_code(0).set_industry_group(
        2);
    auto cf = net.create_internal(name, 0, 0x27).value();

    DiagnosticProtocol diag(net, cf);
    diag.initialize();

    // Set to max values
    diag.set_performance_ratio(3050, 65535, 65535);

    // Try to increment (should not overflow)
    diag.increment_monitor_execution(3050);
    diag.increment_monitor_opportunity(3050);

    auto ratio = diag.get_performance_ratio(3050);
    ASSERT(ratio.has_value());
    ASSERT_EQ(ratio.value().numerator, 65535); // Should stay at max
    ASSERT_EQ(ratio.value().denominator, 65535);
}

TEST(dm20_response_encode_decode) {
    DM20Response resp;
    resp.ignition_cycles = 25;
    resp.obd_monitoring_conditions_met = 18;
    resp.ratios.push_back({.spn = 3050, .numerator = 15, .denominator = 20});
    resp.ratios.push_back({.spn = 3053, .numerator = 18, .denominator = 20});

    // Encode
    auto encoded = resp.encode();
    ASSERT(encoded.size() >= 2 + 7 * 2); // Header + 2 ratios

    // Decode
    DM20Response decoded = DM20Response::decode(encoded);
    ASSERT_EQ(decoded.ignition_cycles, 25);
    ASSERT_EQ(decoded.obd_monitoring_conditions_met, 18);
    ASSERT_EQ(decoded.ratios.size(), 2);
    ASSERT_EQ(decoded.ratios[0].spn, 3050);
    ASSERT_EQ(decoded.ratios[1].spn, 3053);
}

TEST(performance_ratio_reset) {
    IsoNet net;
    Name name = Name::build().set_identity_number(9).set_manufacturer_code(1).set_function_code(0).set_industry_group(
        2);
    auto cf = net.create_internal(name, 0, 0x28).value();

    DiagnosticProtocol diag(net, cf);
    diag.initialize();

    // Set up data
    diag.set_ignition_cycles(10);
    diag.set_obd_conditions_met(5);
    diag.set_performance_ratio(3050, 15, 20);

    // Reset
    diag.reset_dm20_counters();

    // Verify reset
    ASSERT_EQ(diag.dm20_data().ignition_cycles, 0);
    ASSERT_EQ(diag.dm20_data().obd_monitoring_conditions_met, 0);
    ASSERT_EQ(diag.dm20_data().ratios.size(), 0);
}

// ─── Integration Tests ───────────────────────────────────────────────────────

TEST(freeze_frame_with_active_dtc) {
    IsoNet net;
    Name name = Name::build().set_identity_number(10).set_manufacturer_code(1).set_function_code(0).set_industry_group(
        2);
    auto cf = net.create_internal(name, 0, 0x29).value();

    DiagnosticConfig config;
    config.auto_capture_freeze_frames_enabled(true);

    DiagnosticProtocol diag(net, cf, config);
    diag.initialize();

    // Activate DTC
    DTC dtc{.spn = 110, .fmi = FMI::AboveNormal};
    diag.set_active(dtc);

    // Check DTC is active
    const auto &active = diag.active_dtcs();
    ASSERT_EQ(active.size(), 1);
    ASSERT_EQ(active[0].spn, 110);

    // Check freeze frame captured
    auto ff = diag.get_freeze_frame(110, FMI::AboveNormal, 0);
    ASSERT(ff.has_value());

    // Clear DTC
    diag.clear_active(110, FMI::AboveNormal);

    // DTC moved to previously active
    const auto &active2 = diag.active_dtcs();
    ASSERT_EQ(active2.size(), 0);

    // Freeze frame should persist
    auto ff2 = diag.get_freeze_frame(110, FMI::AboveNormal, 0);
    ASSERT(ff2.has_value());
}

TEST(spn_snapshot_encode_decode) {
    SPNSnapshot snap{.spn = 190, .value = 1800};

    // Encode
    auto encoded = snap.encode();
    ASSERT_EQ(encoded.size(), 7);

    // Decode
    SPNSnapshot decoded = SPNSnapshot::decode(encoded.data());
    ASSERT_EQ(decoded.spn, 190);
    ASSERT_EQ(decoded.value, 1800);
}

TEST(dm25_request_encode_decode) {
    DM25Request req;
    req.spn = 110;
    req.fmi = FMI::AboveNormal;
    req.frame_number = 2;

    // Encode
    auto encoded = req.encode();
    ASSERT(encoded.size() >= 5);

    // Decode
    DM25Request decoded = DM25Request::decode(encoded);
    ASSERT_EQ(decoded.spn, 110);
    ASSERT_EQ(static_cast<u32>(decoded.fmi), static_cast<u32>(FMI::AboveNormal));
    ASSERT_EQ(decoded.frame_number, 2);
}

// ─── Main Test Runner ────────────────────────────────────────────────────────

int main() {
    echo::info("=== Diagnostic Extended Test Suite ===");
    echo::info("Testing DM25 Freeze Frames and DM20 Performance Ratios\n");

    // DM25 Freeze Frame Tests
    echo::info("--- DM25 Freeze Frame Tests ---");
    RUN_TEST(freeze_frame_automatic_capture);
    RUN_TEST(freeze_frame_manual_capture);
    RUN_TEST(freeze_frame_depth_limiting);
    RUN_TEST(freeze_frame_encode_decode);
    RUN_TEST(freeze_frame_clear);
    RUN_TEST(freeze_frame_clear_all);

    // DM20 Performance Ratio Tests
    echo::info("\n--- DM20 Performance Ratio Tests ---");
    RUN_TEST(performance_ratio_basic);
    RUN_TEST(performance_ratio_encode_decode);
    RUN_TEST(performance_ratio_tracking);
    RUN_TEST(performance_ratio_ignition_cycles);
    RUN_TEST(performance_ratio_overflow_protection);
    RUN_TEST(dm20_response_encode_decode);
    RUN_TEST(performance_ratio_reset);

    // Integration Tests
    echo::info("\n--- Integration Tests ---");
    RUN_TEST(freeze_frame_with_active_dtc);
    RUN_TEST(spn_snapshot_encode_decode);
    RUN_TEST(dm25_request_encode_decode);

    echo::info("\n=== All Tests Passed ===");
    echo::info("Total tests: 17");
    echo::info("  DM25 Freeze Frame: 6 tests");
    echo::info("  DM20 Performance Ratio: 7 tests");
    echo::info("  Integration: 3 tests");
    echo::info("  SPNSnapshot/DM25Request: 1 test");

    return 0;
}
