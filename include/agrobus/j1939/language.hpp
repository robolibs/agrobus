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

    // ─── Unit systems ────────────────────────────────────────────────────────────
    enum class DistanceUnit : u8 { Metric = 0, Imperial = 1, US = 2 };
    enum class AreaUnit : u8 { Metric = 0, Imperial = 1, US = 2 };
    enum class VolumeUnit : u8 { Metric = 0, Imperial = 1, US = 2 };
    enum class MassUnit : u8 { Metric = 0, Imperial = 1, US = 2 };
    enum class TemperatureUnit : u8 { Metric = 0, Imperial = 1 };
    enum class PressureUnit : u8 { Metric = 0, Imperial = 1 };
    enum class ForceUnit : u8 { Metric = 0, Imperial = 1 };
    enum class TimeFormat : u8 { TwentyFourHour = 0, TwelveHour = 1 };
    enum class DateFormat : u8 { DDMMYYYY = 0, MMDDYYYY = 1, YYYYMMDD = 2 };
    enum class DecimalSymbol : u8 { Comma = 0, Period = 1 };

    // ─── Language command data ───────────────────────────────────────────────────
    struct LanguageData {
        dp::Array<char, 2> language_code = {'e', 'n'};
        DecimalSymbol decimal = DecimalSymbol::Period;
        TimeFormat time_format = TimeFormat::TwentyFourHour;
        DateFormat date_format = DateFormat::DDMMYYYY;
        DistanceUnit distance = DistanceUnit::Metric;
        AreaUnit area = AreaUnit::Metric;
        VolumeUnit volume = VolumeUnit::Metric;
        MassUnit mass = MassUnit::Metric;
        TemperatureUnit temperature = TemperatureUnit::Metric;
        PressureUnit pressure = PressureUnit::Metric;
        ForceUnit force = ForceUnit::Metric;
    };

    // ─── Language command interface ──────────────────────────────────────────────
    class LanguageInterface {
        IsoNet &net_;
        InternalCF *cf_;
        dp::Optional<LanguageData> latest_;

      public:
        LanguageInterface(IsoNet &net, InternalCF *cf) : net_(net), cf_(cf) {}

        Result<void> initialize() {
            if (!cf_) {
                return Result<void>::err(Error::invalid_state("control function not set"));
            }
            net_.register_pgn_callback(PGN_LANGUAGE_COMMAND, [this](const Message &msg) { handle_language(msg); });
            echo::category("isobus.language").debug("initialized");
            return {};
        }

        dp::Optional<LanguageData> latest() const noexcept { return latest_; }

        Result<void> send(const LanguageData &ld) {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = static_cast<u8>(ld.language_code[0]);
            data[1] = static_cast<u8>(ld.language_code[1]);
            data[2] = (static_cast<u8>(ld.decimal) << 6) | (static_cast<u8>(ld.time_format) << 4) |
                      (static_cast<u8>(ld.date_format) << 2);
            data[3] = (static_cast<u8>(ld.distance)) | (static_cast<u8>(ld.area) << 2) |
                      (static_cast<u8>(ld.volume) << 4) | (static_cast<u8>(ld.mass) << 6);
            data[4] = (static_cast<u8>(ld.temperature)) | (static_cast<u8>(ld.pressure) << 2) |
                      (static_cast<u8>(ld.force) << 4);
            data[5] = 0xFF;
            data[6] = 0xFF;
            data[7] = 0xFF;
            return net_.send(PGN_LANGUAGE_COMMAND, data, cf_);
        }

        Event<const LanguageData &> on_language;

      private:
        void handle_language(const Message &msg) {
            echo::category("isobus.language").trace("language command received");
            if (msg.data.size() < 8)
                return;

            LanguageData ld;
            ld.language_code[0] = static_cast<char>(msg.data[0]);
            ld.language_code[1] = static_cast<char>(msg.data[1]);
            ld.decimal = static_cast<DecimalSymbol>((msg.data[2] >> 6) & 0x03);
            ld.time_format = static_cast<TimeFormat>((msg.data[2] >> 4) & 0x03);
            ld.date_format = static_cast<DateFormat>((msg.data[2] >> 2) & 0x03);
            ld.distance = static_cast<DistanceUnit>(msg.data[3] & 0x03);
            ld.area = static_cast<AreaUnit>((msg.data[3] >> 2) & 0x03);
            ld.volume = static_cast<VolumeUnit>((msg.data[3] >> 4) & 0x03);
            ld.mass = static_cast<MassUnit>((msg.data[3] >> 6) & 0x03);
            ld.temperature = static_cast<TemperatureUnit>(msg.data[4] & 0x03);
            ld.pressure = static_cast<PressureUnit>((msg.data[4] >> 2) & 0x03);
            ld.force = static_cast<ForceUnit>((msg.data[4] >> 4) & 0x03);

            latest_ = ld;
            on_language.emit(ld);
        }
    };
} // namespace agrobus::j1939
