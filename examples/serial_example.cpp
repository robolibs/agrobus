#include "tractor/comms/tty.hpp"
#include <chrono>
#include <iostream>
#include <thread>

using namespace tractor::comms;

void example_basic_usage() {
    std::cout << "=== Basic Serial Usage Example ===\n" << std::endl;

    // Create serial port with default settings
    Tty serial("/dev/ttyUSB0", 115200);

    // Open the port
    if (!serial.open()) {
        std::cerr << "Failed to open port: " << serial.get_last_error() << std::endl;
        return;
    }

    std::cout << "Port opened successfully at " << serial.get_baud_rate() << " baud" << std::endl;

    // Write some data
    std::string message = "Hello, Serial!\n";
    ssize_t written = serial.write(message);
    std::cout << "Written " << written << " bytes" << std::endl;

    // Read response
    std::vector<uint8_t> buffer;
    ssize_t bytes_read = serial.read(buffer, 256);
    if (bytes_read > 0) {
        std::cout << "Read " << bytes_read << " bytes: ";
        for (auto byte : buffer) {
            std::cout << static_cast<char>(byte);
        }
        std::cout << std::endl;
    }

    serial.close();
}

void example_advanced_config() {
    std::cout << "\n=== Advanced Configuration Example ===\n" << std::endl;

    // Create configuration
    SerialConfig config;
    config.baud_rate = 9600;
    config.data_bits = DataBits::Eight;
    config.parity = Parity::None;
    config.stop_bits = StopBits::One;
    config.flow_control = FlowControl::None;
    config.read_timeout_ms = 2000;
    config.write_timeout_ms = 2000;

    Tty serial("/dev/ttyACM0", config);

    if (!serial.open()) {
        std::cerr << "Failed to open port: " << serial.get_last_error() << std::endl;
        return;
    }

    std::cout << "Port opened with custom configuration" << std::endl;

    // Check available bytes before reading
    ssize_t available = serial.available();
    std::cout << "Bytes available: " << available << std::endl;

    serial.close();
}

void example_read_line() {
    std::cout << "\n=== Read Line Example ===\n" << std::endl;

    Tty serial("/dev/ttyUSB0", 115200);

    if (!serial.open()) {
        std::cerr << "Failed to open port: " << serial.get_last_error() << std::endl;
        return;
    }

    std::cout << "Waiting for line-based input..." << std::endl;

    // Read lines continuously
    for (int i = 0; i < 5; i++) {
        std::string line = serial.read_line();
        if (!line.empty()) {
            std::cout << "Received line: " << line << std::endl;
        } else {
            std::cout << "Timeout or error reading line" << std::endl;
        }
    }

    serial.close();
}

void example_read_exact() {
    std::cout << "\n=== Read Exact Bytes Example ===\n" << std::endl;

    Tty serial("/dev/ttyUSB0", 115200);

    if (!serial.open()) {
        std::cerr << "Failed to open port: " << serial.get_last_error() << std::endl;
        return;
    }

    // Read exactly 16 bytes with 5 second timeout
    std::cout << "Reading exactly 16 bytes..." << std::endl;
    auto data = serial.read_exact(16, 5000);

    if (data.size() == 16) {
        std::cout << "Successfully read 16 bytes: ";
        for (auto byte : data) {
            std::printf("%02X ", byte);
        }
        std::cout << std::endl;
    } else {
        std::cout << "Only read " << data.size() << " bytes (timeout or error)" << std::endl;
    }

    serial.close();
}

void example_control_lines() {
    std::cout << "\n=== Control Lines Example ===\n" << std::endl;

    Tty serial("/dev/ttyUSB0", 115200);

    if (!serial.open()) {
        std::cerr << "Failed to open port: " << serial.get_last_error() << std::endl;
        return;
    }

    // Set DTR and RTS
    std::cout << "Setting DTR high..." << std::endl;
    serial.set_dtr(true);

    std::cout << "Setting RTS high..." << std::endl;
    serial.set_rts(true);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Check input control lines
    std::cout << "CTS state: " << (serial.get_cts() ? "HIGH" : "LOW") << std::endl;
    std::cout << "DSR state: " << (serial.get_dsr() ? "HIGH" : "LOW") << std::endl;
    std::cout << "CD state: " << (serial.get_cd() ? "HIGH" : "LOW") << std::endl;
    std::cout << "RI state: " << (serial.get_ri() ? "HIGH" : "LOW") << std::endl;

    serial.close();
}

void example_list_ports() {
    std::cout << "\n=== List Available Ports ===\n" << std::endl;

    auto ports = Tty::list_ports();

    if (ports.empty()) {
        std::cout << "No serial ports found" << std::endl;
    } else {
        std::cout << "Available serial ports:" << std::endl;
        for (const auto &port : ports) {
            std::cout << "  - " << port;
            if (Tty::port_exists(port)) {
                std::cout << " (exists)";
            }
            std::cout << std::endl;
        }
    }
}

void example_binary_data() {
    std::cout << "\n=== Binary Data Transfer Example ===\n" << std::endl;

    Tty serial("/dev/ttyUSB0", 115200);

    if (!serial.open()) {
        std::cerr << "Failed to open port: " << serial.get_last_error() << std::endl;
        return;
    }

    // Prepare binary data
    std::vector<uint8_t> binary_data = {0x01, 0x02, 0x03, 0x04, 0x05, 0xAA, 0xBB, 0xCC};

    std::cout << "Sending binary data: ";
    for (auto byte : binary_data) {
        std::printf("%02X ", byte);
    }
    std::cout << std::endl;

    ssize_t written = serial.write(binary_data);
    std::cout << "Written " << written << " bytes" << std::endl;

    // Flush output to ensure data is sent
    serial.flush_output();

    serial.close();
}

int main(int argc, char *argv[]) {
    std::cout << "Serial Communication Library Examples\n" << std::endl;
    std::cout << "Note: These examples require actual serial ports to be connected." << std::endl;
    std::cout << "Modify the port paths in the code to match your system.\n" << std::endl;

    // List available ports first
    example_list_ports();

    // Uncomment the examples you want to run:

    // example_basic_usage();
    // example_advanced_config();
    // example_read_line();
    // example_read_exact();
    // example_control_lines();
    // example_binary_data();

    return 0;
}
