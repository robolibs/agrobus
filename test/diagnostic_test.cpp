#include <doctest/doctest.h>
#include <isobus/protocol/diagnostic.hpp>

using namespace isobus;

TEST_CASE("DTC encode/decode") {
    SUBCASE("basic encode") {
        DTC dtc;
        dtc.spn = 1234;
        dtc.fmi = FMI::VoltageHigh;
        dtc.occurrence_count = 5;

        auto bytes = dtc.encode();
        DTC decoded = DTC::decode(bytes.data());

        CHECK(decoded.spn == 1234);
        CHECK(decoded.fmi == FMI::VoltageHigh);
        CHECK(decoded.occurrence_count == 5);
    }

    SUBCASE("max SPN value (19 bits)") {
        DTC dtc;
        dtc.spn = 0x7FFFF; // max 19-bit value
        dtc.fmi = FMI::AboveNormal;
        dtc.occurrence_count = 126;

        auto bytes = dtc.encode();
        DTC decoded = DTC::decode(bytes.data());

        CHECK(decoded.spn == 0x7FFFF);
        CHECK(decoded.occurrence_count == 126);
    }

    SUBCASE("equality comparison") {
        DTC a{100, FMI::VoltageLow, 1};
        DTC b{100, FMI::VoltageLow, 5};
        DTC c{100, FMI::VoltageHigh, 1};
        CHECK(a == b); // Same SPN+FMI, different OC
        CHECK(!(a == c)); // Different FMI
    }
}

TEST_CASE("DiagnosticLamps encode/decode") {
    DiagnosticLamps lamps;
    lamps.malfunction = LampStatus::On;
    lamps.red_stop = LampStatus::Off;
    lamps.amber_warning = LampStatus::On;
    lamps.engine_protect = LampStatus::Off;
    lamps.malfunction_flash = LampFlash::FastFlash;
    lamps.amber_warning_flash = LampFlash::SlowFlash;

    auto bytes = lamps.encode();
    DiagnosticLamps decoded = DiagnosticLamps::decode(bytes.data());

    CHECK(decoded.malfunction == LampStatus::On);
    CHECK(decoded.red_stop == LampStatus::Off);
    CHECK(decoded.amber_warning == LampStatus::On);
    CHECK(decoded.engine_protect == LampStatus::Off);
    CHECK(decoded.malfunction_flash == LampFlash::FastFlash);
    CHECK(decoded.amber_warning_flash == LampFlash::SlowFlash);
}

TEST_CASE("DiagnosticProtocol DTC management") {
    NetworkManager nm;
    Name name;
    auto cf_result = nm.create_internal(name, 0, 0x28);
    auto* cf = cf_result.value();

    DiagnosticProtocol diag(nm, cf);

    SUBCASE("set active DTC") {
        DTC dtc{500, FMI::AboveNormal, 0};
        auto result = diag.set_active(dtc);
        CHECK(result.is_ok());
        CHECK(diag.active_dtcs().size() == 1);
        CHECK(diag.active_dtcs()[0].spn == 500);
        CHECK(diag.active_dtcs()[0].occurrence_count == 1);
    }

    SUBCASE("duplicate DTC increments count") {
        DTC dtc{500, FMI::AboveNormal, 0};
        diag.set_active(dtc);
        diag.set_active(dtc);
        CHECK(diag.active_dtcs().size() == 1);
        CHECK(diag.active_dtcs()[0].occurrence_count == 2);
    }

    SUBCASE("clear active moves to previous") {
        DTC dtc{500, FMI::AboveNormal, 0};
        diag.set_active(dtc);
        diag.clear_active(500, FMI::AboveNormal);
        CHECK(diag.active_dtcs().empty());
        CHECK(diag.previous_dtcs().size() == 1);
    }

    SUBCASE("clear all active") {
        diag.set_active({100, FMI::VoltageLow, 0});
        diag.set_active({200, FMI::VoltageHigh, 0});
        diag.clear_all_active();
        CHECK(diag.active_dtcs().empty());
        CHECK(diag.previous_dtcs().size() == 2);
    }

    SUBCASE("clear previous") {
        diag.set_active({100, FMI::VoltageLow, 0});
        diag.clear_all_active();
        diag.clear_previous();
        CHECK(diag.previous_dtcs().empty());
    }
}

TEST_CASE("DM5 DiagnosticProtocolID") {
    SUBCASE("encode/decode") {
        DiagnosticProtocolID id;
        id.protocols = DiagProtocol::J1939_73 | DiagProtocol::ISO_14229_3;

        auto data = id.encode();
        CHECK(data.size() == 8);
        CHECK(data[0] == 0x05); // J1939_73 (0x01) | ISO_14229_3 (0x04) = 0x05
        CHECK(data[1] == 0xFF); // reserved

        auto decoded = DiagnosticProtocolID::decode(data);
        CHECK(decoded.protocols == 0x05);
    }

    SUBCASE("default is J1939") {
        DiagnosticProtocolID id;
        CHECK(id.protocols == static_cast<u8>(DiagProtocol::J1939_73));
    }

    SUBCASE("set_diag_protocol and accessor") {
        NetworkManager nm;
        Name name;
        auto cf_result = nm.create_internal(name, 0, 0x28);
        auto *cf = cf_result.value();
        DiagnosticProtocol diag(nm, cf);

        DiagnosticProtocolID id;
        id.protocols = DiagProtocol::J1939_73 | DiagProtocol::ISO_14230;
        diag.set_diag_protocol(id);
        CHECK(diag.diag_protocol_id().protocols == 0x03);
    }
}

TEST_CASE("DM13 suspend duration tracking") {
    NetworkManager nm;
    Name name;
    auto cf_result = nm.create_internal(name, 0, 0x28);
    auto *cf = cf_result.value();

    DiagnosticProtocol diag(nm, cf);
    diag.initialize();

    // Helper: create a DM13 message
    auto make_dm13 = [](u8 byte0, u16 duration) -> Message {
        Message msg;
        msg.pgn = PGN_DM13;
        msg.source = 0x30;
        msg.destination = 0x28;
        msg.data = {byte0, 0xFF, static_cast<u8>(duration & 0xFF), static_cast<u8>((duration >> 8) & 0xFF),
                    0xFF, 0xFF, 0xFF, 0xFF};
        return msg;
    };

    // Track DM13 events
    bool dm13_received = false;
    DM13Signals received_signals;
    diag.on_dm13_received.subscribe([&](const DM13Signals &sig, Address) {
        dm13_received = true;
        received_signals = sig;
    });

    SUBCASE("finite suspend duration auto-resumes") {
        // Byte 0: hold(bits7-6)=DoNotCare(3), dm1(bits5-4)=Suspend(0), dm2(bits3-2)=DoNotCare(3), dm3(bits1-0)=DoNotCare(3)
        // = 0b11_00_11_11 = 0xCF
        nm.inject_message(make_dm13(0xCF, 5)); // dm1=suspend, duration=5s

        CHECK(dm13_received);
        CHECK(received_signals.dm1_signal == DM13Command::SuspendBroadcast);
        CHECK(received_signals.suspend_duration_s == 5);
        CHECK(diag.is_dm1_suspended());

        // After 4 seconds, still suspended
        diag.update(4000);
        CHECK(diag.is_dm1_suspended());

        // After 5 seconds total (1 more), auto-resume
        diag.update(1000);
        CHECK(!diag.is_dm1_suspended());
    }

    SUBCASE("indefinite suspend does not auto-resume") {
        nm.inject_message(make_dm13(0xCF, 0xFFFF)); // dm1=suspend, indefinite

        CHECK(diag.is_dm1_suspended());

        // Even after a very long time, stays suspended
        diag.update(60000);
        CHECK(diag.is_dm1_suspended());
    }

    SUBCASE("resume message clears suspend") {
        // First suspend with 10s duration
        nm.inject_message(make_dm13(0xCF, 10));
        CHECK(diag.is_dm1_suspended());

        // Then explicitly resume before duration expires
        // dm1_signal=Resume(1): bits 5-4 = 01 → 0b11_01_11_11 = 0xDF
        nm.inject_message(make_dm13(0xDF, 0xFFFF));
        CHECK(!diag.is_dm1_suspended());
    }
}
