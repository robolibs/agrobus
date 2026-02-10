#include <doctest/doctest.h>
#include <agrobus/j1939/language.hpp>

using namespace agrobus::j1939;

// Helper functions to test encoding/decoding logic
dp::Vector<u8> encode_language(const LanguageData &ld) {
    dp::Vector<u8> data(8, 0xFF);
    data[0] = static_cast<u8>(ld.language_code[0]);
    data[1] = static_cast<u8>(ld.language_code[1]);
    data[2] = (static_cast<u8>(ld.decimal) << 6) | (static_cast<u8>(ld.time_format) << 4) |
              (static_cast<u8>(ld.date_format) << 2);
    data[3] = (static_cast<u8>(ld.distance)) | (static_cast<u8>(ld.area) << 2) |
              (static_cast<u8>(ld.volume) << 4) | (static_cast<u8>(ld.mass) << 6);
    data[4] = (static_cast<u8>(ld.temperature)) | (static_cast<u8>(ld.pressure) << 2) |
              (static_cast<u8>(ld.force) << 4);
    return data;
}

LanguageData decode_language(const dp::Vector<u8> &data) {
    LanguageData ld;
    if (data.size() < 5) return ld;
    
    ld.language_code[0] = static_cast<char>(data[0]);
    ld.language_code[1] = static_cast<char>(data[1]);
    ld.decimal = static_cast<DecimalSymbol>((data[2] >> 6) & 0x03);
    ld.time_format = static_cast<TimeFormat>((data[2] >> 4) & 0x03);
    ld.date_format = static_cast<DateFormat>((data[2] >> 2) & 0x03);
    ld.distance = static_cast<DistanceUnit>(data[3] & 0x03);
    ld.area = static_cast<AreaUnit>((data[3] >> 2) & 0x03);
    ld.volume = static_cast<VolumeUnit>((data[3] >> 4) & 0x03);
    ld.mass = static_cast<MassUnit>((data[3] >> 6) & 0x03);
    ld.temperature = static_cast<TemperatureUnit>(data[4] & 0x03);
    ld.pressure = static_cast<PressureUnit>((data[4] >> 2) & 0x03);
    ld.force = static_cast<ForceUnit>((data[4] >> 4) & 0x03);
    
    return ld;
}

TEST_CASE("LanguageData structure") {
    SUBCASE("default construction - English/Metric") {
        LanguageData ld;
        CHECK(ld.language_code[0] == 'e');
        CHECK(ld.language_code[1] == 'n');
        CHECK(ld.decimal == DecimalSymbol::Period);
        CHECK(ld.time_format == TimeFormat::TwentyFourHour);
        CHECK(ld.date_format == DateFormat::DDMMYYYY);
        CHECK(ld.distance == DistanceUnit::Metric);
        CHECK(ld.area == AreaUnit::Metric);
        CHECK(ld.volume == VolumeUnit::Metric);
        CHECK(ld.mass == MassUnit::Metric);
        CHECK(ld.temperature == TemperatureUnit::Metric);
        CHECK(ld.pressure == PressureUnit::Metric);
        CHECK(ld.force == ForceUnit::Metric);
    }
}

TEST_CASE("Language encoding") {
    SUBCASE("English metric system") {
        LanguageData ld;
        ld.language_code[0] = 'e';
        ld.language_code[1] = 'n';
        ld.decimal = DecimalSymbol::Period;
        ld.time_format = TimeFormat::TwentyFourHour;
        ld.date_format = DateFormat::DDMMYYYY;
        ld.distance = DistanceUnit::Metric;
        ld.area = AreaUnit::Metric;
        ld.volume = VolumeUnit::Metric;
        ld.mass = MassUnit::Metric;
        ld.temperature = TemperatureUnit::Metric;
        ld.pressure = PressureUnit::Metric;
        ld.force = ForceUnit::Metric;

        auto data = encode_language(ld);
        CHECK(data[0] == 'e');
        CHECK(data[1] == 'n');
        // Byte 2: decimal(6-7), time(4-5), date(2-3)
        CHECK((data[2] & 0xC0) == 0x40); // Period = 1 << 6
        CHECK((data[2] & 0x30) == 0x00); // 24h = 0 << 4
        CHECK((data[2] & 0x0C) == 0x00); // DDMMYYYY = 0 << 2
        // Byte 3: distance(0-1), area(2-3), volume(4-5), mass(6-7) - all 0 for metric
        CHECK(data[3] == 0x00);
        // Byte 4: temp(0-1), pressure(2-3), force(4-5) - all 0 for metric
        CHECK(data[4] == 0x00);
    }

    SUBCASE("US English imperial system") {
        LanguageData ld;
        ld.language_code[0] = 'e';
        ld.language_code[1] = 'n';
        ld.decimal = DecimalSymbol::Period;
        ld.time_format = TimeFormat::TwelveHour;
        ld.date_format = DateFormat::MMDDYYYY;
        ld.distance = DistanceUnit::Imperial;
        ld.area = AreaUnit::Imperial;
        ld.volume = VolumeUnit::Imperial;
        ld.mass = MassUnit::Imperial;
        ld.temperature = TemperatureUnit::Imperial;
        ld.pressure = PressureUnit::Imperial;
        ld.force = ForceUnit::Imperial;

        auto data = encode_language(ld);
        CHECK(data[0] == 'e');
        CHECK(data[1] == 'n');
        // Byte 2: Period(01) + 12h(01) + MMDDYYYY(01)
        CHECK((data[2] & 0xC0) == 0x40); // Period = 1 << 6
        CHECK((data[2] & 0x30) == 0x10); // 12h = 1 << 4
        CHECK((data[2] & 0x0C) == 0x04); // MMDDYYYY = 1 << 2
        // Byte 3: all imperial (1s)
        CHECK(data[3] == 0x55); // 01010101 binary
        // Byte 4: all imperial (1s)
        CHECK(data[4] == 0x15); // 00010101 binary
    }

    SUBCASE("German metric with comma decimal") {
        LanguageData ld;
        ld.language_code[0] = 'd';
        ld.language_code[1] = 'e';
        ld.decimal = DecimalSymbol::Comma;
        ld.time_format = TimeFormat::TwentyFourHour;
        ld.date_format = DateFormat::DDMMYYYY;
        ld.distance = DistanceUnit::Metric;

        auto data = encode_language(ld);
        CHECK(data[0] == 'd');
        CHECK(data[1] == 'e');
        CHECK((data[2] & 0xC0) == 0x00); // Comma = 0 << 6
    }

    SUBCASE("Japanese with YYYYMMDD date format") {
        LanguageData ld;
        ld.language_code[0] = 'j';
        ld.language_code[1] = 'a';
        ld.date_format = DateFormat::YYYYMMDD;

        auto data = encode_language(ld);
        CHECK(data[0] == 'j');
        CHECK(data[1] == 'a');
        CHECK((data[2] & 0x0C) == 0x08); // YYYYMMDD = 2 << 2
    }
}

TEST_CASE("Language decoding") {
    SUBCASE("decode English metric") {
        dp::Vector<u8> data = {'e', 'n', 0x40, 0x00, 0x00, 0xFF, 0xFF, 0xFF};
        
        auto ld = decode_language(data);
        CHECK(ld.language_code[0] == 'e');
        CHECK(ld.language_code[1] == 'n');
        CHECK(ld.decimal == DecimalSymbol::Period);
        CHECK(ld.time_format == TimeFormat::TwentyFourHour);
        CHECK(ld.date_format == DateFormat::DDMMYYYY);
        CHECK(ld.distance == DistanceUnit::Metric);
        CHECK(ld.temperature == TemperatureUnit::Metric);
    }

    SUBCASE("decode US imperial") {
        dp::Vector<u8> data = {'e', 'n', 0x54, 0x55, 0x15, 0xFF, 0xFF, 0xFF};
        // Byte 2: 01010100 = Period(01) + 12h(01) + MMDDYYYY(01) + 00
        // Byte 3: 01010101 = Imperial for distance, area, volume, mass
        // Byte 4: 00010101 = Imperial for temp, pressure, force
        
        auto ld = decode_language(data);
        CHECK(ld.language_code[0] == 'e');
        CHECK(ld.language_code[1] == 'n');
        CHECK(ld.decimal == DecimalSymbol::Period);
        CHECK(ld.time_format == TimeFormat::TwelveHour);
        CHECK(ld.date_format == DateFormat::MMDDYYYY);
        CHECK(ld.distance == DistanceUnit::Imperial);
        CHECK(ld.area == AreaUnit::Imperial);
        CHECK(ld.volume == VolumeUnit::Imperial);
        CHECK(ld.mass == MassUnit::Imperial);
        CHECK(ld.temperature == TemperatureUnit::Imperial);
        CHECK(ld.pressure == PressureUnit::Imperial);
        CHECK(ld.force == ForceUnit::Imperial);
    }

    SUBCASE("decode mixed units") {
        // Distance: US(2), Area: Imperial(1), Volume: Metric(0), Mass: Metric(0)
        // Temp: Imperial(1), Pressure: Metric(0), Force: Metric(0)
        dp::Vector<u8> data = {'f', 'r', 0x00, 0x06, 0x01, 0xFF, 0xFF, 0xFF};
        // Byte 3: 00000110 = US(10) + Imperial(01) + Metric(00) + Metric(00)
        // Byte 4: 00000001 = Imperial(01) + Metric(00) + Metric(00)
        
        auto ld = decode_language(data);
        CHECK(ld.language_code[0] == 'f');
        CHECK(ld.language_code[1] == 'r');
        CHECK(ld.distance == DistanceUnit::US);
        CHECK(ld.area == AreaUnit::Imperial);
        CHECK(ld.volume == VolumeUnit::Metric);
        CHECK(ld.mass == MassUnit::Metric);
        CHECK(ld.temperature == TemperatureUnit::Imperial);
        CHECK(ld.pressure == PressureUnit::Metric);
        CHECK(ld.force == ForceUnit::Metric);
    }
}

TEST_CASE("Language round-trip") {
    SUBCASE("English metric round-trip") {
        LanguageData original;
        original.language_code[0] = 'e';
        original.language_code[1] = 'n';
        original.decimal = DecimalSymbol::Period;
        original.time_format = TimeFormat::TwentyFourHour;
        original.date_format = DateFormat::DDMMYYYY;
        original.distance = DistanceUnit::Metric;
        original.area = AreaUnit::Metric;
        original.volume = VolumeUnit::Metric;
        original.mass = MassUnit::Metric;
        original.temperature = TemperatureUnit::Metric;
        original.pressure = PressureUnit::Metric;
        original.force = ForceUnit::Metric;

        auto encoded = encode_language(original);
        auto decoded = decode_language(encoded);

        CHECK(decoded.language_code[0] == original.language_code[0]);
        CHECK(decoded.language_code[1] == original.language_code[1]);
        CHECK(decoded.decimal == original.decimal);
        CHECK(decoded.time_format == original.time_format);
        CHECK(decoded.date_format == original.date_format);
        CHECK(decoded.distance == original.distance);
        CHECK(decoded.area == original.area);
        CHECK(decoded.volume == original.volume);
        CHECK(decoded.mass == original.mass);
        CHECK(decoded.temperature == original.temperature);
        CHECK(decoded.pressure == original.pressure);
        CHECK(decoded.force == original.force);
    }

    SUBCASE("various language codes") {
        struct TestCase {
            char lang0, lang1;
        };

        dp::Vector<TestCase> cases = {
            {'e', 'n'}, // English
            {'d', 'e'}, // German
            {'f', 'r'}, // French
            {'e', 's'}, // Spanish
            {'i', 't'}, // Italian
            {'j', 'a'}, // Japanese
            {'z', 'h'}, // Chinese
            {'r', 'u'}, // Russian
            {'p', 't'}, // Portuguese
            {'n', 'l'}, // Dutch
        };

        for (const auto &tc : cases) {
            LanguageData original;
            original.language_code[0] = tc.lang0;
            original.language_code[1] = tc.lang1;

            auto encoded = encode_language(original);
            auto decoded = decode_language(encoded);

            CHECK(decoded.language_code[0] == tc.lang0);
            CHECK(decoded.language_code[1] == tc.lang1);
        }
    }

    SUBCASE("all unit combinations") {
        struct UnitConfig {
            DistanceUnit dist;
            AreaUnit area;
            VolumeUnit vol;
            MassUnit mass;
            TemperatureUnit temp;
            PressureUnit pres;
            ForceUnit force;
        };

        dp::Vector<UnitConfig> configs = {
            {DistanceUnit::Metric, AreaUnit::Metric, VolumeUnit::Metric, MassUnit::Metric,
             TemperatureUnit::Metric, PressureUnit::Metric, ForceUnit::Metric},
            {DistanceUnit::Imperial, AreaUnit::Imperial, VolumeUnit::Imperial, MassUnit::Imperial,
             TemperatureUnit::Imperial, PressureUnit::Imperial, ForceUnit::Imperial},
            {DistanceUnit::US, AreaUnit::US, VolumeUnit::US, MassUnit::US,
             TemperatureUnit::Metric, PressureUnit::Metric, ForceUnit::Metric},
            {DistanceUnit::Metric, AreaUnit::Imperial, VolumeUnit::US, MassUnit::Metric,
             TemperatureUnit::Imperial, PressureUnit::Metric, ForceUnit::Imperial},
        };

        for (const auto &cfg : configs) {
            LanguageData original;
            original.language_code[0] = 't';
            original.language_code[1] = 's';
            original.distance = cfg.dist;
            original.area = cfg.area;
            original.volume = cfg.vol;
            original.mass = cfg.mass;
            original.temperature = cfg.temp;
            original.pressure = cfg.pres;
            original.force = cfg.force;

            auto encoded = encode_language(original);
            auto decoded = decode_language(encoded);

            CHECK(decoded.distance == cfg.dist);
            CHECK(decoded.area == cfg.area);
            CHECK(decoded.volume == cfg.vol);
            CHECK(decoded.mass == cfg.mass);
            CHECK(decoded.temperature == cfg.temp);
            CHECK(decoded.pressure == cfg.pres);
            CHECK(decoded.force == cfg.force);
        }
    }

    SUBCASE("format combinations") {
        struct FormatConfig {
            DecimalSymbol dec;
            TimeFormat time;
            DateFormat date;
        };

        dp::Vector<FormatConfig> formats = {
            {DecimalSymbol::Comma, TimeFormat::TwentyFourHour, DateFormat::DDMMYYYY},
            {DecimalSymbol::Period, TimeFormat::TwelveHour, DateFormat::MMDDYYYY},
            {DecimalSymbol::Comma, TimeFormat::TwelveHour, DateFormat::YYYYMMDD},
            {DecimalSymbol::Period, TimeFormat::TwentyFourHour, DateFormat::YYYYMMDD},
        };

        for (const auto &fmt : formats) {
            LanguageData original;
            original.decimal = fmt.dec;
            original.time_format = fmt.time;
            original.date_format = fmt.date;

            auto encoded = encode_language(original);
            auto decoded = decode_language(encoded);

            CHECK(decoded.decimal == fmt.dec);
            CHECK(decoded.time_format == fmt.time);
            CHECK(decoded.date_format == fmt.date);
        }
    }
}

TEST_CASE("Bit packing verification") {
    SUBCASE("byte 2 bit layout") {
        LanguageData ld;
        ld.decimal = DecimalSymbol::Period;       // bits 6-7: 01
        ld.time_format = TimeFormat::TwelveHour;  // bits 4-5: 01
        ld.date_format = DateFormat::YYYYMMDD;    // bits 2-3: 10

        auto data = encode_language(ld);

        // Expected: 01 01 10 00 = 0x54
        CHECK((data[2] & 0xC0) == 0x40); // decimal: 01 << 6
        CHECK((data[2] & 0x30) == 0x10); // time: 01 << 4
        CHECK((data[2] & 0x0C) == 0x08); // date: 10 << 2
    }

    SUBCASE("byte 3 bit layout") {
        LanguageData ld;
        ld.distance = DistanceUnit::Imperial; // bits 0-1: 01
        ld.area = AreaUnit::US;              // bits 2-3: 10
        ld.volume = VolumeUnit::Metric;      // bits 4-5: 00
        ld.mass = MassUnit::Imperial;        // bits 6-7: 01

        auto data = encode_language(ld);

        // Expected: 01 00 10 01 = 0x49
        CHECK((data[3] & 0x03) == 0x01); // distance: 01
        CHECK((data[3] & 0x0C) == 0x08); // area: 10 << 2
        CHECK((data[3] & 0x30) == 0x00); // volume: 00 << 4
        CHECK((data[3] & 0xC0) == 0x40); // mass: 01 << 6
    }

    SUBCASE("byte 4 bit layout") {
        LanguageData ld;
        ld.temperature = TemperatureUnit::Imperial; // bits 0-1: 01
        ld.pressure = PressureUnit::Metric;         // bits 2-3: 00
        ld.force = ForceUnit::Imperial;             // bits 4-5: 01

        auto data = encode_language(ld);

        // Expected: xx 01 00 01 = 0x11
        CHECK((data[4] & 0x03) == 0x01); // temp: 01
        CHECK((data[4] & 0x0C) == 0x00); // pressure: 00 << 2
        CHECK((data[4] & 0x30) == 0x10); // force: 01 << 4
    }
}
