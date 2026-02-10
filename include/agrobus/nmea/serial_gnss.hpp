#pragma once

#include "definitions.hpp"
#include "position.hpp"
#include <agrobus/net/error.hpp>
#include <agrobus/net/event.hpp>
#include <agrobus/net/types.hpp>
#include <concord/concord.hpp>
#include <cstdlib>
#include <cstring>
#include <datapod/datapod.hpp>
#include <echo/echo.hpp>
#include <wirebit/serial/serial_endpoint.hpp>

namespace agrobus::nmea {
    using namespace agrobus::net;

    // ─── Serial GNSS configuration ─────────────────────────────────────────────
    struct SerialGNSSConfig {
        u32 baud = 115200;
        u8 data_bits = 8;
        u8 stop_bits = 1;
        char parity = 'N';

        SerialGNSSConfig &baud_rate(u32 rate) {
            baud = rate;
            return *this;
        }
        SerialGNSSConfig &data(u8 bits) {
            data_bits = bits;
            return *this;
        }
        SerialGNSSConfig &parity_mode(char p) {
            parity = p;
            return *this;
        }
        SerialGNSSConfig &stop(u8 bits) {
            stop_bits = bits;
            return *this;
        }
    };

    // ─── Serial GNSS Interface ──────────────────────────────────────────────────
    // Reads NMEA-0183 sentences from a wirebit::SerialEndpoint (UART/PTY) and
    // parses them into GNSSPosition updates.
    //
    // Supported sentences: $GxGGA, $GxRMC, $GxVTG, $GxGSA
    //
    // Usage:
    //   auto pty = wirebit::PtyLink::create();
    //   auto link = std::make_shared<wirebit::PtyLink>(std::move(pty.value()));
    //   wirebit::SerialEndpoint serial(link, {.baud = 115200}, 1);
    //   nmea::SerialGNSS gnss(serial);
    //   gnss.on_position.subscribe([](const GNSSPosition& pos) { ... });
    //   gnss.update(); // call periodically
    class SerialGNSS {
        wirebit::SerialEndpoint &serial_;
        dp::String line_buffer_;
        GNSSPosition latest_;

      public:
        explicit SerialGNSS(wirebit::SerialEndpoint &serial) : serial_(serial) {}

        // Poll for new NMEA data and parse any complete sentences
        void update() {
            auto result = serial_.recv();
            if (!result.is_ok())
                return;

            auto &data = result.value();
            for (usize i = 0; i < data.size(); ++i) {
                char c = static_cast<char>(data[i]);
                if (c == '\n' || c == '\r') {
                    if (!line_buffer_.empty()) {
                        parse_sentence(line_buffer_);
                        line_buffer_.clear();
                    }
                } else {
                    line_buffer_ += c;
                }
            }
        }

        dp::Optional<GNSSPosition> latest_position() const noexcept {
            if (latest_.has_fix())
                return latest_;
            return dp::nullopt;
        }

        // Events
        Event<const GNSSPosition &> on_position;
        Event<f64> on_cog; // Course over ground (radians)
        Event<f64> on_sog; // Speed over ground (m/s)

      private:
        void parse_sentence(const dp::String &sentence) {
            if (sentence.size() < 6 || sentence[0] != '$')
                return;

            // Verify checksum if present
            auto star = sentence.find('*');
            if (star != dp::String::npos && star + 2 < sentence.size()) {
                u8 computed = 0;
                for (usize i = 1; i < star; ++i)
                    computed ^= static_cast<u8>(sentence[i]);
                u8 expected = static_cast<u8>(std::strtol(sentence.c_str() + star + 1, nullptr, 16));
                if (computed != expected) {
                    echo::category("isobus.nmea.serial").warn("Checksum mismatch: ", sentence);
                    return;
                }
            }

            // Extract sentence type (skip talker ID: $GP, $GN, $GL, etc.)
            dp::String type = sentence.substr(3, 3);

            if (type == "GGA")
                parse_gga(sentence);
            else if (type == "RMC")
                parse_rmc(sentence);
            else if (type == "VTG")
                parse_vtg(sentence);
            else if (type == "GSA")
                parse_gsa(sentence);
        }

        // Split NMEA sentence into fields
        static dp::Vector<dp::String> split_fields(const dp::String &sentence) {
            dp::Vector<dp::String> fields;
            dp::String current;
            for (usize i = 0; i < sentence.size(); ++i) {
                char c = sentence[i];
                if (c == ',' || c == '*') {
                    fields.push_back(current);
                    current.clear();
                    if (c == '*')
                        break;
                } else {
                    current += c;
                }
            }
            if (!current.empty())
                fields.push_back(current);
            return fields;
        }

        static f64 parse_lat(const dp::String &value, const dp::String &dir) {
            if (value.empty())
                return 0.0;
            // Format: DDMM.MMMMM
            f64 raw = std::atof(value.c_str());
            i32 degrees = static_cast<i32>(raw / 100.0);
            f64 minutes = raw - (degrees * 100.0);
            f64 result = degrees + minutes / 60.0;
            if (dir == "S")
                result = -result;
            return result;
        }

        static f64 parse_lon(const dp::String &value, const dp::String &dir) {
            if (value.empty())
                return 0.0;
            // Format: DDDMM.MMMMM
            f64 raw = std::atof(value.c_str());
            i32 degrees = static_cast<i32>(raw / 100.0);
            f64 minutes = raw - (degrees * 100.0);
            f64 result = degrees + minutes / 60.0;
            if (dir == "W")
                result = -result;
            return result;
        }

        // $GxGGA - Global Positioning System Fix Data
        void parse_gga(const dp::String &sentence) {
            auto fields = split_fields(sentence);
            if (fields.size() < 15)
                return;

            // Field 6: Fix quality
            u8 quality = fields[6].empty() ? 0 : static_cast<u8>(std::atoi(fields[6].c_str()));
            if (quality == 0) {
                latest_.fix_type = GNSSFixType::NoFix;
                return;
            }

            latest_.wgs = concord::earth::WGS(parse_lat(fields[2], fields[3]), parse_lon(fields[4], fields[5]),
                                              fields[9].empty() ? 0.0 : std::atof(fields[9].c_str()));

            switch (quality) {
            case 1:
                latest_.fix_type = GNSSFixType::GNSSFix;
                break;
            case 2:
                latest_.fix_type = GNSSFixType::DGNSSFix;
                break;
            case 4:
                latest_.fix_type = GNSSFixType::RTKFixed;
                break;
            case 5:
                latest_.fix_type = GNSSFixType::RTKFloat;
                break;
            default:
                latest_.fix_type = GNSSFixType::GNSSFix;
                break;
            }

            latest_.satellites_used = fields[7].empty() ? 0 : static_cast<u8>(std::atoi(fields[7].c_str()));
            if (!fields[8].empty())
                latest_.hdop = std::atof(fields[8].c_str());
            if (!fields[11].empty())
                latest_.geoidal_separation_m = std::atof(fields[11].c_str());

            on_position.emit(latest_);
            echo::category("isobus.nmea.serial")
                .trace("GGA: ", latest_.wgs.latitude, ", ", latest_.wgs.longitude, " fix=", quality,
                       " sats=", latest_.satellites_used);
        }

        // $GxRMC - Recommended Minimum Navigation Information
        void parse_rmc(const dp::String &sentence) {
            auto fields = split_fields(sentence);
            if (fields.size() < 12)
                return;

            // Field 2: Status (A=valid, V=void)
            if (fields[2] != "A")
                return;

            latest_.wgs = concord::earth::WGS(parse_lat(fields[3], fields[4]), parse_lon(fields[5], fields[6]),
                                              latest_.wgs.altitude);

            // Speed over ground (knots -> m/s)
            if (!fields[7].empty()) {
                f64 speed_knots = std::atof(fields[7].c_str());
                latest_.speed_mps = speed_knots * 0.514444;
                on_sog.emit(*latest_.speed_mps);
            }

            // Course over ground (degrees -> radians)
            if (!fields[8].empty()) {
                f64 cog_deg = std::atof(fields[8].c_str());
                latest_.cog_rad = cog_deg * M_PI / 180.0;
                on_cog.emit(*latest_.cog_rad);
            }

            if (latest_.fix_type == GNSSFixType::NoFix)
                latest_.fix_type = GNSSFixType::GNSSFix;

            on_position.emit(latest_);
            echo::category("isobus.nmea.serial").trace("RMC: ", latest_.wgs.latitude, ", ", latest_.wgs.longitude);
        }

        // $GxVTG - Track Made Good and Ground Speed
        void parse_vtg(const dp::String &sentence) {
            auto fields = split_fields(sentence);
            if (fields.size() < 9)
                return;

            // True track (degrees -> radians)
            if (!fields[1].empty()) {
                f64 track_deg = std::atof(fields[1].c_str());
                latest_.cog_rad = track_deg * M_PI / 180.0;
                on_cog.emit(*latest_.cog_rad);
            }

            // Speed in km/h -> m/s
            if (!fields[7].empty()) {
                f64 speed_kmh = std::atof(fields[7].c_str());
                latest_.speed_mps = speed_kmh / 3.6;
                on_sog.emit(*latest_.speed_mps);
            }
        }

        // $GxGSA - GNSS DOP and Active Satellites
        void parse_gsa(const dp::String &sentence) {
            auto fields = split_fields(sentence);
            if (fields.size() < 18)
                return;

            // Field 2: Fix type (1=no fix, 2=2D, 3=3D)
            u8 fix = fields[2].empty() ? 1 : static_cast<u8>(std::atoi(fields[2].c_str()));
            if (fix == 1) {
                latest_.fix_type = GNSSFixType::NoFix;
            } else if (fix >= 2 && latest_.fix_type == GNSSFixType::NoFix) {
                latest_.fix_type = GNSSFixType::GNSSFix;
            }

            if (!fields[15].empty())
                latest_.pdop = std::atof(fields[15].c_str());
            if (!fields[16].empty())
                latest_.hdop = std::atof(fields[16].c_str());
            if (!fields[17].empty())
                latest_.vdop = std::atof(fields[17].c_str());
        }
    };

} // namespace agrobus::nmea
