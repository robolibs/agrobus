#include <doctest/doctest.h>
#include <agrobus/isobus/implement/tractor_commands.hpp>

using namespace agrobus::isobus;
using namespace agrobus::isobus::implement;

// ─── PGN Constants ───────────────────────────────────────────────────────────

TEST_CASE("Tractor command PGN constants") {
    CHECK(PGN_REAR_HITCH_CMD == 0xFE50);
    CHECK(PGN_FRONT_HITCH_CMD == 0xFE51);
    CHECK(PGN_REAR_PTO_CMD == 0xFE52);
    CHECK(PGN_FRONT_PTO_CMD == 0xFE53);
    CHECK(PGN_AUX_VALVE_CMD == 0xFE30);
    CHECK(PGN_TRACTOR_CONTROL_MODE == 0xFE0B);
}

// ─── HitchCommandMsg Encode/Decode ──────────────────────────────────────────

TEST_CASE("HitchCommandMsg encode") {
    SUBCASE("default values") {
        HitchCommandMsg msg;
        auto data = msg.encode();
        CHECK(data.size() == 8);
        CHECK(data[0] == 0xFF); // target_position low (0xFFFF)
        CHECK(data[1] == 0xFF); // target_position high
        CHECK(data[2] == 0xFF); // reserved
        CHECK(data[3] == 0xFF); // rate
        CHECK(data[4] == 0x00); // NoAction
        CHECK(data[5] == 0xFF); // reserved
        CHECK(data[6] == 0xFF); // reserved
        CHECK(data[7] == 0xFF); // reserved
    }

    SUBCASE("raise command with position") {
        HitchCommandMsg msg;
        msg.command = HitchCommand::Raise;
        msg.target_position = 20000; // 50%
        msg.rate = 128;

        auto data = msg.encode();
        CHECK(data.size() == 8);
        // 20000 = 0x4E20
        CHECK(data[0] == 0x20);
        CHECK(data[1] == 0x4E);
        CHECK(data[3] == 128);
        CHECK(data[4] == static_cast<u8>(HitchCommand::Raise));
    }

    SUBCASE("lower command") {
        HitchCommandMsg msg;
        msg.command = HitchCommand::Lower;
        msg.target_position = 0;
        msg.rate = 50;

        auto data = msg.encode();
        CHECK(data[0] == 0x00);
        CHECK(data[1] == 0x00);
        CHECK(data[3] == 50);
        CHECK(data[4] == static_cast<u8>(HitchCommand::Lower));
    }

    SUBCASE("position command full range") {
        HitchCommandMsg msg;
        msg.command = HitchCommand::Position;
        msg.target_position = 40000; // 100%

        auto data = msg.encode();
        // 40000 = 0x9C40
        CHECK(data[0] == 0x40);
        CHECK(data[1] == 0x9C);
        CHECK(data[4] == static_cast<u8>(HitchCommand::Position));
    }
}

TEST_CASE("HitchCommandMsg decode") {
    SUBCASE("round-trip") {
        HitchCommandMsg original;
        original.command = HitchCommand::Position;
        original.target_position = 30000;
        original.rate = 200;

        auto data = original.encode();
        auto decoded = HitchCommandMsg::decode(data);

        CHECK(decoded.command == HitchCommand::Position);
        CHECK(decoded.target_position == 30000);
        CHECK(decoded.rate == 200);
    }

    SUBCASE("decode all commands") {
        for (u8 cmd = 0; cmd <= 3; ++cmd) {
            HitchCommandMsg msg;
            msg.command = static_cast<HitchCommand>(cmd);
            auto data = msg.encode();
            auto decoded = HitchCommandMsg::decode(data);
            CHECK(decoded.command == static_cast<HitchCommand>(cmd));
        }
    }

    SUBCASE("decode from raw bytes") {
        dp::Vector<u8> data = {0x10, 0x27, 0xFF, 0x64, 0x02, 0xFF, 0xFF, 0xFF};
        auto decoded = HitchCommandMsg::decode(data);
        CHECK(decoded.target_position == 10000); // 0x2710
        CHECK(decoded.rate == 100);              // 0x64
        CHECK(decoded.command == HitchCommand::Raise);
    }

    SUBCASE("decode insufficient data") {
        dp::Vector<u8> data = {0x10, 0x27}; // too short
        auto decoded = HitchCommandMsg::decode(data);
        CHECK(decoded.command == HitchCommand::NoAction); // defaults
    }
}

// ─── PTOCommandMsg Encode/Decode ─────────────────────────────────────────────

TEST_CASE("PTOCommandMsg encode") {
    SUBCASE("default values") {
        PTOCommandMsg msg;
        auto data = msg.encode();
        CHECK(data.size() == 8);
        CHECK(data[0] == 0xFF); // target_speed low
        CHECK(data[1] == 0xFF); // target_speed high
        CHECK(data[3] == 0xFF); // ramp_rate
        CHECK(data[4] == 0x00); // NoAction
    }

    SUBCASE("engage with speed") {
        PTOCommandMsg msg;
        msg.command = PTOCommand::Engage;
        msg.target_speed_rpm = 540 * 8; // 540 RPM at 0.125 per bit = 4320
        msg.ramp_rate = 100;

        auto data = msg.encode();
        // 4320 = 0x10E0
        CHECK(data[0] == 0xE0);
        CHECK(data[1] == 0x10);
        CHECK(data[3] == 100);
        CHECK(data[4] == static_cast<u8>(PTOCommand::Engage));
    }

    SUBCASE("disengage") {
        PTOCommandMsg msg;
        msg.command = PTOCommand::Disengage;
        msg.target_speed_rpm = 0;
        msg.ramp_rate = 255;

        auto data = msg.encode();
        CHECK(data[0] == 0x00);
        CHECK(data[1] == 0x00);
        CHECK(data[4] == static_cast<u8>(PTOCommand::Disengage));
    }

    SUBCASE("set speed 1000 RPM") {
        PTOCommandMsg msg;
        msg.command = PTOCommand::SetSpeed;
        msg.target_speed_rpm = 1000 * 8; // 8000

        auto data = msg.encode();
        // 8000 = 0x1F40
        CHECK(data[0] == 0x40);
        CHECK(data[1] == 0x1F);
        CHECK(data[4] == static_cast<u8>(PTOCommand::SetSpeed));
    }
}

TEST_CASE("PTOCommandMsg decode") {
    SUBCASE("round-trip") {
        PTOCommandMsg original;
        original.command = PTOCommand::SetSpeed;
        original.target_speed_rpm = 4320;
        original.ramp_rate = 50;

        auto data = original.encode();
        auto decoded = PTOCommandMsg::decode(data);

        CHECK(decoded.command == PTOCommand::SetSpeed);
        CHECK(decoded.target_speed_rpm == 4320);
        CHECK(decoded.ramp_rate == 50);
    }

    SUBCASE("decode all commands") {
        for (u8 cmd = 0; cmd <= 3; ++cmd) {
            PTOCommandMsg msg;
            msg.command = static_cast<PTOCommand>(cmd);
            auto data = msg.encode();
            auto decoded = PTOCommandMsg::decode(data);
            CHECK(decoded.command == static_cast<PTOCommand>(cmd));
        }
    }

    SUBCASE("decode insufficient data") {
        dp::Vector<u8> data = {0x10}; // too short
        auto decoded = PTOCommandMsg::decode(data);
        CHECK(decoded.command == PTOCommand::NoAction);
    }
}

// ─── AuxValveCommandMsg Encode/Decode ────────────────────────────────────────

TEST_CASE("AuxValveCommandMsg encode") {
    SUBCASE("default values") {
        AuxValveCommandMsg msg;
        auto data = msg.encode();
        CHECK(data.size() == 8);
        CHECK(data[0] == 0);    // valve_index
        CHECK(data[1] == 0xFF); // flow_rate low
        CHECK(data[2] == 0xFF); // flow_rate high
        CHECK(data[3] == 0x00); // NoAction
    }

    SUBCASE("extend valve 2 at 50% flow") {
        AuxValveCommandMsg msg;
        msg.valve_index = 2;
        msg.command = ValveCommand::Extend;
        msg.flow_rate = 125; // 50% at 0.4%/bit = 125

        auto data = msg.encode();
        CHECK(data[0] == 2);
        CHECK(data[1] == 125);
        CHECK(data[2] == 0x00);
        CHECK(data[3] == static_cast<u8>(ValveCommand::Extend));
    }

    SUBCASE("retract valve 0") {
        AuxValveCommandMsg msg;
        msg.valve_index = 0;
        msg.command = ValveCommand::Retract;
        msg.flow_rate = 250; // 100%

        auto data = msg.encode();
        CHECK(data[0] == 0);
        CHECK(data[1] == 0xFA); // 250
        CHECK(data[2] == 0x00);
        CHECK(data[3] == static_cast<u8>(ValveCommand::Retract));
    }

    SUBCASE("float command") {
        AuxValveCommandMsg msg;
        msg.valve_index = 5;
        msg.command = ValveCommand::Float;

        auto data = msg.encode();
        CHECK(data[0] == 5);
        CHECK(data[3] == static_cast<u8>(ValveCommand::Float));
    }

    SUBCASE("block command") {
        AuxValveCommandMsg msg;
        msg.valve_index = 3;
        msg.command = ValveCommand::Block;
        msg.flow_rate = 0;

        auto data = msg.encode();
        CHECK(data[0] == 3);
        CHECK(data[1] == 0x00);
        CHECK(data[2] == 0x00);
        CHECK(data[3] == static_cast<u8>(ValveCommand::Block));
    }
}

TEST_CASE("AuxValveCommandMsg decode") {
    SUBCASE("round-trip") {
        AuxValveCommandMsg original;
        original.valve_index = 4;
        original.command = ValveCommand::Extend;
        original.flow_rate = 200;

        auto data = original.encode();
        auto decoded = AuxValveCommandMsg::decode(data);

        CHECK(decoded.valve_index == 4);
        CHECK(decoded.command == ValveCommand::Extend);
        CHECK(decoded.flow_rate == 200);
    }

    SUBCASE("decode all commands") {
        for (u8 cmd = 0; cmd <= 4; ++cmd) {
            AuxValveCommandMsg msg;
            msg.command = static_cast<ValveCommand>(cmd);
            auto data = msg.encode();
            auto decoded = AuxValveCommandMsg::decode(data);
            CHECK(decoded.command == static_cast<ValveCommand>(cmd));
        }
    }

    SUBCASE("decode with large flow rate") {
        AuxValveCommandMsg msg;
        msg.flow_rate = 0x1234;
        msg.valve_index = 7;
        msg.command = ValveCommand::Retract;

        auto data = msg.encode();
        auto decoded = AuxValveCommandMsg::decode(data);
        CHECK(decoded.flow_rate == 0x1234);
        CHECK(decoded.valve_index == 7);
    }

    SUBCASE("decode insufficient data") {
        dp::Vector<u8> data = {1, 2}; // too short
        auto decoded = AuxValveCommandMsg::decode(data);
        CHECK(decoded.command == ValveCommand::NoAction);
    }
}

// ─── TractorCommands Class ───────────────────────────────────────────────────

TEST_CASE("TractorCommands construction") {
    IsoNet nm;
    Name name;
    auto cf_result = nm.create_internal(name, 0, 0x28);
    auto* cf = cf_result.value();

    TractorCommands tractor(nm, cf);

    SUBCASE("initialize registers callbacks") {
        auto result = tractor.initialize();
        CHECK(result.is_ok());
    }

    SUBCASE("send commands fail without address claim") {
        tractor.initialize();

        HitchCommandMsg hitch;
        hitch.command = HitchCommand::Raise;
        hitch.target_position = 20000;
        auto result = tractor.send_rear_hitch(hitch);
        CHECK(result.is_err()); // Not connected (no address claimed)
    }
}

TEST_CASE("TractorCommands events") {
    IsoNet nm;
    Name name;
    auto cf_result = nm.create_internal(name, 0, 0x28);
    auto* cf = cf_result.value();

    TractorCommands tractor(nm, cf);
    tractor.initialize();

    SUBCASE("on_rear_hitch_command event fires") {
        HitchCommandMsg received;
        bool called = false;
        tractor.on_rear_hitch_command += [&](HitchCommandMsg cmd) {
            received = cmd;
            called = true;
        };

        // Simulate a received rear hitch command message
        HitchCommandMsg original;
        original.command = HitchCommand::Position;
        original.target_position = 25000;
        original.rate = 150;

        Message msg;
        msg.pgn = PGN_REAR_HITCH_CMD;
        msg.source = 0x00;
        msg.data = original.encode();

        // Dispatch through network manager PGN callback system
        // The TractorCommands registered a callback for PGN_REAR_HITCH_CMD
        // We simulate what the network manager would do
        tractor.on_rear_hitch_command.emit(HitchCommandMsg::decode(msg.data));

        CHECK(called == true);
        CHECK(received.command == HitchCommand::Position);
        CHECK(received.target_position == 25000);
        CHECK(received.rate == 150);
    }

    SUBCASE("on_front_hitch_command event fires") {
        bool called = false;
        tractor.on_front_hitch_command += [&](HitchCommandMsg cmd) {
            called = true;
            CHECK(cmd.command == HitchCommand::Lower);
        };

        HitchCommandMsg msg;
        msg.command = HitchCommand::Lower;
        msg.target_position = 0;
        tractor.on_front_hitch_command.emit(msg);
        CHECK(called == true);
    }

    SUBCASE("on_rear_pto_command event fires") {
        PTOCommandMsg received;
        tractor.on_rear_pto_command += [&](PTOCommandMsg cmd) {
            received = cmd;
        };

        PTOCommandMsg pto;
        pto.command = PTOCommand::Engage;
        pto.target_speed_rpm = 4320;
        pto.ramp_rate = 80;
        tractor.on_rear_pto_command.emit(pto);

        CHECK(received.command == PTOCommand::Engage);
        CHECK(received.target_speed_rpm == 4320);
        CHECK(received.ramp_rate == 80);
    }

    SUBCASE("on_front_pto_command event fires") {
        PTOCommandMsg received;
        tractor.on_front_pto_command += [&](PTOCommandMsg cmd) {
            received = cmd;
        };

        PTOCommandMsg pto;
        pto.command = PTOCommand::SetSpeed;
        pto.target_speed_rpm = 8000;
        tractor.on_front_pto_command.emit(pto);

        CHECK(received.command == PTOCommand::SetSpeed);
        CHECK(received.target_speed_rpm == 8000);
    }

    SUBCASE("on_aux_valve_command event fires") {
        AuxValveCommandMsg received;
        tractor.on_aux_valve_command += [&](AuxValveCommandMsg cmd) {
            received = cmd;
        };

        AuxValveCommandMsg valve;
        valve.valve_index = 3;
        valve.command = ValveCommand::Extend;
        valve.flow_rate = 125;
        tractor.on_aux_valve_command.emit(valve);

        CHECK(received.valve_index == 3);
        CHECK(received.command == ValveCommand::Extend);
        CHECK(received.flow_rate == 125);
    }
}

TEST_CASE("TractorCommands send methods API") {
    IsoNet nm;
    Name name;
    auto cf_result = nm.create_internal(name, 0, 0x28);
    auto* cf = cf_result.value();

    TractorCommands tractor(nm, cf);
    tractor.initialize();

    SUBCASE("send_rear_hitch") {
        HitchCommandMsg cmd;
        cmd.command = HitchCommand::Raise;
        cmd.target_position = 40000;
        auto result = tractor.send_rear_hitch(cmd);
        // Will fail without address claim, but API is callable
        CHECK(result.is_err());
    }

    SUBCASE("send_front_hitch") {
        HitchCommandMsg cmd;
        cmd.command = HitchCommand::Lower;
        auto result = tractor.send_front_hitch(cmd);
        CHECK(result.is_err());
    }

    SUBCASE("send_rear_pto") {
        PTOCommandMsg cmd;
        cmd.command = PTOCommand::Engage;
        cmd.target_speed_rpm = 4320;
        auto result = tractor.send_rear_pto(cmd);
        CHECK(result.is_err());
    }

    SUBCASE("send_front_pto") {
        PTOCommandMsg cmd;
        cmd.command = PTOCommand::Disengage;
        auto result = tractor.send_front_pto(cmd);
        CHECK(result.is_err());
    }

    SUBCASE("send_aux_valve") {
        AuxValveCommandMsg cmd;
        cmd.valve_index = 2;
        cmd.command = ValveCommand::Extend;
        cmd.flow_rate = 200;
        auto result = tractor.send_aux_valve(cmd);
        CHECK(result.is_err());
    }
}

// ─── Enum Value Tests ────────────────────────────────────────────────────────

TEST_CASE("HitchCommand enum values") {
    CHECK(static_cast<u8>(HitchCommand::NoAction) == 0);
    CHECK(static_cast<u8>(HitchCommand::Lower) == 1);
    CHECK(static_cast<u8>(HitchCommand::Raise) == 2);
    CHECK(static_cast<u8>(HitchCommand::Position) == 3);
}

TEST_CASE("PTOCommand enum values") {
    CHECK(static_cast<u8>(PTOCommand::NoAction) == 0);
    CHECK(static_cast<u8>(PTOCommand::Engage) == 1);
    CHECK(static_cast<u8>(PTOCommand::Disengage) == 2);
    CHECK(static_cast<u8>(PTOCommand::SetSpeed) == 3);
}

TEST_CASE("ValveCommand enum values") {
    CHECK(static_cast<u8>(ValveCommand::NoAction) == 0);
    CHECK(static_cast<u8>(ValveCommand::Extend) == 1);
    CHECK(static_cast<u8>(ValveCommand::Retract) == 2);
    CHECK(static_cast<u8>(ValveCommand::Float) == 3);
    CHECK(static_cast<u8>(ValveCommand::Block) == 4);
}
