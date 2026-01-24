#include <doctest/doctest.h>
#include <agrobus/j1939/diagnostic.hpp>

using namespace agrobus::j1939;

TEST_CASE("DTC encode/decode") {
    SUBCASE("basic encode") {
        DTC dtc;
        dtc.spn = 500;
        dtc.fmi = FMI::AboveNormal;
        dtc.occurrence_count = 3;

        auto bytes = dtc.encode();
        CHECK(bytes[0] == (500 & 0xFF));
        CHECK(bytes[1] == ((500 >> 8) & 0xFF));
        CHECK(bytes[3] == 3); // occurrence count
    }

    SUBCASE("roundtrip encode/decode") {
        DTC original;
        original.spn = 12345;
        original.fmi = FMI::VoltageHigh;
        original.occurrence_count = 7;

        auto bytes = original.encode();
        DTC decoded = DTC::decode(bytes.data());

        CHECK(decoded.spn == original.spn);
        CHECK(decoded.fmi == original.fmi);
        CHECK(decoded.occurrence_count == original.occurrence_count);
    }

    SUBCASE("max SPN value (19 bits)") {
        DTC dtc;
        dtc.spn = 0x7FFFF; // 19 bits max
        dtc.fmi = FMI::MechanicalFail;
        dtc.occurrence_count = 126;

        auto bytes = dtc.encode();
        DTC decoded = DTC::decode(bytes.data());

        CHECK(decoded.spn == 0x7FFFF);
        CHECK(decoded.fmi == FMI::MechanicalFail);
        CHECK(decoded.occurrence_count == 126);
    }

    SUBCASE("SPN zero") {
        DTC dtc;
        dtc.spn = 0;
        dtc.fmi = FMI::RootCauseUnknown;
        dtc.occurrence_count = 0;

        auto bytes = dtc.encode();
        DTC decoded = DTC::decode(bytes.data());
        CHECK(decoded.spn == 0);
        CHECK(decoded.occurrence_count == 0);
    }

    SUBCASE("all FMI values round-trip") {
        for (u8 fmi_val = 0; fmi_val <= 31; ++fmi_val) {
            DTC dtc;
            dtc.spn = 1000;
            dtc.fmi = static_cast<FMI>(fmi_val);
            dtc.occurrence_count = 1;

            auto bytes = dtc.encode();
            DTC decoded = DTC::decode(bytes.data());
            CHECK(static_cast<u8>(decoded.fmi) == fmi_val);
        }
    }
}

TEST_CASE("DTC equality") {
    DTC a{500, FMI::AboveNormal, 1};
    DTC b{500, FMI::AboveNormal, 5};
    DTC c{501, FMI::AboveNormal, 1};
    DTC d{500, FMI::BelowNormal, 1};

    // Equality is based on SPN + FMI, not occurrence count
    CHECK(a == b);
    CHECK(!(a == c));
    CHECK(!(a == d));
}

TEST_CASE("DiagnosticLamps encode/decode") {
    SUBCASE("all off") {
        DiagnosticLamps lamps;
        auto bytes = lamps.encode();
        // LampStatus::Off = 0, so byte[0] (status bits) = 0x00
        CHECK(bytes[0] == 0x00);
        // LampFlash::Off = 2, so byte[1] (flash bits) = (2<<6)|(2<<4)|(2<<2)|2 = 0xAA
        CHECK(bytes[1] == 0xAA);
    }

    SUBCASE("roundtrip") {
        DiagnosticLamps original;
        original.malfunction = LampStatus::On;
        original.red_stop = LampStatus::Error;
        original.amber_warning = LampStatus::On;
        original.engine_protect = LampStatus::Off;
        original.malfunction_flash = LampFlash::FastFlash;
        original.red_stop_flash = LampFlash::SlowFlash;
        original.amber_warning_flash = LampFlash::Off;
        original.engine_protect_flash = LampFlash::NotAvailable;

        auto bytes = original.encode();
        DiagnosticLamps decoded = DiagnosticLamps::decode(bytes.data());

        CHECK(decoded.malfunction == LampStatus::On);
        CHECK(decoded.red_stop == LampStatus::Error);
        CHECK(decoded.amber_warning == LampStatus::On);
        CHECK(decoded.engine_protect == LampStatus::Off);
        CHECK(decoded.malfunction_flash == LampFlash::FastFlash);
        CHECK(decoded.red_stop_flash == LampFlash::SlowFlash);
        CHECK(decoded.amber_warning_flash == LampFlash::Off);
        CHECK(decoded.engine_protect_flash == LampFlash::NotAvailable);
    }

    SUBCASE("all on") {
        DiagnosticLamps lamps;
        lamps.malfunction = LampStatus::On;
        lamps.red_stop = LampStatus::On;
        lamps.amber_warning = LampStatus::On;
        lamps.engine_protect = LampStatus::On;

        auto bytes = lamps.encode();
        DiagnosticLamps decoded = DiagnosticLamps::decode(bytes.data());
        CHECK(decoded.malfunction == LampStatus::On);
        CHECK(decoded.red_stop == LampStatus::On);
        CHECK(decoded.amber_warning == LampStatus::On);
        CHECK(decoded.engine_protect == LampStatus::On);
    }
}
