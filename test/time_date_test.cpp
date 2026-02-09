#include <doctest/doctest.h>
#include <agrobus/j1939/time_date.hpp>

using namespace agrobus::j1939;
using namespace agrobus::net;

// ═════════════════════════════════════════════════════════════════════════════
// Helper Functions for Testing Encoding/Decoding
// ═════════════════════════════════════════════════════════════════════════════

// Encode TimeDate to CAN data (mimics send() logic)
dp::Vector<u8> encode_time_date(const TimeDate &td) {
    dp::Vector<u8> data(8, 0xFF);
    if (td.seconds)
        data[0] = static_cast<u8>(*td.seconds * 4); // SPN 959: 0.25s/bit
    if (td.minutes)
        data[1] = *td.minutes; // SPN 960
    if (td.hours)
        data[2] = *td.hours; // SPN 961
    if (td.month)
        data[3] = *td.month; // SPN 962
    if (td.day)
        data[4] = static_cast<u8>(*td.day * 4); // SPN 963: 0.25 day/bit
    if (td.year)
        data[5] = static_cast<u8>(*td.year - 1985); // SPN 964: offset 1985
    if (td.utc_offset_min)
        data[6] = static_cast<u8>(*td.utc_offset_min + 125); // SPN 1601: offset -125
    if (td.utc_offset_hours)
        data[7] = static_cast<u8>(*td.utc_offset_hours + 125); // SPN 1602: offset -125
    return data;
}

// Decode TimeDate from CAN data (mimics handle_time_date() logic)
TimeDate decode_time_date(const dp::Vector<u8> &data) {
    TimeDate td;
    if (data.size() < 8)
        return td;

    if (data[0] != 0xFF)
        td.seconds = data[0] / 4; // SPN 959: 0.25s/bit
    if (data[1] != 0xFF)
        td.minutes = data[1]; // SPN 960
    if (data[2] != 0xFF)
        td.hours = data[2]; // SPN 961
    if (data[3] != 0xFF)
        td.month = data[3]; // SPN 962
    if (data[4] != 0xFF)
        td.day = data[4] / 4; // SPN 963: 0.25 day/bit
    if (data[5] != 0xFF)
        td.year = static_cast<u16>(data[5]) + 1985; // SPN 964
    if (data[6] != 0xFF)
        td.utc_offset_min = static_cast<i16>(data[6]) - 125; // SPN 1601: minutes
    if (data[7] != 0xFF)
        td.utc_offset_hours = static_cast<i8>(data[7]) - 125; // SPN 1602: hours

    return td;
}

// ═════════════════════════════════════════════════════════════════════════════
// TimeDate Structure Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("TimeDate structure") {
    SUBCASE("default construction") {
        TimeDate td;
        CHECK_FALSE(td.seconds.has_value());
        CHECK_FALSE(td.minutes.has_value());
        CHECK_FALSE(td.hours.has_value());
        CHECK_FALSE(td.day.has_value());
        CHECK_FALSE(td.month.has_value());
        CHECK_FALSE(td.year.has_value());
        CHECK_FALSE(td.utc_offset_min.has_value());
        CHECK_FALSE(td.utc_offset_hours.has_value());
        CHECK(td.timestamp_us == 0);
    }

    SUBCASE("all fields set") {
        TimeDate td;
        td.seconds = 45;
        td.minutes = 30;
        td.hours = 14;
        td.day = 15;
        td.month = 6;
        td.year = 2025;
        td.utc_offset_min = 0;
        td.utc_offset_hours = 0;
        td.timestamp_us = 123456;

        CHECK(*td.seconds == 45);
        CHECK(*td.minutes == 30);
        CHECK(*td.hours == 14);
        CHECK(*td.day == 15);
        CHECK(*td.month == 6);
        CHECK(*td.year == 2025);
        CHECK(*td.utc_offset_min == 0);
        CHECK(*td.utc_offset_hours == 0);
        CHECK(td.timestamp_us == 123456);
    }

    SUBCASE("partial fields set") {
        TimeDate td;
        td.hours = 10;
        td.minutes = 25;
        // Other fields not set

        CHECK(*td.hours == 10);
        CHECK(*td.minutes == 25);
        CHECK_FALSE(td.seconds.has_value());
        CHECK_FALSE(td.day.has_value());
        CHECK_FALSE(td.year.has_value());
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Encoding Tests (PGN 65254 - Time/Date)
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("TimeDate encoding") {
    SUBCASE("full time/date encoding") {
        TimeDate td;
        td.seconds = 30;  // → 120 (30 * 4)
        td.minutes = 15;
        td.hours = 10;
        td.month = 6;
        td.day = 9;       // → 36 (9 * 4)
        td.year = 2026;   // → 41 (2026 - 1985)
        td.utc_offset_min = -60;   // → 65 (-60 + 125)
        td.utc_offset_hours = -1;  // → 124 (-1 + 125)

        auto data = encode_time_date(td);
        CHECK(data.size() == 8);

        CHECK(data[0] == 120);  // seconds * 4
        CHECK(data[1] == 15);   // minutes
        CHECK(data[2] == 10);   // hours
        CHECK(data[3] == 6);    // month
        CHECK(data[4] == 36);   // day * 4
        CHECK(data[5] == 41);   // year - 1985
        CHECK(data[6] == 65);   // utc_offset_min + 125
        CHECK(data[7] == 124);  // utc_offset_hours + 125
    }

    SUBCASE("partial encoding - time only") {
        TimeDate td;
        td.hours = 8;
        td.minutes = 30;
        td.seconds = 45;  // → 180 (45 * 4)
        // Date fields not set

        auto data = encode_time_date(td);

        CHECK(data[0] == 180);  // seconds * 4
        CHECK(data[1] == 30);   // minutes
        CHECK(data[2] == 8);    // hours
        CHECK(data[3] == 0xFF); // month not available
        CHECK(data[4] == 0xFF); // day not available
        CHECK(data[5] == 0xFF); // year not available
        CHECK(data[6] == 0xFF); // utc_offset_min not available
        CHECK(data[7] == 0xFF); // utc_offset_hours not available
    }

    SUBCASE("partial encoding - date only") {
        TimeDate td;
        td.day = 25;      // → 100 (25 * 4)
        td.month = 12;
        td.year = 1985;   // → 0 (1985 - 1985)
        // Time fields not set

        auto data = encode_time_date(td);

        CHECK(data[0] == 0xFF); // seconds not available
        CHECK(data[1] == 0xFF); // minutes not available
        CHECK(data[2] == 0xFF); // hours not available
        CHECK(data[3] == 12);   // month
        CHECK(data[4] == 100);  // day * 4
        CHECK(data[5] == 0);    // year - 1985
    }

    SUBCASE("UTC offset encoding") {
        SUBCASE("positive UTC offset") {
            TimeDate td;
            td.hours = 12;
            td.minutes = 0;
            td.utc_offset_min = 60;      // +1 hour → 185 (60 + 125)
            td.utc_offset_hours = 1;     // → 126 (1 + 125)

            auto data = encode_time_date(td);

            CHECK(data[6] == 185);  // 60 + 125
            CHECK(data[7] == 126);  // 1 + 125
        }

        SUBCASE("negative UTC offset") {
            TimeDate td;
            td.hours = 12;
            td.minutes = 0;
            td.utc_offset_min = -60;     // -1 hour → 65 (-60 + 125)
            td.utc_offset_hours = -5;    // → 120 (-5 + 125)

            auto data = encode_time_date(td);

            CHECK(data[6] == 65);    // -60 + 125
            CHECK(data[7] == 120);  // -5 + 125
        }

        SUBCASE("zero UTC offset") {
            TimeDate td;
            td.hours = 12;
            td.minutes = 0;
            td.utc_offset_min = 0;       // → 125 (0 + 125)
            td.utc_offset_hours = 0;     // → 125 (0 + 125)

            auto data = encode_time_date(td);

            CHECK(data[6] == 125);  // 0 + 125
            CHECK(data[7] == 125);  // 0 + 125
        }
    }

    SUBCASE("edge cases - minimum values") {
        TimeDate td;
        td.seconds = 0;   // → 0
        td.minutes = 0;
        td.hours = 0;
        td.day = 1;       // → 4 (1 * 4)
        td.month = 1;
        td.year = 1985;   // → 0 (1985 - 1985)

        auto data = encode_time_date(td);

        CHECK(data[0] == 0);  // seconds * 4
        CHECK(data[1] == 0);  // minutes
        CHECK(data[2] == 0);  // hours
        CHECK(data[3] == 1);  // month
        CHECK(data[4] == 4);  // day * 4
        CHECK(data[5] == 0);  // year - 1985
    }

    SUBCASE("edge cases - maximum values") {
        TimeDate td;
        td.seconds = 59;  // → 236 (59 * 4)
        td.minutes = 59;
        td.hours = 23;
        td.day = 31;      // → 124 (31 * 4)
        td.month = 12;
        td.year = 2235;   // → 250 (2235 - 1985)

        auto data = encode_time_date(td);

        CHECK(data[0] == 236); // seconds * 4
        CHECK(data[1] == 59);  // minutes
        CHECK(data[2] == 23);  // hours
        CHECK(data[3] == 12);  // month
        CHECK(data[4] == 124); // day * 4
        CHECK(data[5] == 250); // year - 1985
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Decoding Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("TimeDate decoding") {
    SUBCASE("full time/date decoding") {
        dp::Vector<u8> data = {
            120,  // seconds: 30 (120 / 4)
            15,   // minutes: 15
            10,   // hours: 10
            6,    // month: 6
            36,   // day: 9 (36 / 4)
            41,   // year: 2026 (41 + 1985)
            65,   // utc_offset_min: -60 (65 - 125)
            124   // utc_offset_hours: -1 (124 - 125)
        };

        auto td = decode_time_date(data);

        CHECK(td.seconds.has_value());
        CHECK(*td.seconds == 30);
        CHECK(*td.minutes == 15);
        CHECK(*td.hours == 10);
        CHECK(*td.month == 6);
        CHECK(*td.day == 9);
        CHECK(*td.year == 2026);
        CHECK(*td.utc_offset_min == -60);
        CHECK(*td.utc_offset_hours == -1);
    }

    SUBCASE("partial decoding - time only") {
        dp::Vector<u8> data = {
            180,  // seconds: 45 (180 / 4)
            30,   // minutes: 30
            8,    // hours: 8
            0xFF, // month not available
            0xFF, // day not available
            0xFF, // year not available
            0xFF, // utc_offset_min not available
            0xFF  // utc_offset_hours not available
        };

        auto td = decode_time_date(data);

        CHECK(*td.seconds == 45);
        CHECK(*td.minutes == 30);
        CHECK(*td.hours == 8);
        CHECK_FALSE(td.month.has_value());
        CHECK_FALSE(td.day.has_value());
        CHECK_FALSE(td.year.has_value());
        CHECK_FALSE(td.utc_offset_min.has_value());
        CHECK_FALSE(td.utc_offset_hours.has_value());
    }

    SUBCASE("partial decoding - date only") {
        dp::Vector<u8> data = {
            0xFF, // seconds not available
            0xFF, // minutes not available
            0xFF, // hours not available
            12,   // month: 12
            100,  // day: 25 (100 / 4)
            0,    // year: 1985 (0 + 1985)
            0xFF, // utc_offset_min not available
            0xFF  // utc_offset_hours not available
        };

        auto td = decode_time_date(data);

        CHECK_FALSE(td.seconds.has_value());
        CHECK_FALSE(td.minutes.has_value());
        CHECK_FALSE(td.hours.has_value());
        CHECK(*td.month == 12);
        CHECK(*td.day == 25);
        CHECK(*td.year == 1985);
        CHECK_FALSE(td.utc_offset_min.has_value());
        CHECK_FALSE(td.utc_offset_hours.has_value());
    }

    SUBCASE("UTC offset decoding") {
        SUBCASE("positive UTC offset") {
            dp::Vector<u8> data = {0, 0, 12, 1, 4, 40, 185, 126};
            // utc_offset_min: 60 (185 - 125), utc_offset_hours: 1 (126 - 125)

            auto td = decode_time_date(data);

            CHECK(*td.utc_offset_min == 60);
            CHECK(*td.utc_offset_hours == 1);
        }

        SUBCASE("negative UTC offset") {
            dp::Vector<u8> data = {0, 0, 12, 1, 4, 40, 65, 120};
            // utc_offset_min: -60 (65 - 125), utc_offset_hours: -5 (120 - 125)

            auto td = decode_time_date(data);

            CHECK(*td.utc_offset_min == -60);
            CHECK(*td.utc_offset_hours == -5);
        }

        SUBCASE("zero UTC offset") {
            dp::Vector<u8> data = {0, 0, 12, 1, 4, 40, 125, 125};
            // utc_offset_min: 0 (125 - 125), utc_offset_hours: 0 (125 - 125)

            auto td = decode_time_date(data);

            CHECK(*td.utc_offset_min == 0);
            CHECK(*td.utc_offset_hours == 0);
        }
    }

    SUBCASE("edge cases - minimum values") {
        dp::Vector<u8> data = {0, 0, 0, 1, 4, 0, 0xFF, 0xFF};
        // seconds: 0, minutes: 0, hours: 0, month: 1, day: 1, year: 1985

        auto td = decode_time_date(data);

        CHECK(*td.seconds == 0);
        CHECK(*td.minutes == 0);
        CHECK(*td.hours == 0);
        CHECK(*td.month == 1);
        CHECK(*td.day == 1);
        CHECK(*td.year == 1985);
    }

    SUBCASE("edge cases - maximum values") {
        dp::Vector<u8> data = {236, 59, 23, 12, 124, 250, 0xFF, 0xFF};
        // seconds: 59, minutes: 59, hours: 23, month: 12, day: 31, year: 2235

        auto td = decode_time_date(data);

        CHECK(*td.seconds == 59);
        CHECK(*td.minutes == 59);
        CHECK(*td.hours == 23);
        CHECK(*td.month == 12);
        CHECK(*td.day == 31);
        CHECK(*td.year == 2235);
    }

    SUBCASE("invalid message length") {
        dp::Vector<u8> data = {120, 15, 10}; // Too short

        auto td = decode_time_date(data);

        // Should return empty TimeDate when data is too short
        CHECK_FALSE(td.seconds.has_value());
        CHECK_FALSE(td.minutes.has_value());
        CHECK_FALSE(td.hours.has_value());
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Round-trip Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("TimeDate round-trip encoding/decoding") {
    SUBCASE("full time/date round-trip") {
        TimeDate original;
        original.seconds = 42;
        original.minutes = 18;
        original.hours = 16;
        original.day = 9;
        original.month = 2;
        original.year = 2026;
        original.utc_offset_min = -60;
        original.utc_offset_hours = -1;

        auto data = encode_time_date(original);
        auto decoded = decode_time_date(data);

        CHECK(*decoded.seconds == *original.seconds);
        CHECK(*decoded.minutes == *original.minutes);
        CHECK(*decoded.hours == *original.hours);
        CHECK(*decoded.day == *original.day);
        CHECK(*decoded.month == *original.month);
        CHECK(*decoded.year == *original.year);
        CHECK(*decoded.utc_offset_min == *original.utc_offset_min);
        CHECK(*decoded.utc_offset_hours == *original.utc_offset_hours);
    }

    SUBCASE("partial round-trip - time only") {
        TimeDate original;
        original.seconds = 15;
        original.minutes = 30;
        original.hours = 9;

        auto data = encode_time_date(original);
        auto decoded = decode_time_date(data);

        CHECK(*decoded.seconds == *original.seconds);
        CHECK(*decoded.minutes == *original.minutes);
        CHECK(*decoded.hours == *original.hours);
        CHECK_FALSE(decoded.day.has_value());
        CHECK_FALSE(decoded.month.has_value());
        CHECK_FALSE(decoded.year.has_value());
    }

    SUBCASE("various date combinations") {
        struct TestCase {
            u8 day;
            u8 month;
            u16 year;
        };

        dp::Vector<TestCase> cases = {
            {1, 1, 1985},      // Minimum date
            {15, 6, 2025},     // Mid date
            {31, 12, 2235},    // Maximum date
            {29, 2, 2024},     // Leap year
            {28, 2, 2023},     // Non-leap year
        };

        for (const auto &tc : cases) {
            TimeDate original;
            original.day = tc.day;
            original.month = tc.month;
            original.year = tc.year;

            auto data = encode_time_date(original);
            auto decoded = decode_time_date(data);

            CHECK(*decoded.day == tc.day);
            CHECK(*decoded.month == tc.month);
            CHECK(*decoded.year == tc.year);
        }
    }

    SUBCASE("various time combinations") {
        struct TestCase {
            u8 seconds;
            u8 minutes;
            u8 hours;
        };

        dp::Vector<TestCase> cases = {
            {0, 0, 0},       // Midnight
            {59, 59, 23},    // End of day
            {30, 15, 12},    // Noon
            {45, 30, 6},     // Morning
            {15, 45, 18},    // Evening
        };

        for (const auto &tc : cases) {
            TimeDate original;
            original.seconds = tc.seconds;
            original.minutes = tc.minutes;
            original.hours = tc.hours;

            auto data = encode_time_date(original);
            auto decoded = decode_time_date(data);

            CHECK(*decoded.seconds == tc.seconds);
            CHECK(*decoded.minutes == tc.minutes);
            CHECK(*decoded.hours == tc.hours);
        }
    }

    SUBCASE("various UTC offsets") {
        struct TestCase {
            i16 utc_offset_min;  // Fractional hour offset in minutes (range: -125 to +130)
            i8 utc_offset_hours;  // Hour offset (range: -125 to +125)
        };

        // Note: SPN 1601 is 1 byte with offset -125, so range is -125 to +130 minutes
        // This represents the fractional part of timezone offset (e.g., 30 in UTC+5:30)
        dp::Vector<TestCase> cases = {
            {0, 0},          // UTC
            {0, 1},          // UTC+1
            {0, -1},         // UTC-1
            {30, 5},         // UTC+5:30 (India) - 30 minute fractional offset
            {0, -5},         // UTC-5 (EST)
            {0, 10},         // UTC+10 (Australia)
            {0, -8},         // UTC-8 (PST)
            {45, 5},         // UTC+5:45 (Nepal) - 45 minute fractional offset
            {-30, 3},        // UTC+2:30 - negative fractional offset
        };

        for (const auto &tc : cases) {
            TimeDate original;
            original.hours = 12;
            original.minutes = 0;
            original.utc_offset_min = tc.utc_offset_min;
            original.utc_offset_hours = tc.utc_offset_hours;

            auto data = encode_time_date(original);
            auto decoded = decode_time_date(data);

            CHECK(*decoded.utc_offset_min == tc.utc_offset_min);
            CHECK(*decoded.utc_offset_hours == tc.utc_offset_hours);
        }
    }
}
