#include <doctest/doctest.h>
#include <agrobus/net/can_bus_config.hpp>

using namespace agrobus::net;

TEST_CASE("CAN bus config - default config is compliant") {
    CanBusConfig config;
    auto result = validate_can_bus_config(config);
    CHECK(result.bitrate_ok);
    CHECK(result.sample_point_ok);
    CHECK(result.overall_ok);
    CHECK(result.error_message.empty());
}

TEST_CASE("CAN bus config - valid config passes validation") {
    CanBusConfig config;
    config.set_bitrate(250000).set_sample_point(0.80);
    auto result = validate_can_bus_config(config);
    CHECK(result.bitrate_ok);
    CHECK(result.sample_point_ok);
    CHECK(result.overall_ok);
}

TEST_CASE("CAN bus config - sample point at boundaries passes") {
    SUBCASE("at minimum boundary") {
        CanBusConfig config;
        config.set_sample_point(ISO_SAMPLE_POINT_MIN);
        auto result = validate_can_bus_config(config);
        CHECK(result.sample_point_ok);
        CHECK(result.overall_ok);
    }

    SUBCASE("at maximum boundary") {
        CanBusConfig config;
        config.set_sample_point(ISO_SAMPLE_POINT_MAX);
        auto result = validate_can_bus_config(config);
        CHECK(result.sample_point_ok);
        CHECK(result.overall_ok);
    }
}

TEST_CASE("CAN bus config - wrong bitrate fails") {
    CanBusConfig config;
    config.set_bitrate(500000);
    auto result = validate_can_bus_config(config);
    CHECK_FALSE(result.bitrate_ok);
    CHECK_FALSE(result.overall_ok);
    CHECK(result.error_message == "bitrate must be 250000");
}

TEST_CASE("CAN bus config - bitrate of 125000 fails") {
    CanBusConfig config;
    config.set_bitrate(125000);
    auto result = validate_can_bus_config(config);
    CHECK_FALSE(result.bitrate_ok);
    CHECK_FALSE(result.overall_ok);
}

TEST_CASE("CAN bus config - sample point too low fails") {
    CanBusConfig config;
    config.set_sample_point(0.70);
    auto result = validate_can_bus_config(config);
    CHECK(result.bitrate_ok);
    CHECK_FALSE(result.sample_point_ok);
    CHECK_FALSE(result.overall_ok);
    CHECK(result.error_message == "sample point must be 80% +/- 3%");
}

TEST_CASE("CAN bus config - sample point too high fails") {
    CanBusConfig config;
    config.set_sample_point(0.90);
    auto result = validate_can_bus_config(config);
    CHECK(result.bitrate_ok);
    CHECK_FALSE(result.sample_point_ok);
    CHECK_FALSE(result.overall_ok);
    CHECK(result.error_message == "sample point must be 80% +/- 3%");
}

TEST_CASE("CAN bus config - sample point just below min fails") {
    CanBusConfig config;
    config.set_sample_point(0.769);
    auto result = validate_can_bus_config(config);
    CHECK_FALSE(result.sample_point_ok);
    CHECK_FALSE(result.overall_ok);
}

TEST_CASE("CAN bus config - sample point just above max fails") {
    CanBusConfig config;
    config.set_sample_point(0.831);
    auto result = validate_can_bus_config(config);
    CHECK_FALSE(result.sample_point_ok);
    CHECK_FALSE(result.overall_ok);
}

TEST_CASE("CAN bus config - enforce_iso_can_config returns error on invalid") {
    SUBCASE("invalid bitrate") {
        CanBusConfig config;
        config.set_bitrate(1000000);
        auto result = enforce_iso_can_config(config);
        CHECK(result.is_err());
    }

    SUBCASE("invalid sample point") {
        CanBusConfig config;
        config.set_sample_point(0.50);
        auto result = enforce_iso_can_config(config);
        CHECK(result.is_err());
    }

    SUBCASE("both invalid") {
        CanBusConfig config;
        config.set_bitrate(500000).set_sample_point(0.50);
        auto result = enforce_iso_can_config(config);
        CHECK(result.is_err());
    }
}

TEST_CASE("CAN bus config - enforce_iso_can_config succeeds on valid") {
    CanBusConfig config; // defaults are compliant
    auto result = enforce_iso_can_config(config);
    CHECK(result.is_ok());
}

TEST_CASE("CAN bus config - fluent API chaining") {
    CanBusConfig config;
    config.set_bitrate(250000)
          .set_sample_point(0.80)
          .set_sjw(2)
          .set_silent(true)
          .set_loopback(true);

    CHECK(config.bitrate == 250000);
    CHECK(config.sample_point == 0.80);
    CHECK(config.sjw == 2);
    CHECK(config.silent_mode == true);
    CHECK(config.loopback == true);
}

TEST_CASE("CAN bus config - ISO constants have correct values") {
    CHECK(ISO_CAN_BITRATE == 250000);
    CHECK(ISO_SAMPLE_POINT_MIN == doctest::Approx(0.77));
    CHECK(ISO_SAMPLE_POINT_MAX == doctest::Approx(0.83));
    CHECK(ISO_SAMPLE_POINT_NOMINAL == doctest::Approx(0.80));
}
