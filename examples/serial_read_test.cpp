#include "tractor/comms/serial.hpp"
#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

static std::atomic_bool running = true;

void signal_handler(int) { running = false; }

int main(int argc, char **argv) {
    using namespace tractor::comms;

    // Get port from command line or use default
    const char *port = (argc > 1) ? argv[1] : "/tmp/ttyV0";
    int baud = (argc > 2) ? std::atoi(argv[2]) : 115200;

    std::cout << "Serial Read Test (High-Level API)\n";
    std::cout << "==================================\n";
    std::cout << "Port: " << port << "\n";
    std::cout << "Baud: " << baud << "\n";
    std::cout << "Press Ctrl+C to exit\n\n";

    // Create high-level serial object
    Serial serial(port, baud);

    // Set up callbacks
    serial.on_line([](const std::string &line) { std::cout << "RX: " << line << "\n"; });

    serial.on_connection([](bool connected) {
        if (connected) {
            std::cout << "Connected to serial port!\n";
        } else {
            std::cout << "Disconnected from serial port\n";
        }
    });

    serial.on_error([](const std::string &error) { std::cerr << "Error: " << error << "\n"; });

    // Setup signal handler for clean exit
    std::signal(SIGINT, signal_handler);

    // Start serial communication
    if (!serial.start()) {
        std::cerr << "Failed to start serial communication\n";
        return 1;
    }

    std::cout << "Listening for lines...\n\n";

    // Main loop - just wait for Ctrl+C
    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "\n\nShutting down...\n";

    auto stats = serial.get_statistics();
    std::cout << "Statistics:\n";
    std::cout << "  Lines received: " << stats.lines_received << "\n";
    std::cout << "  Bytes received: " << stats.bytes_received << "\n";
    std::cout << "  Bytes sent:     " << stats.bytes_sent << "\n";
    std::cout << "  Errors:         " << stats.errors << "\n";

    serial.stop();
    std::cout << "Serial port closed.\n";

    return 0;
}
