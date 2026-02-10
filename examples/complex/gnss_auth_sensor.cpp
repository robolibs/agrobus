/*******************************************************************************
 * GNSS AUTHENTICATION SENSOR - TASK CONTROLLER CLIENT EXAMPLE
 *******************************************************************************
 *
 * This example demonstrates a GNSS authentication sensor that:
 *   - Reads PHTG NMEA sentences from a serial port using wirebit::TtyLink
 *   - Parses authentication status and warning codes
 *   - Reports these values to a Task Controller via ISOBUS
 *   - Uses proprietary DDI for custom authentication data
 *
 * ARCHITECTURE:
 * ═══════════════════════════════════════════════════════════════════════════
 *
 *   ┌─────────────────┐     NMEA      ┌─────────────────┐
 *   │  GNSS Receiver  │───────────────│  Serial Port    │
 *   │  (with auth)    │   $PHTG,...   │  /dev/ttyUSB0   │
 *   └─────────────────┘               └────────┬────────┘
 *                                              │
 *                                              ▼
 *                                    ┌─────────────────┐
 *                                    │  wirebit::TtyLink│
 *                                    │  + SerialEndpoint│
 *                                    └────────┬────────┘
 *                                              │
 *                                              ▼
 *                                    ┌─────────────────┐
 *                                    │  This Example   │
 *                                    │  - Parse PHTG   │
 *                                    │  - TC Client    │
 *                                    └────────┬────────┘
 *                                              │
 *                                              ▼ CAN Bus
 *                                    ┌─────────────────┐
 *                                    │  Task Controller│
 *                                    │  (Farm Terminal)│
 *                                    └─────────────────┘
 *
 * PHTG SENTENCE FORMAT:
 * ═══════════════════════════════════════════════════════════════════════════
 *
 *   $PHTG,<date>,<time>,<system>,<service>,<auth_result>,<warning>*<cs>
 *
 *   Fields:
 *     date        - Date (DDMMYY)
 *     time        - Time (HHMMSS.ss)
 *     system      - GNSS system (e.g., "GPS", "GAL")
 *     service     - Service type (e.g., "OSNMA", "CHIMERA")
 *     auth_result - Authentication result: 0=unknown, 1=ok, 2=fail
 *     warning     - Warning code (vendor specific)
 *     cs          - NMEA checksum (XOR of bytes between $ and *)
 *
 *   Example: $PHTG,250125,143052.00,GPS,OSNMA,1,0*5A
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
#include <wirebit/serial/serial_endpoint.hpp>
#include <wirebit/serial/tty_link.hpp>

#include <atomic>
#include <csignal>
#include <sstream>

using namespace agrobus::net;
using namespace agrobus::isobus;
using namespace agrobus::isobus::tc;
namespace ddi = agrobus::isobus::tc::ddi;

// ─── Signal handling ─────────────────────────────────────────────────────────
static std::atomic<bool> running{true};
void signal_handler(int) { running = false; }

// ─── GNSS Authentication State ───────────────────────────────────────────────
static std::atomic<i32> gnss_auth_status{0}; // 0=unknown, 1=authenticated, 2=failed
static std::atomic<i32> gnss_warning{0};     // Warning code from receiver

// ─── Proprietary DDI for authentication result ───────────────────────────────
// DDI range 57344-65534 is reserved for proprietary use (ISO 11783-11)
static constexpr DDI DDI_AUTH_RESULT = 65432;
static constexpr DDI DDI_AUTH_WARNING = 65433;

// ─── PHTG Data Structure ─────────────────────────────────────────────────────
struct PHTGData {
    dp::String date;
    dp::String time;
    dp::String system;
    dp::String service;
    i32 auth_result = 0;
    i32 warning = 0;
};

/*******************************************************************************
 * NMEA CHECKSUM VALIDATION
 ******************************************************************************/
static bool validate_nmea_checksum(const dp::String &sentence) {
    auto star_pos = sentence.find('*');
    if (star_pos == dp::String::npos || star_pos + 2 >= sentence.size()) {
        return false;
    }

    u8 calc_cs = 0;
    for (usize i = 1; i < star_pos; ++i) {
        calc_cs ^= static_cast<u8>(sentence[i]);
    }

    dp::String cs_str = sentence.substr(star_pos + 1, 2);
    i32 recv_cs = std::stoi(cs_str.c_str(), nullptr, 16);

    return calc_cs == static_cast<u8>(recv_cs);
}

/*******************************************************************************
 * PHTG SENTENCE PARSER
 ******************************************************************************/
static bool parse_phtg(const dp::String &sentence, PHTGData &data) {
    // Check minimum length and prefix
    if (sentence.size() < 5 || sentence.substr(0, 5) != "$PHTG") {
        return false;
    }

    // Validate checksum
    if (!validate_nmea_checksum(sentence)) {
        echo::category("nmea").warn("PHTG checksum invalid");
        return false;
    }

    // Find end of data (before checksum)
    auto star_pos = sentence.find('*');
    if (star_pos == dp::String::npos) {
        return false;
    }

    // Extract body between "$PHTG," and "*"
    dp::String body = sentence.substr(6, star_pos - 6);

    // Parse comma-separated fields
    std::stringstream ss(body.c_str());
    dp::String token;
    i32 field = 0;

    char buf[256];
    while (ss.getline(buf, sizeof(buf), ',')) {
        token = buf;
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
            data.auth_result = token.empty() ? 0 : std::stoi(token.c_str());
            break;
        case 5:
            data.warning = token.empty() ? 0 : std::stoi(token.c_str());
            break;
        default:
            break;
        }
        field++;
    }

    return field >= 6;
}

/*******************************************************************************
 * NMEA LINE BUFFER
 * Accumulates bytes from serial and extracts complete lines
 ******************************************************************************/
class NmeaLineBuffer {
    dp::String buffer_;

  public:
    // Add bytes to buffer and extract complete lines
    dp::Vector<dp::String> add_bytes(const wirebit::Bytes &data) {
        dp::Vector<dp::String> lines;

        for (auto byte : data) {
            char c = static_cast<char>(byte);
            if (c == '\n' || c == '\r') {
                if (!buffer_.empty()) {
                    lines.push_back(buffer_);
                    buffer_.clear();
                }
            } else {
                buffer_ += c;
            }
        }

        return lines;
    }
};

/*******************************************************************************
 * DDOP CREATION
 *******************************************************************************
 *
 * DDOP STRUCTURE:
 * ═══════════════════════════════════════════════════════════════════════════
 *
 *   Device (GNSS Auth Sensor)
 *   └─ MainDeviceElement (element 0)
 *      ├─ ActualWorkState (DDI 0x00A1) - Standard work state
 *      ├─ AuthResult (DDI 65432)       - Proprietary auth status
 *      └─ AuthWarning (DDI 65433)      - Proprietary warning code
 *
 ******************************************************************************/

// Element numbers
namespace elem {
    inline constexpr ElementNumber DEVICE = 0;
} // namespace elem

DDOP create_sensor_ddop() {
    DDOP ddop;

    // ─── Value Presentations ─────────────────────────────────────────────────
    auto vp_raw_id = ddop.next_id();
    ddop.add_value_presentation(
        DeviceValuePresentation{}.set_id(vp_raw_id).set_offset(0).set_scale(1.0f).set_decimals(0).set_unit("raw"));

    // ─── Device Object ───────────────────────────────────────────────────────
    auto device_id = ddop.next_id();
    ddop.add_device(DeviceObject{}
                        .set_id(device_id)
                        .set_designator("GNSSAuth")
                        .set_software_version("1.0.0")
                        .set_serial_number("AUTH001")
                        .set_structure_label({'G', 'N', 'S', 'S', 'A', '1', 0})
                        .set_localization_label({'e', 'n', 0x50, 0x00, 0x55, 0x55, 0xFF}));

    // ─── Main Device Element ─────────────────────────────────────────────────
    auto main_elem_id = ddop.next_id();

    // Actual Work State (standard DDI)
    auto work_state_pd_id = ddop.next_id();
    ddop.add_process_data(DeviceProcessData{}
                              .set_id(work_state_pd_id)
                              .set_ddi(ddi::ACTUAL_WORK_STATE)
                              .add_trigger(TriggerMethod::OnChange)
                              .add_trigger(TriggerMethod::TimeInterval)
                              .set_designator("WorkState"));

    // Authentication Result (proprietary DDI)
    auto auth_result_pd_id = ddop.next_id();
    ddop.add_process_data(DeviceProcessData{}
                              .set_id(auth_result_pd_id)
                              .set_ddi(DDI_AUTH_RESULT)
                              .add_trigger(TriggerMethod::OnChange)
                              .add_trigger(TriggerMethod::TimeInterval)
                              .set_presentation(vp_raw_id)
                              .set_designator("AuthResult"));

    // Authentication Warning (proprietary DDI)
    auto auth_warning_pd_id = ddop.next_id();
    ddop.add_process_data(DeviceProcessData{}
                              .set_id(auth_warning_pd_id)
                              .set_ddi(DDI_AUTH_WARNING)
                              .add_trigger(TriggerMethod::OnChange)
                              .add_trigger(TriggerMethod::TimeInterval)
                              .set_presentation(vp_raw_id)
                              .set_designator("AuthWarn"));

    DeviceElement main_elem = DeviceElement{}
                                  .set_id(main_elem_id)
                                  .set_type(DeviceElementType::Device)
                                  .set_number(elem::DEVICE)
                                  .set_designator("Sensor")
                                  .set_parent(device_id)
                                  .add_child(work_state_pd_id)
                                  .add_child(auth_result_pd_id)
                                  .add_child(auth_warning_pd_id);

    ddop.add_element(main_elem);

    return ddop;
}

/*******************************************************************************
 * TC STATE NAME HELPER
 ******************************************************************************/
static const char *tc_state_name(TCState s) {
    switch (s) {
    case TCState::Disconnected:
        return "Disconnected";
    case TCState::WaitForServerStatus:
        return "WaitForServerStatus";
    case TCState::SendWorkingSetMaster:
        return "SendWorkingSetMaster";
    case TCState::RequestVersion:
        return "RequestVersion";
    case TCState::WaitForVersion:
        return "WaitForVersion";
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

static const char *auth_status_name(i32 status) {
    switch (status) {
    case 0:
        return "Unknown";
    case 1:
        return "Authenticated";
    case 2:
        return "Failed";
    default:
        return "Invalid";
    }
}

/*******************************************************************************
 * MAIN
 ******************************************************************************/
int main(int argc, char *argv[]) {
    echo::box("GNSS AUTHENTICATION SENSOR - TC CLIENT");

    // ─── Parse arguments ─────────────────────────────────────────────────────
    dp::String can_interface = "vcan0";
    dp::String serial_device = "/dev/ttyUSB0";
    u32 serial_baud = 115200;

    for (int i = 1; i < argc; ++i) {
        dp::String arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            echo::info("Usage: ", argv[0], " [can_interface] [serial_device] [baud]");
            echo::info("  can_interface: CAN interface (default: vcan0)");
            echo::info("  serial_device: Serial port for NMEA (default: /dev/ttyUSB0)");
            echo::info("  baud:          Baud rate (default: 115200)");
            return 0;
        } else if (i == 1) {
            can_interface = arg;
        } else if (i == 2) {
            serial_device = arg;
        } else if (i == 3) {
            serial_baud = static_cast<u32>(std::atoi(arg.c_str()));
        }
    }

    echo::info("CAN interface: ", can_interface);
    echo::info("Serial device: ", serial_device, " @ ", serial_baud, " baud");
    echo::info("");

    std::signal(SIGINT, signal_handler);

    // ─── Serial port setup using wirebit::TtyLink ────────────────────────────
    echo::info("[1/5] Opening serial port with wirebit::TtyLink...");

    std::shared_ptr<wirebit::TtyLink> tty_link;
    auto tty_result = wirebit::TtyLink::create({.device = serial_device, .baud = serial_baud});

    if (!tty_result.is_ok()) {
        echo::warn("Serial port not available: ", tty_result.error().message);
        echo::info("  Continuing without GNSS data (will report auth_status=0)");
    } else {
        tty_link = std::make_shared<wirebit::TtyLink>(std::move(tty_result.value()));
        echo::info("  Serial port opened via wirebit::TtyLink");
    }

    // NMEA line buffer for accumulating bytes into lines
    NmeaLineBuffer nmea_buffer;

    // ─── CAN bus setup ───────────────────────────────────────────────────────
    echo::info("[2/5] Initializing CAN interface...");

    auto link_result = wirebit::SocketCanLink::create({.interface_name = can_interface, .create_if_missing = true});
    if (!link_result.is_ok()) {
        echo::error("Failed to open ", can_interface, ": ", link_result.error().message);
        echo::info("Run: sudo modprobe vcan && sudo ip link add dev ", can_interface,
                   " type vcan && sudo ip link set ", can_interface, " up");
        return 1;
    }
    auto can_link = std::make_shared<wirebit::SocketCanLink>(std::move(link_result.value()));
    wirebit::CanEndpoint can(can_link, wirebit::CanConfig{.bitrate = 250000}, 1);

    echo::info("  CAN interface ready");

    // ─── Network Manager ─────────────────────────────────────────────────────
    echo::info("[3/5] Creating Network Manager and Control Function...");

    IsoNet nm(NetworkConfig{}.bus_load(true));
    nm.set_endpoint(0, &can);

    // ISO NAME for Object Detection Sensor (function code 75)
    Name my_name = Name::build()
                       .set_identity_number(42)
                       .set_manufacturer_code(64)
                       .set_function_code(75) // Object Detection Sensor
                       .set_device_class(0)   // Non-specific
                       .set_industry_group(2) // Agriculture
                       .set_self_configurable(true);

    auto cf_result = nm.create_internal(my_name, 0, 0x80);
    if (!cf_result.is_ok()) {
        echo::error("Failed to create Internal CF: ", cf_result.error().message);
        return 1;
    }
    auto *cf = cf_result.value();

    echo::info("  Internal CF created, preferred address: 0x80");

    // ─── DDOP ────────────────────────────────────────────────────────────────
    echo::info("[4/5] Creating DDOP...");

    DDOP ddop = create_sensor_ddop();

    auto validation = ddop.validate();
    if (!validation.is_ok()) {
        echo::error("DDOP validation failed: ", validation.error().message);
        return 1;
    }

    echo::info("  DDOP created: ", ddop.object_count(), " objects");

    // ─── TC Client ───────────────────────────────────────────────────────────
    echo::info("[5/5] Creating TC Client...");

    TCClientConfig tc_config;
    tc_config.timeout(6000);

    TaskControllerClient tc_client(nm, cf, tc_config);
    tc_client.set_ddop(std::move(ddop));

    // Value request callback
    tc_client.on_value_request([](ElementNumber element, DDI ddi) -> Result<i32> {
        if (element != elem::DEVICE) {
            return Result<i32>::err(Error::invalid_state("unknown element"));
        }

        switch (ddi) {
        case ddi::ACTUAL_WORK_STATE:
            // Report working if we have valid auth data
            return Result<i32>::ok(gnss_auth_status.load() > 0 ? 1 : 0);

        case DDI_AUTH_RESULT:
            return Result<i32>::ok(gnss_auth_status.load());

        case DDI_AUTH_WARNING:
            return Result<i32>::ok(gnss_warning.load());

        default:
            return Result<i32>::err(Error::invalid_state("unknown DDI"));
        }
    });

    // Value command callback (we don't accept commands for this sensor)
    tc_client.on_value_command([](ElementNumber, DDI, i32) -> Result<void> {
        return Result<void>::err(Error::invalid_state("sensor is read-only"));
    });

    // State change notification
    tc_client.on_state_change.subscribe(
        [](TCState new_state) { echo::info(">>> TC state: ", tc_state_name(new_state)); });

    echo::info("  TC Client created");

    // ─── Start ───────────────────────────────────────────────────────────────
    echo::box("GNSS AUTH SENSOR RUNNING");

    nm.start_address_claiming();

    auto connect_result = tc_client.connect();
    if (!connect_result.is_ok()) {
        echo::error("TC connect failed: ", connect_result.error().message);
        return 1;
    }

    echo::info("Waiting for TC Server and GNSS data...");
    echo::info("Press Ctrl+C to stop");
    echo::info("");

    // ─── Main loop ───────────────────────────────────────────────────────────
    i32 last_auth = gnss_auth_status.load();
    i32 last_warning = gnss_warning.load();
    u32 tick = 0;

    while (running) {
        u32 dt_ms = 50;

        // ─── Read serial data using wirebit::TtyLink ─────────────────────────
        if (tty_link) {
            auto recv_result = tty_link->recv();
            if (recv_result.is_ok()) {
                auto &frame = recv_result.value();
                auto lines = nmea_buffer.add_bytes(frame.payload);

                for (const auto &line : lines) {
                    if (line.size() >= 5 && line.substr(0, 5) == "$PHTG") {
                        PHTGData phtg;
                        if (parse_phtg(line, phtg)) {
                            i32 old_auth = gnss_auth_status.load();
                            gnss_auth_status.store(phtg.auth_result);
                            gnss_warning.store(phtg.warning);

                            if (phtg.auth_result != old_auth) {
                                echo::category("gnss").info("Auth changed: ", auth_status_name(old_auth), " -> ",
                                                            auth_status_name(phtg.auth_result));
                            }
                        }
                    }
                }
            }
        }

        // ─── Update network and TC client ────────────────────────────────────
        nm.update(dt_ms);
        tc_client.update(dt_ms);

        // Check for auth status changes
        i32 auth = gnss_auth_status.load();
        i32 warning = gnss_warning.load();

        if (auth != last_auth || warning != last_warning) {
            echo::info("GNSS update: auth=", auth_status_name(auth), " warning=", warning);
            last_auth = auth;
            last_warning = warning;
        }

        // Print status every 10 seconds
        if (tick % 200 == 0) {
            echo::info("────────────────────────────────────────");
            echo::info("TC state:    ", tc_state_name(tc_client.state()));
            echo::info("Auth status: ", auth_status_name(gnss_auth_status.load()));
            echo::info("Warning:     ", gnss_warning.load());
            if (tty_link) {
                echo::info("Serial RX:   ", tty_link->stats().bytes_received, " bytes");
            }
            echo::info("────────────────────────────────────────");
        }

        usleep(static_cast<useconds_t>(dt_ms * 1000));
        tick++;
    }

    // ─── Shutdown ────────────────────────────────────────────────────────────
    echo::box("SHUTTING DOWN");

    tc_client.disconnect();
    tty_link.reset(); // Close serial port

    echo::info("Goodbye!");

    return 0;
}
