#include "isobus/hardware_integration/can_hardware_interface.hpp"
#include "isobus/hardware_integration/socket_can_interface.hpp"
#include "isobus/isobus/can_network_manager.hpp"
#include "isobus/isobus/can_partnered_control_function.hpp"
#include "isobus/isobus/isobus_device_descriptor_object_pool.hpp"
#include "isobus/isobus/isobus_standard_data_description_indices.hpp"
#include "isobus/isobus/isobus_task_controller_client.hpp"
#include "tractor/comms/serial.hpp"

#include <atomic>
#include <csignal>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

static std::atomic_bool running = true;
static std::atomic<std::int32_t> gnss_auth_status = 0;
static std::atomic<std::int32_t> gnss_warning = 0;

void signal_handler(int) { running = false; }

enum class DDOPObjectIDs : std::uint16_t {
    Device = 0,
    MainDeviceElement = 1,
    DeviceActualWorkState = 2,
    Connector = 3,
    ConnectorXOffset = 4,
    ConnectorYOffset = 5,
    Implement = 6,
    GNSSAuthStatus = 7,
    GNSSWarning = 8,
    AreaPresentation = 100,
    WidthPresentation = 101
};

struct PHTGData {
    std::string date;    // Field 0: Date (DD:MM:YYYY)
    std::string time;    // Field 1: Time (HH:MM:SS.SS)
    std::string system;  // Field 2: GNSS System
    std::string service; // Field 3: Service
    int auth_result;     // Field 4: Authentication result
    int warning;         // Field 5: Warning status
};

bool validate_checksum(const std::string &sentence) {
    size_t star_pos = sentence.find('*');
    if (star_pos == std::string::npos || star_pos + 2 >= sentence.length()) {
        return false;
    }

    std::uint8_t calc_cs = 0;
    for (size_t i = 1; i < star_pos; i++) {
        calc_cs ^= static_cast<std::uint8_t>(sentence[i]);
    }

    std::string cs_str = sentence.substr(star_pos + 1, 2);
    int recv_cs = std::stoi(cs_str, nullptr, 16);

    return calc_cs == recv_cs;
}

bool parse_phtg(const std::string &sentence, PHTGData &data) {
    if (sentence.substr(0, 5) != "$PHTG") {
        return false;
    }

    if (!validate_checksum(sentence)) {
        return false;
    }

    size_t star_pos = sentence.find('*');
    std::string body = sentence.substr(6, star_pos - 6);

    std::stringstream ss(body);
    std::string token;
    int field = 0;

    while (std::getline(ss, token, ',')) {
        switch (field) {
        case 0:
            data.date = token;
            break;
        case 1:
            data.time = token;
            break;
        case 2:
            data.system = token;
            break;
        case 3:
            data.service = token;
            break;
        case 4:
            data.auth_result = std::stoi(token);
            break;
        case 5:
            data.warning = std::stoi(token);
            break;
        }
        field++;
    }

    return field >= 6;
}

void process_nmea_line(const std::string &line) {
    // Only process PHTG messages
    if (line.substr(0, 5) == "$PHTG") {
        PHTGData phtg;
        if (parse_phtg(line, phtg)) {
            std::cout << "PHTG: [" << phtg.date << " " << phtg.time << "] " << phtg.system << "/" << phtg.service
                      << " Auth=" << phtg.auth_result << " Warn=" << phtg.warning << "\n";

            gnss_auth_status.store(phtg.auth_result);
            gnss_warning.store(phtg.warning);
        }
    }
}

bool create_simple_ddop(std::shared_ptr<isobus::DeviceDescriptorObjectPool> ddop, isobus::NAME clientName) {
    bool success = true;
    ddop->clear();

    std::array<std::uint8_t, 7> localizationData = {'e', 'n', 0x50, 0x00, 0x55, 0x55, 0xFF};

    success &= ddop->add_device("GNSSAuthDevice", "1.0.0", "001", "GAD1.0", localizationData,
                                std::vector<std::uint8_t>(), clientName.get_full_name());

    success &=
        ddop->add_device_element("Device", 0, 0, isobus::task_controller_object::DeviceElementObject::Type::Device,
                                 static_cast<std::uint16_t>(DDOPObjectIDs::MainDeviceElement));

    success &= ddop->add_device_element("Connector", 1, static_cast<std::uint16_t>(DDOPObjectIDs::MainDeviceElement),
                                        isobus::task_controller_object::DeviceElementObject::Type::Connector,
                                        static_cast<std::uint16_t>(DDOPObjectIDs::Connector));

    success &= ddop->add_device_process_data(
        "Connector X", static_cast<std::uint16_t>(isobus::DataDescriptionIndex::DeviceElementOffsetX),
        static_cast<std::uint16_t>(DDOPObjectIDs::WidthPresentation),
        static_cast<std::uint8_t>(isobus::task_controller_object::DeviceProcessDataObject::PropertiesBit::Settable), 0,
        static_cast<std::uint16_t>(DDOPObjectIDs::ConnectorXOffset));

    success &= ddop->add_device_process_data(
        "Connector Y", static_cast<std::uint16_t>(isobus::DataDescriptionIndex::DeviceElementOffsetY),
        static_cast<std::uint16_t>(DDOPObjectIDs::WidthPresentation),
        static_cast<std::uint8_t>(isobus::task_controller_object::DeviceProcessDataObject::PropertiesBit::Settable), 0,
        static_cast<std::uint16_t>(DDOPObjectIDs::ConnectorYOffset));

    success &= ddop->add_device_element("Function", 2, static_cast<std::uint16_t>(DDOPObjectIDs::MainDeviceElement),
                                        isobus::task_controller_object::DeviceElementObject::Type::Function,
                                        static_cast<std::uint16_t>(DDOPObjectIDs::Implement));

    success &= ddop->add_device_process_data(
        "GNSS Auth Status", static_cast<std::uint16_t>(isobus::DataDescriptionIndex::ActualWorkState),
        isobus::NULL_OBJECT_ID,
        static_cast<std::uint8_t>(
            isobus::task_controller_object::DeviceProcessDataObject::PropertiesBit::MemberOfDefaultSet),
        static_cast<std::uint8_t>(
            isobus::task_controller_object::DeviceProcessDataObject::AvailableTriggerMethods::OnChange),
        static_cast<std::uint16_t>(DDOPObjectIDs::GNSSAuthStatus));

    success &= ddop->add_device_process_data(
        "GNSS Warning", static_cast<std::uint16_t>(isobus::DataDescriptionIndex::SetpointWorkState),
        isobus::NULL_OBJECT_ID,
        static_cast<std::uint8_t>(
            isobus::task_controller_object::DeviceProcessDataObject::PropertiesBit::MemberOfDefaultSet),
        static_cast<std::uint8_t>(
            isobus::task_controller_object::DeviceProcessDataObject::AvailableTriggerMethods::OnChange),
        static_cast<std::uint16_t>(DDOPObjectIDs::GNSSWarning));

    success &= ddop->add_device_value_presentation("mm", 0, 1.0f, 0,
                                                   static_cast<std::uint16_t>(DDOPObjectIDs::WidthPresentation));

    success &= ddop->add_device_value_presentation("m^2", 0, 1.0f, 0,
                                                   static_cast<std::uint16_t>(DDOPObjectIDs::AreaPresentation));

    if (success) {
        auto mainElement = std::static_pointer_cast<isobus::task_controller_object::DeviceElementObject>(
            ddop->get_object_by_id(static_cast<std::uint16_t>(DDOPObjectIDs::MainDeviceElement)));
        auto connector = std::static_pointer_cast<isobus::task_controller_object::DeviceElementObject>(
            ddop->get_object_by_id(static_cast<std::uint16_t>(DDOPObjectIDs::Connector)));
        auto implement = std::static_pointer_cast<isobus::task_controller_object::DeviceElementObject>(
            ddop->get_object_by_id(static_cast<std::uint16_t>(DDOPObjectIDs::Implement)));

        connector->add_reference_to_child_object(static_cast<std::uint16_t>(DDOPObjectIDs::ConnectorXOffset));
        connector->add_reference_to_child_object(static_cast<std::uint16_t>(DDOPObjectIDs::ConnectorYOffset));

        implement->add_reference_to_child_object(static_cast<std::uint16_t>(DDOPObjectIDs::GNSSAuthStatus));
        implement->add_reference_to_child_object(static_cast<std::uint16_t>(DDOPObjectIDs::GNSSWarning));
    }

    return success;
}

int main(int argc, char **argv) {
    const char *serial_device = "/dev/ttyUSB1";
    int serial_baud = 9600;

    if (argc > 1) {
        serial_device = argv[1];
    }
    if (argc > 2) {
        serial_baud = std::atoi(argv[2]);
    }

    std::cout << "NMEA to ISOBUS Bridge\n";
    std::cout << "Serial: " << serial_device << " @ " << serial_baud << " baud\n\n";

    auto can_driver = std::make_shared<isobus::SocketCANInterface>("can0");
    isobus::CANHardwareInterface::set_number_of_can_channels(1);
    isobus::CANHardwareInterface::assign_can_channel_frame_handler(0, can_driver);

    if ((!isobus::CANHardwareInterface::start()) || (!can_driver->get_is_valid())) {
        std::cerr << "Failed to start CAN interface\n";
        return -1;
    }

    std::signal(SIGINT, signal_handler);

    isobus::NAME my_name(0);
    my_name.set_arbitrary_address_capable(true);
    my_name.set_industry_group(2);
    my_name.set_device_class(6);
    my_name.set_function_code(static_cast<std::uint8_t>(isobus::NAME::Function::RateControl));
    my_name.set_identity_number(43);
    my_name.set_ecu_instance(0);
    my_name.set_function_instance(0);
    my_name.set_device_class_instance(0);
    my_name.set_manufacturer_code(1407);

    auto my_ecu = isobus::CANNetworkManager::CANNetwork.create_internal_control_function(my_name, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(800));

    const isobus::NAMEFilter filterTaskController(isobus::NAME::NAMEParameters::FunctionCode,
                                                  static_cast<std::uint8_t>(isobus::NAME::Function::TaskController));
    const isobus::NAMEFilter filterTaskControllerInstance(isobus::NAME::NAMEParameters::FunctionInstance, 0);
    const isobus::NAMEFilter filterTaskControllerIndustryGroup(
        isobus::NAME::NAMEParameters::IndustryGroup,
        static_cast<std::uint8_t>(isobus::NAME::IndustryGroup::AgriculturalAndForestryEquipment));
    const isobus::NAMEFilter filterTaskControllerDeviceClass(
        isobus::NAME::NAMEParameters::DeviceClass, static_cast<std::uint8_t>(isobus::NAME::DeviceClass::NonSpecific));

    const std::vector<isobus::NAMEFilter> tcNameFilters = {filterTaskController, filterTaskControllerInstance,
                                                           filterTaskControllerIndustryGroup,
                                                           filterTaskControllerDeviceClass};

    auto tc_partner = isobus::CANNetworkManager::CANNetwork.create_partnered_control_function(0, tcNameFilters);

    auto tc = std::make_shared<isobus::TaskControllerClient>(tc_partner, my_ecu, nullptr);

    auto ddop = std::make_shared<isobus::DeviceDescriptorObjectPool>();
    if (!create_simple_ddop(ddop, my_ecu->get_NAME())) {
        std::cerr << "Failed to create DDOP\n";
        return -1;
    }

    tc->add_request_value_callback(
        [](std::uint16_t elementNumber, std::uint16_t ddi, std::int32_t &outValue, void *parent) -> bool {
            switch (ddi) {
            case static_cast<std::uint16_t>(isobus::DataDescriptionIndex::ActualWorkState):
                outValue = gnss_auth_status.load();
                std::cout << "TC requests Auth Status: " << outValue << "\n";
                return true;

            case static_cast<std::uint16_t>(isobus::DataDescriptionIndex::SetpointWorkState):
                outValue = gnss_warning.load();
                std::cout << "TC requests Warning: " << outValue << "\n";
                return true;

            default:
                outValue = 0;
                return false;
            }
        },
        nullptr);

    tc->add_value_command_callback([](std::uint16_t elementNumber, std::uint16_t ddi, std::int32_t processVariableValue,
                                      void *parent) -> bool { return false; },
                                   nullptr);

    tc->configure(ddop, 1, 0, 0, true, false, false, false, false);
    tc->initialize(true);

    // Set up high-level serial communication
    auto nmea_serial = std::make_shared<tractor::comms::Serial>(serial_device, serial_baud);

    nmea_serial->on_line([](const std::string &line) { process_nmea_line(line); });

    nmea_serial->on_connection([](bool connected) {
        if (connected) {
            std::cout << "Serial port connected\n";
        } else {
            std::cout << "Serial port disconnected\n";
        }
    });

    nmea_serial->on_error([](const std::string &error) { std::cerr << "Serial error: " << error << "\n"; });

    if (!nmea_serial->start()) {
        std::cerr << "Failed to start serial communication\n";
        return -1;
    }

    std::cout << "Running... Press Ctrl+C to exit\n\n";

    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    std::cout << "\nShutting down...\n";

    nmea_serial->stop();
    tc->terminate();
    isobus::CANHardwareInterface::stop();

    return 0;
}
