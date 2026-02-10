/*******************************************************************************
 * ISOBUS TASK CONTROLLER CLIENT EXAMPLE
 *******************************************************************************
 *
 * This example demonstrates how to create a Task Controller (TC) CLIENT.
 * A TC CLIENT represents an IMPLEMENT (like a sprayer, planter, etc.) that
 * communicates with a TC SERVER (typically on a tractor terminal).
 *
 * ARCHITECTURE OVERVIEW:
 * ═══════════════════════════════════════════════════════════════════════════
 *
 *     ┌─────────────────────┐         CAN Bus         ┌─────────────────────┐
 *     │   TC CLIENT         │◄──────────────────────►│   TC SERVER         │
 *     │   (This Code)       │                         │   (Tractor/VT)      │
 *     │                     │                         │                     │
 *     │  - Implement ECU    │   1) Address Claim      │  - Task Controller  │
 *     │  - Rate Controller  │   2) Partner Discovery  │  - Farm Management  │
 *     │  - DDOP Provider    │   3) DDOP Upload        │  - Data Logging     │
 *     │  - Data Reporter    │   4) Process Data       │  - Section Control  │
 *     └─────────────────────┘   5) Commands/Values    └─────────────────────┘
 *
 *
 * COMMUNICATION FLOW:
 * ═══════════════════════════════════════════════════════════════════════════
 *
 *   CLIENT                                SERVER
 *     │                                     │
 *     ├──[1] Address Claim (NAME)──────────►
 *     │                                     │
 *     ◄─────[2] TC Server Announces────────┤
 *     │        (Partner Discovery)          │
 *     │                                     │
 *     ├──[3] Working Set Master────────────►
 *     │      (I want to connect!)           │
 *     │                                     │
 *     ◄─────[4] Request DDOP───────────────┤
 *     │                                     │
 *     ├──[5] Upload DDOP────────────────────►
 *     │      (Device Structure)             │
 *     │                                     │
 *     ◄─────[6] Request Process Data───────┤
 *     │                                     │
 *     ├──[7] Report Values──────────────────►
 *     │      (Work State, Width, Rate)      │
 *     │                                     │
 *     ◄─────[8] Set Commands───────────────┤
 *     │      (Turn On/Off, Set Rate)        │
 *     │                                     │
 *     ├──[9] Confirm & Report───────────────►
 *     │      (Continuous Operation)         │
 *     └─────────────────────────────────────┘
 *
 ******************************************************************************/

#include <agrobus/isobus/tc/client.hpp>
#include <agrobus/isobus/tc/ddi_database.hpp>
#include <agrobus/isobus/tc/ddop.hpp>
#include <agrobus/isobus/tc/objects.hpp>
#include <agrobus/net/network_manager.hpp>
#include <echo/echo.hpp>
#include <wirebit/can/can_endpoint.hpp>
#include <wirebit/can/socketcan_link.hpp>

#include <atomic>
#include <csignal>

using namespace agrobus::net;
using namespace agrobus::isobus;
using namespace agrobus::isobus::tc;
namespace ddi = agrobus::isobus::tc::ddi;

// Signal handler for clean shutdown (Ctrl+C)
static std::atomic<bool> running{true};
// Current work state: 0 = not working, 1 = working
static std::atomic<i32> current_work_state{0};

void signal_handler(int) { running = false; }

/*******************************************************************************
 * DDOP OBJECT IDs
 *******************************************************************************
 *
 * The Device Descriptor Object Pool (DDOP) describes the structure of your
 * implement. Each object in the DDOP needs a unique ID. Think of these as
 * "handles" or "names" for different parts of your device.
 *
 * DDOP HIERARCHY VISUALIZED:
 * ═══════════════════════════════════════════════════════════════════════════
 *
 *                           Device (0)
 *                              │
 *                 ┌────────────┼────────────┐
 *                 │            │            │
 *          MainElement(1)  Connector(3)  Implement(6)
 *                 │            │            │
 *         WorkState(2)  ├──X-Offset(4) WorkingWidth(8)
 *                       └──Y-Offset(5)
 *
 * Additional Objects:
 *   - Presentations (100, 101): Define units (mm, m², etc.)
 *
 * OBJECT TYPES:
 *   - Device:         Root object containing device info
 *   - DeviceElement:  Physical/logical parts (connector, boom, etc.)
 *   - ProcessData:    Measurable values (width, rate, state)
 *   - Property:       Fixed characteristics (offsets, types)
 *   - Presentation:   How to display values (units, decimals)
 *
 ******************************************************************************/

// Element numbers for referencing in TC messages
namespace elem {
    inline constexpr ElementNumber DEVICE = 0;
    inline constexpr ElementNumber CONNECTOR = 1;
    inline constexpr ElementNumber IMPLEMENT = 2;
} // namespace elem

/*******************************************************************************
 * FUNCTION: create_simple_ddop()
 *******************************************************************************
 *
 * Creates a Device Descriptor Object Pool (DDOP) that describes our implement
 * to the Task Controller. The DDOP is like a blueprint or schema that tells
 * the TC:
 *   - What kind of device we are
 *   - What physical parts we have (elements)
 *   - What data we can provide (process data)
 *   - How to display that data (presentations)
 *
 * DDOP STRUCTURE FOR THIS EXAMPLE:
 * ═══════════════════════════════════════════════════════════════════════════
 *
 *   Device Object
 *   └─ "SimpleImplement" v1.0.0
 *      │
 *      ├─ MainDeviceElement (Type: Device)
 *      │  └─ DeviceActualWorkState (DDI: ActualWorkState)
 *      │     → Reports if device is working or not
 *      │
 *      ├─ Connector (Type: Connector)
 *      │  ├─ ConnectorXOffset (DDI: DeviceElementOffsetX)
 *      │  │  → Where connector is (fore/aft) in mm
 *      │  └─ ConnectorYOffset (DDI: DeviceElementOffsetY)
 *      │     → Where connector is (left/right) in mm
 *      │
 *      └─ Implement (Type: Function)
 *         └─ ImplementWorkingWidth (DDI: ActualWorkingWidth)
 *            → How wide is the working area in mm
 *
 ******************************************************************************/
DDOP create_simple_ddop() {
    DDOP ddop;

    //==========================================================================
    // VALUE PRESENTATIONS
    //==========================================================================
    // Presentations define how values should be displayed to the operator.
    // They specify units, decimal places, and scale factors.

    // Width presentation: millimeters, no scaling, whole numbers
    auto vp_mm_id = ddop.next_id();
    ddop.add_value_presentation(
        DeviceValuePresentation{}.set_id(vp_mm_id).set_offset(0).set_scale(1.0f).set_decimals(0).set_unit("mm"));

    // Area presentation: square meters
    auto vp_area_id = ddop.next_id();
    ddop.add_value_presentation(
        DeviceValuePresentation{}.set_id(vp_area_id).set_offset(0).set_scale(1.0f).set_decimals(0).set_unit("m2"));

    //==========================================================================
    // DEVICE OBJECT (Root)
    //==========================================================================
    // The device object is the root of the DDOP tree and contains metadata
    // about the entire implement.
    auto device_id = ddop.next_id();
    ddop.add_device(DeviceObject{}
                        .set_id(device_id)
                        .set_designator("SimpleImpl")
                        .set_software_version("1.0.0")
                        .set_serial_number("001")
                        .set_structure_label({'S', 'M', 'P', '1', '.', '0', 0})
                        .set_localization_label({'e', 'n', 0x50, 0x00, 0x55, 0x55, 0xFF}));

    //==========================================================================
    // MAIN DEVICE ELEMENT (Element 0)
    //==========================================================================
    // This represents the main implement itself. Every DDOP must have at least
    // one device element.
    auto main_elem_id = ddop.next_id();

    // Process data: Actual Work State (DDI 0x00A1)
    // Reports whether the device is working or not.
    // Values: 0 = Not working / Off, 1 = Working / On
    auto work_state_pd_id = ddop.next_id();
    ddop.add_process_data(DeviceProcessData{}
                              .set_id(work_state_pd_id)
                              .set_ddi(ddi::ACTUAL_WORK_STATE)
                              .add_trigger(TriggerMethod::OnChange)
                              .add_trigger(TriggerMethod::TimeInterval)
                              .set_designator("WorkState"));

    DeviceElement main_elem = DeviceElement{}
                                  .set_id(main_elem_id)
                                  .set_type(DeviceElementType::Device)
                                  .set_number(elem::DEVICE)
                                  .set_designator("Implement")
                                  .set_parent(device_id)
                                  .add_child(work_state_pd_id);

    //==========================================================================
    // CONNECTOR ELEMENT (Element 1)
    //==========================================================================
    // The connector represents the physical attachment point (hitch) where
    // the implement connects to the tractor. This is important for TC-GEO
    // applications to calculate positions accurately.
    auto connector_elem_id = ddop.next_id();

    // Property: Connector X Offset (fore/aft position)
    // X-AXIS: Positive = Forward, Negative = Backward
    auto conn_x_prop_id = ddop.next_id();
    ddop.add_property(DeviceProperty{}
                          .set_id(conn_x_prop_id)
                          .set_ddi(ddi::DEVICE_ELEMENT_OFFSET_X)
                          .set_value(0) // 0 mm offset
                          .set_presentation(vp_mm_id)
                          .set_designator("ConnX"));

    // Property: Connector Y Offset (left/right position)
    // Y-AXIS: Positive = Right, Negative = Left (driver's perspective)
    auto conn_y_prop_id = ddop.next_id();
    ddop.add_property(DeviceProperty{}
                          .set_id(conn_y_prop_id)
                          .set_ddi(ddi::DEVICE_ELEMENT_OFFSET_Y)
                          .set_value(0) // Centered
                          .set_presentation(vp_mm_id)
                          .set_designator("ConnY"));

    DeviceElement connector_elem = DeviceElement{}
                                       .set_id(connector_elem_id)
                                       .set_type(DeviceElementType::Connector)
                                       .set_number(elem::CONNECTOR)
                                       .set_designator("Connector")
                                       .set_parent(main_elem_id)
                                       .add_child(conn_x_prop_id)
                                       .add_child(conn_y_prop_id);

    //==========================================================================
    // IMPLEMENT FUNCTION ELEMENT (Element 2)
    //==========================================================================
    // This represents the working function of the implement (e.g., boom, tool,
    // applicator). This is where the actual work happens.
    auto implement_elem_id = ddop.next_id();

    // Process data: Actual Working Width (DDI 0x0043)
    // Reports the current active working width in millimeters.
    auto width_pd_id = ddop.next_id();
    ddop.add_process_data(DeviceProcessData{}
                              .set_id(width_pd_id)
                              .set_ddi(ddi::ACTUAL_WORKING_WIDTH)
                              .add_trigger(TriggerMethod::OnChange)
                              .set_presentation(vp_mm_id)
                              .set_designator("WorkWidth"));

    DeviceElement implement_elem = DeviceElement{}
                                       .set_id(implement_elem_id)
                                       .set_type(DeviceElementType::Function)
                                       .set_number(elem::IMPLEMENT)
                                       .set_designator("Function")
                                       .set_parent(main_elem_id)
                                       .add_child(width_pd_id);

    // Link implement and connector as children of main element
    main_elem.add_child(connector_elem_id);
    main_elem.add_child(implement_elem_id);

    // Add elements to DDOP
    ddop.add_element(main_elem);
    ddop.add_element(connector_elem);
    ddop.add_element(implement_elem);

    return ddop;
}

/*******************************************************************************
 * TC STATE NAME HELPER
 ******************************************************************************/
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

/*******************************************************************************
 * MAIN FUNCTION
 ******************************************************************************/
int main(int argc, char *argv[]) {
    echo::box("ISOBUS TASK CONTROLLER CLIENT TUTORIAL");

    dp::String interface = "vcan0";
    if (argc > 1)
        interface = argv[1];

    //==========================================================================
    // STEP 1: CAN BUS SETUP
    //==========================================================================
    // Initialize the CAN hardware interface. This sets up communication with
    // the physical CAN bus hardware (SocketCAN on Linux).
    echo::info("[1/7] Initializing CAN interface: ", interface);

    auto link_result = wirebit::SocketCanLink::create({.interface_name = interface, .create_if_missing = true});
    if (!link_result.is_ok()) {
        echo::error("Failed to open ", interface, ": ", link_result.error().message);
        echo::info("Run: sudo modprobe vcan && sudo ip link add dev ", interface,
                   " type vcan && sudo ip link set ", interface, " up");
        return 1;
    }
    auto link = std::make_shared<wirebit::SocketCanLink>(std::move(link_result.value()));
    wirebit::CanEndpoint can(link, wirebit::CanConfig{.bitrate = 250000}, 1);

    echo::info("  CAN interface started successfully");

    // Setup signal handler for graceful shutdown
    std::signal(SIGINT, signal_handler);

    //==========================================================================
    // STEP 2: NETWORK MANAGER SETUP
    //==========================================================================
    // The IsoNet (Network Manager) handles all ISOBUS communication:
    //   - Address claiming
    //   - Message routing
    //   - Transport protocols (TP/ETP)
    echo::info("[2/7] Creating Network Manager...");

    IsoNet nm(NetworkConfig{}.bus_load(true));
    nm.set_endpoint(0, &can);

    echo::info("  Network Manager created");

    //==========================================================================
    // STEP 3: CREATE ISO NAME
    //==========================================================================
    // The ISO NAME is a 64-bit identifier that uniquely identifies our ECU
    // on the ISOBUS network. It's used during the address claim process.
    //
    // ISO NAME STRUCTURE (64 bits):
    // ┌──────────────────────────────────────────────────────────────────┐
    // │ Field                    │ Description                           │
    // ├──────────────────────────┼───────────────────────────────────────┤
    // │ Arbitrary Address Capable│ Can we change our address?            │
    // │ Industry Group           │ 2 = Agriculture/Forestry              │
    // │ Device Class             │ 6 = Rate Control                      │
    // │ Function                 │ 128 = Rate Control                    │
    // │ Manufacturer Code        │ 64 = Example                          │
    // │ Identity Number          │ Serial number (42)                    │
    // └──────────────────────────────────────────────────────────────────┘
    echo::info("[3/7] Creating ISO NAME and Internal Control Function...");

    Name my_name = Name::build()
                       .set_identity_number(42)
                       .set_manufacturer_code(64)
                       .set_function_code(128) // Rate Control
                       .set_device_class(6)    // Rate Control
                       .set_industry_group(2)  // Agriculture
                       .set_self_configurable(true);

    auto cf_result = nm.create_internal(my_name, 0, 0x28);
    if (!cf_result.is_ok()) {
        echo::error("Failed to create Internal CF: ", cf_result.error().message);
        return 1;
    }
    auto *cf = cf_result.value();

    echo::info("  Internal CF created, preferred address: 0x28");

    //==========================================================================
    // STEP 4: CREATE DEVICE DESCRIPTOR OBJECT POOL (DDOP)
    //==========================================================================
    // The DDOP is a data structure that describes our implement to the TC.
    // It defines:
    //   - What our device is
    //   - What parts it has
    //   - What data we can provide
    //   - How that data should be displayed
    echo::info("[4/7] Creating Device Descriptor Object Pool (DDOP)...");

    DDOP ddop = create_simple_ddop();

    auto validation = ddop.validate();
    if (!validation.is_ok()) {
        echo::error("DDOP validation failed: ", validation.error().message);
        return 1;
    }

    echo::info("  DDOP created with ", ddop.object_count(), " objects");

    auto serialized = ddop.serialize();
    if (serialized.is_ok()) {
        echo::info("  DDOP binary size: ", serialized.value().size(), " bytes");
    }

    //==========================================================================
    // STEP 5: CREATE TASK CONTROLLER CLIENT
    //==========================================================================
    // The TaskControllerClient manages all TC client operations:
    //   - Connection management
    //   - DDOP upload
    //   - Process data handling
    //   - Command processing
    //   - State machine management
    echo::info("[5/7] Creating Task Controller Client...");

    TCClientConfig tc_config;
    tc_config.timeout(6000); // 6 second timeout

    TaskControllerClient tc_client(nm, cf, tc_config);
    tc_client.set_ddop(std::move(ddop));

    echo::info("  TC Client created");

    //==========================================================================
    // STEP 6: SETUP CALLBACKS
    //==========================================================================
    // Callbacks are functions that get called when the TC requests data or
    // sends commands.
    //
    // TWO MAIN CALLBACK TYPES:
    //
    // 1. REQUEST VALUE CALLBACK:
    //    Called when TC asks: "What is your current working width?"
    //    We must respond with the requested value.
    //
    // 2. VALUE COMMAND CALLBACK:
    //    Called when TC commands: "Turn on section 3"
    //    We must execute the command and respond with success/failure.
    echo::info("[6/7] Setting up callbacks...");

    //--------------------------------------------------------------------------
    // REQUEST VALUE CALLBACK
    //--------------------------------------------------------------------------
    // Called when TC requests a value from us.
    //
    // Parameters:
    //   elementNumber: Which element (0=Device, 1=Connector, 2=Implement)
    //   ddi:           What data is requested (DDI code)
    //
    // Return: Result<i32> with the value, or error if not supported
    tc_client.on_value_request([](ElementNumber element, DDI ddi) -> Result<i32> {
        echo::category("tc.callback").debug("Value request: elem=", element, " DDI=0x", dp::to_string(ddi).c_str());

        // Device element (element 0) - Work State
        if (element == elem::DEVICE) {
            if (ddi == ddi::ACTUAL_WORK_STATE) {
                i32 state = current_work_state.load();
                echo::category("tc.callback").info("  Reporting work state: ", state ? "WORKING" : "NOT WORKING");
                return Result<i32>::ok(state);
            }
        }

        // Implement element (element 2) - Working Width
        if (element == elem::IMPLEMENT) {
            if (ddi == ddi::ACTUAL_WORKING_WIDTH) {
                i32 width_mm = 3000; // 3 meters
                echo::category("tc.callback").info("  Reporting width: ", width_mm, " mm");
                return Result<i32>::ok(width_mm);
            }
        }

        echo::category("tc.callback").warn("  Unknown request: elem=", element, " DDI=", ddi);
        return Result<i32>::err(Error::invalid_state("unsupported DDI"));
    });

    //--------------------------------------------------------------------------
    // VALUE COMMAND CALLBACK
    //--------------------------------------------------------------------------
    // Called when TC sends a command to us.
    //
    // Parameters:
    //   elementNumber: Which element to command
    //   ddi:           What type of command (DDI code)
    //   value:         The commanded value
    //
    // Return: Result<void> success or error
    tc_client.on_value_command([](ElementNumber element, DDI ddi, i32 value) -> Result<void> {
        echo::category("tc.callback")
            .info("Command received: elem=", element, " DDI=0x", dp::to_string(ddi).c_str(), " value=", value);

        // Setpoint Work State command
        if (ddi == ddi::SETPOINT_WORK_STATE) {
            echo::category("tc.callback").info("  TC commanded: ", value ? "TURN ON" : "TURN OFF");
            current_work_state.store(value);
            return {};
        }

        echo::category("tc.callback").warn("  Unknown command");
        return Result<void>::err(Error::invalid_state("unsupported command"));
    });

    echo::info("  Callbacks registered");

    //==========================================================================
    // STEP 7: STATE CHANGE EVENT
    //==========================================================================
    tc_client.on_state_change.subscribe(
        [](TCState new_state) { echo::info(">>> TC state changed: ", tc_state_name(new_state)); });

    //==========================================================================
    // STEP 8: START AND CONNECT
    //==========================================================================
    echo::info("[7/7] Starting address claim and TC connection...");

    nm.start_address_claiming();

    auto connect_result = tc_client.connect();
    if (!connect_result.is_ok()) {
        echo::error("TC connect failed: ", connect_result.error().message);
        return 1;
    }

    echo::info("  TC Client connecting, state: ", tc_state_name(tc_client.state()));

    echo::box("TC CLIENT IS RUNNING");

    echo::info("The client is waiting for a TC Server to appear on the bus.");
    echo::info("Watch for TC communication events above.");
    echo::info("Press Ctrl+C to exit cleanly.");
    echo::info("");

    //==========================================================================
    // MAIN LOOP - MONITORING
    //==========================================================================
    // The TC client state machine runs in update(), so we call it regularly.
    u32 tick = 0;
    while (running) {
        u32 dt_ms = 100;

        nm.update(dt_ms);
        tc_client.update(dt_ms);

        // Print status every 10 seconds
        if (tick % 100 == 0) {
            echo::info("────────────────────────────────────────────────────────");
            echo::info("[Status] TC Client state: ", tc_state_name(tc_client.state()));
            echo::info("         Work state: ", current_work_state.load() ? "WORKING" : "NOT WORKING");
            echo::info("────────────────────────────────────────────────────────");
        }

        // Toggle work state every 5 seconds for demo
        if (tick % 50 == 0 && tick > 0) {
            i32 new_state = 1 - current_work_state.load();
            current_work_state.store(new_state);
            echo::info("[AUTO-TOGGLE] Work state -> ", new_state ? "WORKING" : "NOT WORKING");
        }

        usleep(static_cast<useconds_t>(dt_ms * 1000));
        tick++;
    }

    //==========================================================================
    // CLEANUP AND SHUTDOWN
    //==========================================================================
    echo::box("SHUTTING DOWN");

    echo::info("Disconnecting TC client...");
    tc_client.disconnect();

    echo::info("Shutdown complete. Goodbye!");

    return 0;
}
