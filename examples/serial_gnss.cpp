#include <agrobus/nmea/serial_gnss.hpp>
#include <echo/echo.hpp>
#include <wirebit/serial/serial_endpoint.hpp>
#include <wirebit/shm/shm_link.hpp>
#include <csignal>
#include <atomic>
#include <cstring>

using namespace agrobus::nmea;

static std::atomic<bool> running{true};
void signal_handler(int) { running = false; }

// Demonstrates reading GNSS data from a serial port using wirebit.
// Uses a ShmLink to simulate a GPS receiver sending NMEA sentences.
int main() {
    echo::info("=== Serial GNSS Demo (wirebit) ===");

    // Create a shared memory link to simulate a serial GPS receiver
    auto link_result = wirebit::ShmLink::create("gnss_serial", 4096);
    if (!link_result.is_ok()) {
        echo::error("Failed to create ShmLink for serial simulation");
        return 1;
    }
    auto link = std::make_shared<wirebit::ShmLink>(std::move(link_result.value()));

    // Create the SerialEndpoint (receiver side)
    wirebit::SerialConfig serial_config{.baud = 115200, .data_bits = 8, .stop_bits = 1, .parity = 'N'};
    wirebit::SerialEndpoint serial(link, serial_config, 1);

    // Create the GNSS parser
    SerialGNSS gnss(serial);

    gnss.on_position.subscribe([](const GNSSPosition& pos) {
        echo::info("Position: lat=", pos.wgs.latitude, " lon=", pos.wgs.longitude,
                   " alt=", pos.wgs.altitude, " sats=", pos.satellites_used,
                   " fix=", static_cast<int>(pos.fix_type));
    });

    gnss.on_cog.subscribe([](f64 cog) {
        echo::info("COG: ", cog * 180.0 / M_PI, " deg");
    });

    gnss.on_sog.subscribe([](f64 sog) {
        echo::info("SOG: ", sog * 3.6, " km/h");
    });

    // Simulate GPS receiver: inject NMEA sentences into the link
    // (In production, the link would be a PtyLink connected to real hardware)
    auto tx_link_result = wirebit::ShmLink::attach("gnss_serial");
    if (!tx_link_result.is_ok()) {
        echo::error("Failed to attach TX ShmLink");
        return 1;
    }
    auto tx_link = std::make_shared<wirebit::ShmLink>(std::move(tx_link_result.value()));
    wirebit::SerialEndpoint gps_tx(tx_link, serial_config, 2);

    // Sample NMEA sentences from a GPS receiver
    const char* nmea_sentences[] = {
        "$GPGGA,123519.00,4807.038,N,01131.000,E,4,08,0.9,545.4,M,47.0,M,,*42\r\n",
        "$GPRMC,123519.00,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A\r\n",
        "$GPVTG,084.4,T,088.5,M,022.4,N,041.5,K*65\r\n",
        "$GPGSA,A,3,04,05,,09,12,,,24,,,,,2.5,1.3,2.1*39\r\n",
        "$GPGGA,123520.00,4807.040,N,01131.002,E,4,09,0.8,545.5,M,47.0,M,,*4E\r\n",
        "$GPRMC,123520.00,A,4807.040,N,01131.002,E,023.1,085.0,230394,003.1,W*67\r\n",
    };

    echo::info("Injecting simulated NMEA sentences...\n");

    signal(SIGINT, signal_handler);

    for (const char* sentence : nmea_sentences) {
        if (!running) break;

        // Send the sentence as serial data
        wirebit::Bytes data(std::strlen(sentence));
        std::memcpy(data.data(), sentence, data.size());
        gps_tx.send(data);

        echo::debug("TX: ", sentence);

        // Process on receiver side
        gnss.update();

        usleep(200000); // 200ms between sentences (5Hz GPS)
    }

    // Show final position
    auto pos = gnss.latest_position();
    if (pos) {
        echo::info("\n=== Final Position ===");
        echo::info("WGS84: ", pos->wgs.latitude, ", ", pos->wgs.longitude, ", ", pos->wgs.altitude);
        echo::info("Fix: ", static_cast<int>(pos->fix_type), " Sats: ", pos->satellites_used);
        if (pos->hdop)
            echo::info("HDOP: ", *pos->hdop);
        if (pos->speed_mps)
            echo::info("Speed: ", *pos->speed_mps * 3.6, " km/h");

        // Convert to local frame
        dp::Geo reference{48.0, 11.0, 500.0};
        auto enu = pos->to_enu(reference);
        echo::info("ENU from ref: E=", enu.east(), " N=", enu.north(), " U=", enu.up());
    }

    echo::info("\nDone.");
    return 0;
}
