#include "tractor/comms/serial.hpp"
#include <atomic>
#include <csignal>
#include <iostream>

static std::atomic_bool running = true;

void signal_handler(int) { running = false; }

int main(int argc, char **argv) {
    using namespace tractor::comms;

    // Get port from command line or use default
    const char *port = (argc > 1) ? argv[1] : "/tmp/ttyV0";
    int baud = (argc > 2) ? std::atoi(argv[2]) : 115200;

    std::cout << "Serial Read Test\n";
    std::cout << "================\n";
    std::cout << "Port: " << port << "\n";
    std::cout << "Baud: " << baud << "\n";
    std::cout << "Press Ctrl+C to exit\n\n";

    // Configure serial port
    SerialConfig config;
    config.baud_rate = baud;
    config.data_bits = DataBits::Eight;
    config.parity = Parity::None;
    config.stop_bits = StopBits::One;
    config.flow_control = FlowControl::None;
    config.read_timeout_ms = 100; // 100ms timeout for responsive exit

    Serial serial(port, config);

    // Open serial port
    if (!serial.open()) {
        std::cerr << "Failed to open serial port: " << port << "\n";
        std::cerr << "Error: " << serial.get_last_error() << "\n";
        return 1;
    }

    std::cout << "Serial port opened successfully!\n";
    std::cout << "Listening for data...\n\n";

    // Setup signal handler for clean exit
    std::signal(SIGINT, signal_handler);

    std::string line;
    uint8_t ch;
    size_t bytes_received = 0;

    while (running) {
        ssize_t bytes_read = serial.read(&ch, 1);

        if (bytes_read > 0) {
            bytes_received++;

            // Echo the character
            if (ch >= 32 && ch <= 126) {
                // Printable ASCII
                std::cout << static_cast<char>(ch) << std::flush;
                line += static_cast<char>(ch);
            } else if (ch == '\n') {
                // Newline - print the complete line
                std::cout << "\n";
                if (!line.empty()) {
                    std::cout << "[Line complete: " << line.length() << " chars]\n";
                    line.clear();
                }
            } else if (ch == '\r') {
                // Carriage return - just ignore or handle
                std::cout << std::flush;
            } else {
                // Non-printable character - show as hex
                std::cout << "[0x" << std::hex << static_cast<int>(ch) << std::dec << "]" << std::flush;
            }
        } else if (bytes_read < 0) {
            // Error
            std::cerr << "\nRead error: " << serial.get_last_error() << "\n";
            break;
        }
        // bytes_read == 0 means timeout, just continue
    }

    std::cout << "\n\nShutting down...\n";
    std::cout << "Total bytes received: " << bytes_received << "\n";

    serial.close();
    std::cout << "Serial port closed.\n";

    return 0;
}
