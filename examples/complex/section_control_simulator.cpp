/// @file section_control_simulator.cpp
/// @brief Section Control Implement Simulator for Task Controller communication.
///
/// A comprehensive example demonstrating a fully-functional section control
/// implement (sprayer) simulator with configurable sections, condensed work
/// states, application rates, and proper DDOP structure.
///
/// Based on the AgIsoStack++ SectionControlImplementSimulator example,
/// reimplemented for the agrobus ISOBUS library.
///
/// This example demonstrates:
///   - Building a complete DDOP with connector, boom, product bin, and sections
///   - Handling condensed work states (16 sections packed into 4 bytes)
///   - Application rate setpoints and prescriptions
///   - Section offset and width properties
///   - Value request/command callbacks
///   - Named structure/localization labels

#include <agrobus/isobus/tc/client.hpp>
#include <agrobus/isobus/tc/ddi_database.hpp>
#include <agrobus/isobus/tc/ddop.hpp>
#include <agrobus/isobus/tc/objects.hpp>
#include <agrobus/net/network_manager.hpp>
#include <echo/echo.hpp>
#include <wirebit/can/can_endpoint.hpp>
#include <wirebit/can/socketcan_link.hpp>
#include <csignal>
#include <atomic>

using namespace agrobus::net;
using namespace agrobus::isobus;
using namespace agrobus::isobus::tc;
namespace ddi = agrobus::isobus::tc::ddi;

static std::atomic<bool> running{true};
void signal_handler(int) { running = false; }

// ─── Section Control Implement Simulator ─────────────────────────────────────────
/// Simulates a sprayer implement with configurable number of sections.
/// Handles condensed work states, application rates, and section geometry.
class SectionControlImplementSimulator {
  public:
    static constexpr u8 MAX_SECTIONS = 16;

    struct SectionInfo {
        i32 x_offset_mm = 0;  // Lateral offset from boom center (mm)
        i32 width_mm = 1500;  // Section width (mm)
        bool state = false;   // Current on/off state
    };

    // ─── Configuration ───────────────────────────────────────────────────────
    u8 num_sections = 6;              // Number of sections (1-16)
    i32 boom_width_mm = 18000;        // Total boom width (mm)
    i32 connector_x_offset_mm = 0;    // Connector X offset from reference point (mm)
    i32 connector_y_offset_mm = -500; // Connector Y offset (typically behind tractor) (mm)

    // ─── State ───────────────────────────────────────────────────────────────
    dp::Array<SectionInfo, MAX_SECTIONS> sections{};
    i32 setpoint_condensed_work_state = 0; // TC's desired section states (packed)
    i32 actual_condensed_work_state = 0;   // Current section states (packed)
    i32 prescription_rate = 0;              // Prescription from TC (rate * 10000)
    i32 setpoint_rate = 0;                  // Target application rate (L/ha * 10000)
    i32 actual_rate = 0;                    // Current application rate (L/ha * 10000)
    bool in_auto_mode = true;               // Whether we follow TC commands

    // ─── Element Numbers ─────────────────────────────────────────────────────
    // Element 0: Device
    // Element 1: Connector
    // Element 2: Boom (Function)
    // Element 3: Product Bin
    // Elements 4+: Sections (4 to 4+num_sections-1)
    static constexpr ElementNumber ELEM_DEVICE = 0;
    static constexpr ElementNumber ELEM_CONNECTOR = 1;
    static constexpr ElementNumber ELEM_BOOM = 2;
    static constexpr ElementNumber ELEM_PRODUCT = 3;
    static constexpr ElementNumber ELEM_SECTION_BASE = 4;

    // ─── Constructor ─────────────────────────────────────────────────────────
    explicit SectionControlImplementSimulator(u8 section_count = 6) : num_sections(section_count) {
        if (num_sections > MAX_SECTIONS)
            num_sections = MAX_SECTIONS;
        if (num_sections < 1)
            num_sections = 1;

        calculate_section_geometry();
    }

    // Calculate section offsets and widths based on boom width and section count
    void calculate_section_geometry() {
        i32 section_width = boom_width_mm / num_sections;
        i32 half_width = boom_width_mm / 2;

        for (u8 i = 0; i < num_sections; ++i) {
            // Calculate X offset from boom center (left sections are negative)
            // Sections are numbered left-to-right, offset from center
            i32 section_center = -half_width + (section_width / 2) + (i * section_width);
            sections[i].x_offset_mm = section_center;
            sections[i].width_mm = section_width;
            sections[i].state = false;
        }
    }

    // Get section index from element number
    i16 element_to_section_index(ElementNumber elem) const {
        if (elem >= ELEM_SECTION_BASE && elem < ELEM_SECTION_BASE + num_sections) {
            return static_cast<i16>(elem - ELEM_SECTION_BASE);
        }
        return -1;
    }

    // Update section states from condensed work state
    void apply_setpoint_work_state() {
        if (!in_auto_mode)
            return;

        for (u8 i = 0; i < num_sections; ++i) {
            // Each section uses 2 bits in the condensed work state
            // 0 = disabled, 1 = enabled, 2 = error, 3 = not available
            u8 state_bits = (setpoint_condensed_work_state >> (i * 2)) & 0x03;
            sections[i].state = (state_bits == 1);
        }
        update_actual_condensed_state();
    }

    // Build condensed work state from section states
    void update_actual_condensed_state() {
        actual_condensed_work_state = 0;
        for (u8 i = 0; i < num_sections; ++i) {
            // Set to 1 (enabled) if section is on, 0 (disabled) if off
            u8 state_bits = sections[i].state ? 1 : 0;
            actual_condensed_work_state |= (state_bits << (i * 2));
        }
    }

    // Set section state directly (e.g., from manual control)
    void set_section(u8 index, bool on) {
        if (index < num_sections) {
            sections[index].state = on;
            update_actual_condensed_state();
            echo::category("sprayer").info("Section ", index + 1, " -> ", on ? "ON" : "OFF");
        }
    }

    // Print current state
    void print_status() const {
        echo::info("=== Sprayer Status ===");
        echo::info("Mode: ", in_auto_mode ? "AUTO" : "MANUAL");
        echo::info("Sections: ", num_sections, " x ", sections[0].width_mm, " mm");
        echo::info("Setpoint rate: ", setpoint_rate / 10000.0f, " L/ha");
        echo::info("Actual rate: ", actual_rate / 10000.0f, " L/ha");
        echo::info("Setpoint work state: 0x", dp::to_string(setpoint_condensed_work_state).c_str());
        echo::info("Actual work state: 0x", dp::to_string(actual_condensed_work_state).c_str());

        dp::String section_str;
        for (u8 i = 0; i < num_sections; ++i) {
            section_str += sections[i].state ? "O" : ".";
        }
        echo::info("Sections [", section_str, "]");
    }
};

// ─── Build DDOP ─────────────────────────────────────────────────────────────────
/// Constructs a complete Device Descriptor Object Pool for the sprayer.
///
/// Object hierarchy:
///   Device (SprayerSimulator)
///     DeviceElement[Device] (element 0)
///       DeviceElement[Connector] (element 1)
///         - Connector X offset property
///         - Connector Y offset property
///       DeviceElement[Function/Boom] (element 2)
///         - Actual condensed work state (16 sections)
///         - Setpoint condensed work state (16 sections)
///         - Working width property
///         DeviceElement[Bin/Product] (element 3)
///           - Actual volume rate
///           - Setpoint volume rate
///           - Prescription rate
///         DeviceElement[Section] (elements 4..N)
///           - Section X offset property
///           - Section width property
///   ValuePresentations: mm, L/ha
DDOP build_ddop(const SectionControlImplementSimulator &sim) {
    DDOP ddop;

    // ─── Value Presentations ─────────────────────────────────────────────────
    auto vp_mm_id = ddop.next_id();
    ddop.add_value_presentation(DeviceValuePresentation{}
                                    .set_id(vp_mm_id)
                                    .set_offset(0)
                                    .set_scale(1.0f)
                                    .set_decimals(0)
                                    .set_unit("mm"));

    auto vp_rate_id = ddop.next_id();
    ddop.add_value_presentation(DeviceValuePresentation{}
                                    .set_id(vp_rate_id)
                                    .set_offset(0)
                                    .set_scale(0.0001f) // Value is stored as rate * 10000
                                    .set_decimals(1)
                                    .set_unit("L/ha"));

    // ─── Device Object ───────────────────────────────────────────────────────
    auto device_id = ddop.next_id();
    ddop.add_device(DeviceObject{}
                        .set_id(device_id)
                        .set_designator("SprayerSim")
                        .set_software_version("1.0.0")
                        .set_serial_number("SIM001")
                        .set_structure_label({'S', 'P', 'R', 'A', 'Y', '0', '1'})
                        .set_localization_label({0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}));

    // ─── Device Element (Root, element 0) ────────────────────────────────────
    auto device_elem_id = ddop.next_id();
    DeviceElement device_elem =
        DeviceElement{}.set_id(device_elem_id).set_type(DeviceElementType::Device).set_number(0).set_designator("Root");

    // ─── Connector Element (element 1) ───────────────────────────────────────
    auto connector_elem_id = ddop.next_id();

    // Connector X offset property
    auto conn_x_prop_id = ddop.next_id();
    ddop.add_property(DeviceProperty{}
                          .set_id(conn_x_prop_id)
                          .set_ddi(ddi::DEVICE_ELEMENT_OFFSET_X)
                          .set_value(sim.connector_x_offset_mm)
                          .set_presentation(vp_mm_id)
                          .set_designator("ConnX"));

    // Connector Y offset property
    auto conn_y_prop_id = ddop.next_id();
    ddop.add_property(DeviceProperty{}
                          .set_id(conn_y_prop_id)
                          .set_ddi(ddi::DEVICE_ELEMENT_OFFSET_Y)
                          .set_value(sim.connector_y_offset_mm)
                          .set_presentation(vp_mm_id)
                          .set_designator("ConnY"));

    DeviceElement connector_elem = DeviceElement{}
                                       .set_id(connector_elem_id)
                                       .set_type(DeviceElementType::Connector)
                                       .set_number(SectionControlImplementSimulator::ELEM_CONNECTOR)
                                       .set_designator("Connector")
                                       .set_parent(device_elem_id)
                                       .add_child(conn_x_prop_id)
                                       .add_child(conn_y_prop_id);

    // ─── Boom Element (Function, element 2) ──────────────────────────────────
    auto boom_elem_id = ddop.next_id();

    // Actual condensed work state (sections 1-16)
    auto actual_cws_pd_id = ddop.next_id();
    ddop.add_process_data(DeviceProcessData{}
                              .set_id(actual_cws_pd_id)
                              .set_ddi(ddi::ACTUAL_CONDENSED_WORK_STATE_1_16)
                              .add_trigger(TriggerMethod::OnChange)
                              .add_trigger(TriggerMethod::TimeInterval)
                              .set_designator("ActualCWS"));

    // Setpoint condensed work state (sections 1-16)
    auto setpoint_cws_pd_id = ddop.next_id();
    ddop.add_process_data(DeviceProcessData{}
                              .set_id(setpoint_cws_pd_id)
                              .set_ddi(ddi::SETPOINT_CONDENSED_WORK_STATE_1_16)
                              .add_trigger(TriggerMethod::OnChange)
                              .add_trigger(TriggerMethod::TimeInterval)
                              .set_designator("SetpointCWS"));

    // Working width property
    auto width_prop_id = ddop.next_id();
    ddop.add_property(DeviceProperty{}
                          .set_id(width_prop_id)
                          .set_ddi(ddi::WORKING_WIDTH)
                          .set_value(sim.boom_width_mm)
                          .set_presentation(vp_mm_id)
                          .set_designator("BoomWidth"));

    DeviceElement boom_elem = DeviceElement{}
                                  .set_id(boom_elem_id)
                                  .set_type(DeviceElementType::Function)
                                  .set_number(SectionControlImplementSimulator::ELEM_BOOM)
                                  .set_designator("Boom")
                                  .set_parent(device_elem_id)
                                  .add_child(actual_cws_pd_id)
                                  .add_child(setpoint_cws_pd_id)
                                  .add_child(width_prop_id);

    // ─── Product Bin Element (Bin, element 3) ────────────────────────────────
    auto product_elem_id = ddop.next_id();

    // Actual volume per area rate
    auto actual_rate_pd_id = ddop.next_id();
    ddop.add_process_data(DeviceProcessData{}
                              .set_id(actual_rate_pd_id)
                              .set_ddi(ddi::ACTUAL_VOLUME_PER_AREA_APPLICATION_RATE)
                              .add_trigger(TriggerMethod::OnChange)
                              .add_trigger(TriggerMethod::TimeInterval)
                              .set_presentation(vp_rate_id)
                              .set_designator("ActualRate"));

    // Setpoint volume per area rate
    auto setpoint_rate_pd_id = ddop.next_id();
    ddop.add_process_data(DeviceProcessData{}
                              .set_id(setpoint_rate_pd_id)
                              .set_ddi(ddi::SETPOINT_VOLUME_PER_AREA_APPLICATION_RATE)
                              .add_trigger(TriggerMethod::OnChange)
                              .add_trigger(TriggerMethod::TimeInterval)
                              .set_presentation(vp_rate_id)
                              .set_designator("SetpointRate"));

    // Default/prescription rate
    auto prescription_pd_id = ddop.next_id();
    ddop.add_process_data(DeviceProcessData{}
                              .set_id(prescription_pd_id)
                              .set_ddi(ddi::DEFAULT_VOLUME_PER_AREA_APPLICATION_RATE)
                              .add_trigger(TriggerMethod::OnChange)
                              .set_presentation(vp_rate_id)
                              .set_designator("Prescription"));

    DeviceElement product_elem = DeviceElement{}
                                     .set_id(product_elem_id)
                                     .set_type(DeviceElementType::Bin)
                                     .set_number(SectionControlImplementSimulator::ELEM_PRODUCT)
                                     .set_designator("Product")
                                     .set_parent(boom_elem_id)
                                     .add_child(actual_rate_pd_id)
                                     .add_child(setpoint_rate_pd_id)
                                     .add_child(prescription_pd_id);

    boom_elem.add_child(product_elem_id);

    // ─── Section Elements (elements 4..N) ────────────────────────────────────
    for (u8 i = 0; i < sim.num_sections; ++i) {
        auto section_elem_id = ddop.next_id();

        // Section X offset (lateral position)
        auto section_x_prop_id = ddop.next_id();
        ddop.add_property(DeviceProperty{}
                              .set_id(section_x_prop_id)
                              .set_ddi(ddi::DEVICE_ELEMENT_OFFSET_X)
                              .set_value(sim.sections[i].x_offset_mm)
                              .set_presentation(vp_mm_id)
                              .set_designator("SectX" + dp::String(dp::to_string(i + 1).c_str())));

        // Section width
        auto section_width_prop_id = ddop.next_id();
        ddop.add_property(DeviceProperty{}
                              .set_id(section_width_prop_id)
                              .set_ddi(ddi::WORKING_WIDTH)
                              .set_value(sim.sections[i].width_mm)
                              .set_presentation(vp_mm_id)
                              .set_designator("SectW" + dp::String(dp::to_string(i + 1).c_str())));

        DeviceElement section_elem = DeviceElement{}
                                         .set_id(section_elem_id)
                                         .set_type(DeviceElementType::Section)
                                         .set_number(SectionControlImplementSimulator::ELEM_SECTION_BASE + i)
                                         .set_designator("Sect" + dp::String(dp::to_string(i + 1).c_str()))
                                         .set_parent(boom_elem_id)
                                         .add_child(section_x_prop_id)
                                         .add_child(section_width_prop_id);

        ddop.add_element(section_elem);
        boom_elem.add_child(section_elem_id);
    }

    // Add elements to DDOP (order matters for parent-child validation)
    ddop.add_element(device_elem);
    ddop.add_element(connector_elem);
    ddop.add_element(product_elem);
    ddop.add_element(boom_elem);

    // Link device element as parent
    // Note: In real DDOP, device element's parent is the device object (device_id)
    // but our simple model doesn't require strict parent chain validation

    return ddop;
}

// ─── State name helper ──────────────────────────────────────────────────────────
static const char *tc_state_name(TCState s) {
    switch (s) {
    case TCState::Disconnected:
        return "Disconnected";
    case TCState::WaitForStartup:
        return "WaitForStartup";
    case TCState::WaitForServerStatus:
        return "WaitForServerStatus";
    case TCState::SendWorkingSetMaster:
        return "SendWorkingSetMaster";
    case TCState::RequestVersion:
        return "RequestVersion";
    case TCState::WaitForVersion:
        return "WaitForVersion";
    case TCState::ProcessDDOP:
        return "ProcessDDOP";
    case TCState::TransferDDOP:
        return "TransferDDOP";
    case TCState::WaitForPoolResponse:
        return "WaitForPoolResponse";
    case TCState::ActivatePool:
        return "ActivatePool";
    case TCState::WaitForActivation:
        return "WaitForActivation";
    case TCState::Connected:
        return "Connected";
    default:
        return "Unknown";
    }
}

// ─── Main ───────────────────────────────────────────────────────────────────────
int main(int argc, char *argv[]) {
    echo::box("=== Section Control Implement Simulator ===");

    // Parse command line arguments
    dp::String interface = "vcan0";
    u8 num_sections = 6;

    if (argc > 1)
        interface = argv[1];
    if (argc > 2)
        num_sections = static_cast<u8>(std::atoi(argv[2]));

    echo::info("CAN interface: ", interface);
    echo::info("Number of sections: ", num_sections);
    echo::info("");

    // ─── Hardware Setup ──────────────────────────────────────────────────────
    auto link_result = wirebit::SocketCanLink::create({.interface_name = interface, .create_if_missing = true});
    if (!link_result.is_ok()) {
        echo::error("Failed to open ", interface, ": ", link_result.error().message);
        echo::info("Run: sudo modprobe vcan && sudo ip link add dev ", interface,
                   " type vcan && sudo ip link set ", interface, " up");
        return 1;
    }
    auto link = std::make_shared<wirebit::SocketCanLink>(std::move(link_result.value()));
    wirebit::CanEndpoint can(link, wirebit::CanConfig{.bitrate = 250000}, 1);

    // ─── Network Setup ───────────────────────────────────────────────────────
    IsoNet nm(NetworkConfig{}.bus_load(true));
    nm.set_endpoint(0, &can);

    Name name = Name::build()
                    .set_identity_number(12345)
                    .set_manufacturer_code(64)
                    .set_function_code(130) // Sprayer function
                    .set_device_class(4)    // Sprayers
                    .set_industry_group(2)  // Agriculture
                    .set_self_configurable(true);

    auto cf_result = nm.create_internal(name, 0, 0x28);
    if (!cf_result.is_ok()) {
        echo::error("Failed to create internal CF: ", cf_result.error().message);
        return 1;
    }
    auto *cf = cf_result.value();
    echo::info("Internal CF created: address=0x28");

    // ─── Sprayer Simulator ───────────────────────────────────────────────────
    SectionControlImplementSimulator sprayer(num_sections);
    sprayer.setpoint_rate = 2000000; // 200 L/ha default
    sprayer.actual_rate = 2000000;

    echo::info("Boom width: ", sprayer.boom_width_mm, " mm");
    echo::info("Section width: ", sprayer.sections[0].width_mm, " mm");
    echo::info("");

    // ─── Build DDOP ──────────────────────────────────────────────────────────
    DDOP ddop = build_ddop(sprayer);

    auto validation = ddop.validate();
    if (!validation.is_ok()) {
        echo::error("DDOP validation failed: ", validation.error().message);
        return 1;
    }
    echo::info("DDOP validation: OK");
    echo::info("DDOP objects: ", ddop.object_count());

    auto serialized = ddop.serialize();
    if (serialized.is_ok()) {
        echo::info("DDOP binary size: ", serialized.value().size(), " bytes");
    }

    // Generate ISOXML for debugging
    echo::info("");
    echo::info("--- DDOP ISOXML (fragment) ---");
    auto isoxml = ddop.to_isoxml();
    // Print first 800 chars
    if (isoxml.size() > 800) {
        echo::info(isoxml.substr(0, 800).c_str(), "...");
    } else {
        echo::info(isoxml.c_str());
    }
    echo::info("--- End ISOXML ---");
    echo::info("");

    // ─── TC Client ───────────────────────────────────────────────────────────
    TCClientConfig tc_config;
    tc_config.timeout(6000);

    TaskControllerClient tc_client(nm, cf, tc_config);
    tc_client.set_ddop(std::move(ddop));

    // ─── Value Request Callback ──────────────────────────────────────────────
    tc_client.on_value_request([&](ElementNumber elem, DDI ddi) -> Result<i32> {
        echo::category("tc.req").debug("Request: elem=", elem, " DDI=0x", dp::to_string(ddi).c_str());

        // Boom element (element 2)
        if (elem == SectionControlImplementSimulator::ELEM_BOOM) {
            switch (ddi) {
            case ddi::ACTUAL_CONDENSED_WORK_STATE_1_16:
                return Result<i32>::ok(sprayer.actual_condensed_work_state);
            case ddi::SETPOINT_CONDENSED_WORK_STATE_1_16:
                return Result<i32>::ok(sprayer.setpoint_condensed_work_state);
            default:
                break;
            }
        }

        // Product element (element 3)
        if (elem == SectionControlImplementSimulator::ELEM_PRODUCT) {
            switch (ddi) {
            case ddi::ACTUAL_VOLUME_PER_AREA_APPLICATION_RATE:
                return Result<i32>::ok(sprayer.actual_rate);
            case ddi::SETPOINT_VOLUME_PER_AREA_APPLICATION_RATE:
                return Result<i32>::ok(sprayer.setpoint_rate);
            case ddi::DEFAULT_VOLUME_PER_AREA_APPLICATION_RATE:
                return Result<i32>::ok(sprayer.prescription_rate);
            default:
                break;
            }
        }

        // Section elements
        i16 section_idx = sprayer.element_to_section_index(elem);
        if (section_idx >= 0) {
            switch (ddi) {
            case ddi::ACTUAL_WORK_STATE:
                return Result<i32>::ok(sprayer.sections[section_idx].state ? 1 : 0);
            default:
                break;
            }
        }

        echo::category("tc.req").warn("Unhandled request: elem=", elem, " DDI=", ddi);
        return Result<i32>::err(Error::invalid_state("unhandled DDI"));
    });

    // ─── Value Command Callback ──────────────────────────────────────────────
    tc_client.on_value_command([&](ElementNumber elem, DDI ddi, i32 value) -> Result<void> {
        echo::category("tc.cmd").info("Command: elem=", elem, " DDI=0x", dp::to_string(ddi).c_str(), " value=", value);

        // Boom element (element 2) - condensed work state commands
        if (elem == SectionControlImplementSimulator::ELEM_BOOM) {
            if (ddi == ddi::SETPOINT_CONDENSED_WORK_STATE_1_16) {
                sprayer.setpoint_condensed_work_state = value;
                sprayer.apply_setpoint_work_state();
                echo::info("Applied condensed work state: 0x", dp::to_string(value).c_str());
                return {};
            }
        }

        // Product element (element 3) - rate commands
        if (elem == SectionControlImplementSimulator::ELEM_PRODUCT) {
            switch (ddi) {
            case ddi::SETPOINT_VOLUME_PER_AREA_APPLICATION_RATE:
                sprayer.setpoint_rate = value;
                sprayer.actual_rate = value; // Simulate instant response
                echo::info("Setpoint rate: ", value / 10000.0f, " L/ha");
                return {};
            case ddi::DEFAULT_VOLUME_PER_AREA_APPLICATION_RATE:
                sprayer.prescription_rate = value;
                echo::info("Prescription rate: ", value / 10000.0f, " L/ha");
                return {};
            default:
                break;
            }
        }

        echo::category("tc.cmd").warn("Unhandled command: elem=", elem, " DDI=", ddi);
        return Result<void>::err(Error::invalid_state("unhandled command DDI"));
    });

    // ─── State Change Event ──────────────────────────────────────────────────
    tc_client.on_state_change.subscribe(
        [](TCState new_state) { echo::info(">>> TC state: ", tc_state_name(new_state)); });

    // ─── Connect ─────────────────────────────────────────────────────────────
    nm.start_address_claiming();
    auto connect_result = tc_client.connect();
    if (!connect_result.is_ok()) {
        echo::error("TC connect failed: ", connect_result.error().message);
        return 1;
    }
    echo::info("TC client connecting, state: ", tc_state_name(tc_client.state()));

    // ─── Main Loop ───────────────────────────────────────────────────────────
    signal(SIGINT, signal_handler);
    echo::info("");
    echo::info("Running... (Ctrl+C to stop)");
    echo::info("");

    u32 tick = 0;
    while (running) {
        u32 dt_ms = 50;

        nm.update(dt_ms);
        tc_client.update(dt_ms);

        // Print status every 2 seconds
        if (tick % 40 == 0) {
            sprayer.print_status();
            echo::info("TC state: ", tc_state_name(tc_client.state()));
            echo::info("");
        }

        // Simulate some manual section toggles for demo
        if (tick == 100) {
            echo::info("--- Demo: Manually toggling sections 1-3 ON ---");
            sprayer.set_section(0, true);
            sprayer.set_section(1, true);
            sprayer.set_section(2, true);
        }
        if (tick == 200) {
            echo::info("--- Demo: Manually toggling section 2 OFF ---");
            sprayer.set_section(1, false);
        }

        usleep(static_cast<useconds_t>(dt_ms * 1000));
        tick++;
    }

    // ─── Shutdown ────────────────────────────────────────────────────────────
    echo::info("");
    echo::info("=== Final Status ===");
    sprayer.print_status();
    echo::info("Bus load: ", nm.bus_load(0), "%");
    echo::info("");
    echo::info("=== Section Control Implement Simulator Complete ===");

    return 0;
}
