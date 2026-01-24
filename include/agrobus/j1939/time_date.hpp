#pragma once

#include <agrobus/net/constants.hpp>
#include <agrobus/net/event.hpp>
#include <agrobus/net/message.hpp>
#include <agrobus/net/network_manager.hpp>
#include <agrobus/net/types.hpp>
#include <datapod/datapod.hpp>
#include <echo/echo.hpp>

namespace agrobus::j1939 {
    using namespace agrobus::net;

    // ─── Time/Date data (J1939-71 PGN 65254) ─────────────────────────────────────
    struct TimeDate {
        dp::Optional<u8> seconds;          // 0-59 (SPN 959: 0.25s resolution)
        dp::Optional<u8> minutes;          // 0-59
        dp::Optional<u8> hours;            // 0-23
        dp::Optional<u8> day;              // 1-31 (SPN 963: 0.25 day/bit resolution)
        dp::Optional<u8> month;            // 1-12
        dp::Optional<u16> year;            // 1985+ (SPN 964: offset 1985)
        dp::Optional<i16> utc_offset_min;  // UTC offset in minutes (SPN 1601: 1 min/bit, offset -125)
        dp::Optional<i8> utc_offset_hours; // UTC hour offset (SPN 1602: 1 hr/bit, offset -125)
        u64 timestamp_us = 0;
    };

    // ─── Time/Date interface ─────────────────────────────────────────────────────
    class TimeDateInterface {
        IsoNet &net_;
        InternalCF *cf_;
        dp::Optional<TimeDate> latest_;

      public:
        TimeDateInterface(IsoNet &net, InternalCF *cf) : net_(net), cf_(cf) {}

        Result<void> initialize() {
            if (!cf_) {
                return Result<void>::err(Error::invalid_state("control function not set"));
            }
            net_.register_pgn_callback(PGN_TIME_DATE, [this](const Message &msg) { handle_time_date(msg); });
            echo::category("isobus.timedate").debug("initialized");
            return {};
        }

        dp::Optional<TimeDate> latest() const noexcept { return latest_; }

        Result<void> send(const TimeDate &td) {
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
            return net_.send(PGN_TIME_DATE, data, cf_);
        }

        Event<const TimeDate &> on_time_date;

      private:
        void handle_time_date(const Message &msg) {
            echo::category("isobus.timedate").trace("time/date received");
            if (msg.data.size() < 8)
                return;

            TimeDate td;
            td.timestamp_us = msg.timestamp_us;

            if (msg.data[0] != 0xFF)
                td.seconds = msg.data[0] / 4; // SPN 959: 0.25s/bit
            if (msg.data[1] != 0xFF)
                td.minutes = msg.data[1]; // SPN 960
            if (msg.data[2] != 0xFF)
                td.hours = msg.data[2]; // SPN 961
            if (msg.data[3] != 0xFF)
                td.month = msg.data[3]; // SPN 962
            if (msg.data[4] != 0xFF)
                td.day = msg.data[4] / 4; // SPN 963: 0.25 day/bit
            if (msg.data[5] != 0xFF)
                td.year = static_cast<u16>(msg.data[5]) + 1985; // SPN 964
            if (msg.data[6] != 0xFF)
                td.utc_offset_min = static_cast<i16>(msg.data[6]) - 125; // SPN 1601: minutes
            if (msg.data[7] != 0xFF)
                td.utc_offset_hours = static_cast<i8>(msg.data[7]) - 125; // SPN 1602: hours

            latest_ = td;
            on_time_date.emit(td);
        }
    };
} // namespace agrobus::j1939
