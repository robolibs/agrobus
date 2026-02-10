#include <doctest/doctest.h>
#include <agrobus/isobus/guidance.hpp>
#include <echo/echo.hpp>

using namespace agrobus::isobus;
using namespace agrobus::net;

// Helper to simulate receiving a guidance message by encoding and injecting it
void inject_guidance_machine(IsoNet &net, Address src_addr, const GuidanceData &gd) {
    dp::Vector<u8> data(8, 0xFF);
    if (gd.curvature) {
        i16 raw = static_cast<i16>((*gd.curvature * 1000.0) / 0.25 + 8032);
        data[0] = static_cast<u8>(raw & 0xFF);
        data[1] = static_cast<u8>((raw >> 8) & 0xFF);
    }
    if (gd.status) {
        data[2] = *gd.status;
    }

    Message msg;
    msg.pgn = PGN_GUIDANCE_MACHINE;
    msg.source = src_addr;
    msg.data = data;
    msg.timestamp_us = 0;
    net.inject_message(msg);
}

void inject_guidance_system(IsoNet &net, Address src_addr, const GuidanceData &gd) {
    dp::Vector<u8> data(8, 0xFF);
    if (gd.curvature) {
        i16 raw = static_cast<i16>((*gd.curvature * 1000.0) / 0.25 + 8032);
        data[0] = static_cast<u8>(raw & 0xFF);
        data[1] = static_cast<u8>((raw >> 8) & 0xFF);
    }
    if (gd.status) {
        data[2] = *gd.status;
    }

    Message msg;
    msg.pgn = PGN_GUIDANCE_SYSTEM;
    msg.source = src_addr;
    msg.data = data;
    msg.timestamp_us = 0;
    net.inject_message(msg);
}

TEST_CASE("GuidanceData construction") {
    GuidanceData gd;
    CHECK(!gd.curvature.has_value());
    CHECK(!gd.heading_rad.has_value());
    CHECK(!gd.cross_track_m.has_value());
    CHECK(!gd.status.has_value());
    CHECK(gd.timestamp_us == 0);
}

TEST_CASE("inject_message test") {
    IsoNet net;
    bool callback_fired = false;

    net.register_pgn_callback(PGN_GUIDANCE_MACHINE, [&](const Message &msg) {
        callback_fired = true;
    });

    Message msg;
    msg.pgn = PGN_GUIDANCE_MACHINE;
    msg.source = 0x30;
    msg.data = {0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    net.inject_message(msg);

    CHECK(callback_fired);
}

TEST_CASE("GuidanceData field assignment") {
    GuidanceData gd;

    gd.curvature = 0.01;  // 1/100m radius (right turn)
    CHECK(gd.curvature.has_value());
    CHECK(*gd.curvature == doctest::Approx(0.01));

    gd.heading_rad = 1.57;  // ~90 degrees
    CHECK(gd.heading_rad.has_value());
    CHECK(*gd.heading_rad == doctest::Approx(1.57));

    gd.cross_track_m = 2.5;  // 2.5m right of line
    CHECK(gd.cross_track_m.has_value());
    CHECK(*gd.cross_track_m == doctest::Approx(2.5));

    gd.status = 0x42;
    CHECK(gd.status.has_value());
    CHECK(*gd.status == 0x42);
}

TEST_CASE("GuidanceInterface initialization") {
    IsoNet net;
    Name name = Name::build().set_identity_number(100);
    auto cf = net.create_internal(name, 0, 0x28).value();

    GuidanceInterface gi(net, cf);
    auto result = gi.initialize();

    CHECK(result.is_ok());
    CHECK(!gi.latest_machine().has_value());
    CHECK(!gi.latest_system().has_value());
}

TEST_CASE("GuidanceInterface with custom config") {
    IsoNet net;
    Name name = Name::build().set_identity_number(101);
    auto cf = net.create_internal(name, 0, 0x29).value();

    GuidanceConfig config;
    config.machine_info(false).system_command(true);

    GuidanceInterface gi(net, cf, config);
    auto result = gi.initialize();

    CHECK(result.is_ok());
}

TEST_CASE("Curvature encoding/decoding - straight line") {
    IsoNet net;
    Name receiver_name = Name::build().set_identity_number(103);
    auto receiver_cf = net.create_internal(receiver_name, 0, 0x31).value();

    GuidanceInterface receiver(net, receiver_cf);
    receiver.initialize();

    // Send curvature = 0 (straight line)
    GuidanceData send_data;
    send_data.curvature = 0.0;
    send_data.status = 0x00;

    bool received = false;
    receiver.on_guidance_machine.subscribe([&](const GuidanceData &gd) {
        CHECK(gd.curvature.has_value());
        CHECK(*gd.curvature == doctest::Approx(0.0).epsilon(0.0001));
        received = true;
    });

    // Simulate receiving a message from address 0x30
    inject_guidance_machine(net, 0x30, send_data);

    CHECK(received);
}

TEST_CASE("Curvature encoding/decoding - right turn") {
    IsoNet net;
    Name receiver_name = Name::build().set_identity_number(105);
    auto receiver_cf = net.create_internal(receiver_name, 0, 0x33).value();

    GuidanceInterface receiver(net, receiver_cf);
    receiver.initialize();

    // Positive curvature = right turn
    // 0.01 = 1/100m radius = 100m radius right turn
    GuidanceData send_data;
    send_data.curvature = 0.01;
    send_data.status = 0x01;

    bool received = false;
    receiver.on_guidance_machine.subscribe([&](const GuidanceData &gd) {
        CHECK(gd.curvature.has_value());
        CHECK(*gd.curvature == doctest::Approx(0.01).epsilon(0.0001));
        CHECK(gd.status.has_value());
        CHECK(*gd.status == 0x01);
        received = true;
    });

    inject_guidance_machine(net, 0x32, send_data);

    CHECK(received);
}

TEST_CASE("Curvature encoding/decoding - left turn") {
    IsoNet net;
    Name receiver_name = Name::build().set_identity_number(107);
    auto receiver_cf = net.create_internal(receiver_name, 0, 0x35).value();

    GuidanceInterface receiver(net, receiver_cf);
    receiver.initialize();

    // Negative curvature = left turn
    GuidanceData send_data;
    send_data.curvature = -0.005;  // 200m radius left turn
    send_data.status = 0x02;

    bool received = false;
    receiver.on_guidance_machine.subscribe([&](const GuidanceData &gd) {
        CHECK(gd.curvature.has_value());
        CHECK(*gd.curvature == doctest::Approx(-0.005).epsilon(0.0001));
        received = true;
    });

    inject_guidance_machine(net, 0x34, send_data);

    CHECK(received);
}

TEST_CASE("Curvature encoding/decoding - sharp turn") {
    IsoNet net;
    Name receiver_name = Name::build().set_identity_number(109);
    auto receiver_cf = net.create_internal(receiver_name, 0, 0x37).value();

    GuidanceInterface receiver(net, receiver_cf);
    receiver.initialize();

    // Sharp turn: 0.1 = 1/10m = 10m radius
    GuidanceData send_data;
    send_data.curvature = 0.1;

    bool received = false;
    receiver.on_guidance_machine.subscribe([&](const GuidanceData &gd) {
        CHECK(gd.curvature.has_value());
        CHECK(*gd.curvature == doctest::Approx(0.1).epsilon(0.001));
        received = true;
    });

    inject_guidance_machine(net, 0x36, send_data);

    CHECK(received);
}

TEST_CASE("System command encoding/decoding") {
    IsoNet net;
    Name receiver_name = Name::build().set_identity_number(111);
    auto receiver_cf = net.create_internal(receiver_name, 0, 0x39).value();

    GuidanceInterface receiver(net, receiver_cf);
    receiver.initialize();

    GuidanceData send_data;
    send_data.curvature = 0.025;  // 40m radius
    send_data.status = 0xAB;

    bool received = false;
    receiver.on_guidance_system.subscribe([&](const GuidanceData &gd) {
        CHECK(gd.curvature.has_value());
        CHECK(*gd.curvature == doctest::Approx(0.025).epsilon(0.0001));
        CHECK(gd.status.has_value());
        CHECK(*gd.status == 0xAB);
        received = true;
    });

    inject_guidance_system(net, 0x38, send_data);

    CHECK(received);
}

TEST_CASE("Guidance message without curvature") {
    IsoNet net;
    Name receiver_name = Name::build().set_identity_number(113);
    auto receiver_cf = net.create_internal(receiver_name, 0, 0x3B).value();

    GuidanceInterface receiver(net, receiver_cf);
    receiver.initialize();

    // Send only status, no curvature
    GuidanceData send_data;
    send_data.status = 0x55;

    bool received = false;
    receiver.on_guidance_machine.subscribe([&](const GuidanceData &gd) {
        CHECK(!gd.curvature.has_value());  // Should be empty
        CHECK(gd.status.has_value());
        CHECK(*gd.status == 0x55);
        received = true;
    });

    inject_guidance_machine(net, 0x3A, send_data);

    CHECK(received);
}

TEST_CASE("Latest machine info tracking") {
    IsoNet net;
    Name receiver_name = Name::build().set_identity_number(115);
    auto receiver_cf = net.create_internal(receiver_name, 0, 0x3D).value();

    GuidanceInterface receiver(net, receiver_cf);
    receiver.initialize();

    CHECK(!receiver.latest_machine().has_value());

    GuidanceData send_data;
    send_data.curvature = 0.015;
    inject_guidance_machine(net, 0x3C, send_data);

    CHECK(receiver.latest_machine().has_value());
    CHECK(receiver.latest_machine()->curvature.has_value());
    CHECK(*receiver.latest_machine()->curvature == doctest::Approx(0.015).epsilon(0.0001));
}

TEST_CASE("Latest system command tracking") {
    IsoNet net;
    Name receiver_name = Name::build().set_identity_number(117);
    auto receiver_cf = net.create_internal(receiver_name, 0, 0x3F).value();

    GuidanceInterface receiver(net, receiver_cf);
    receiver.initialize();

    CHECK(!receiver.latest_system().has_value());

    GuidanceData send_data;
    send_data.curvature = -0.02;
    send_data.status = 0x99;
    inject_guidance_system(net, 0x3E, send_data);

    CHECK(receiver.latest_system().has_value());
    CHECK(receiver.latest_system()->curvature.has_value());
    CHECK(*receiver.latest_system()->curvature == doctest::Approx(-0.02).epsilon(0.0001));
    CHECK(*receiver.latest_system()->status == 0x99);
}

TEST_CASE("Multiple guidance updates") {
    IsoNet net;
    Name receiver_name = Name::build().set_identity_number(119);
    auto receiver_cf = net.create_internal(receiver_name, 0, 0x41).value();

    GuidanceInterface receiver(net, receiver_cf);
    receiver.initialize();

    int count = 0;
    receiver.on_guidance_machine.subscribe([&](const GuidanceData &) {
        count++;
    });

    // Send multiple updates
    for (int i = 0; i < 5; ++i) {
        GuidanceData send_data;
        send_data.curvature = 0.001 * i;
        inject_guidance_machine(net, 0x40, send_data);
    }

    CHECK(count == 5);

    // Latest should be the last one sent
    CHECK(receiver.latest_machine().has_value());
    CHECK(*receiver.latest_machine()->curvature == doctest::Approx(0.004).epsilon(0.0001));
}

TEST_CASE("Curvature precision - resolution 0.25 km^-1") {
    IsoNet net;
    Name receiver_name = Name::build().set_identity_number(121);
    auto receiver_cf = net.create_internal(receiver_name, 0, 0x43).value();

    GuidanceInterface receiver(net, receiver_cf);
    receiver.initialize();

    // Resolution is 0.25 km^-1 = 0.00025 m^-1
    GuidanceData send_data;
    send_data.curvature = 0.00025;  // Minimum resolution

    bool received = false;
    receiver.on_guidance_machine.subscribe([&](const GuidanceData &gd) {
        CHECK(gd.curvature.has_value());
        // Should be close to 0.00025 within encoding resolution
        CHECK(*gd.curvature == doctest::Approx(0.00025).epsilon(0.0001));
        received = true;
    });

    inject_guidance_machine(net, 0x42, send_data);

    CHECK(received);
}
