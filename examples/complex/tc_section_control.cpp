/// @file tc_section_control.cpp
/// @brief Comprehensive Task Controller client example with full DDOP and
///        16-section sprayer section control.
///
/// This example demonstrates:
///   - Setting up IsoNet, Name, and InternalCF
///   - Building a complete DDOP (Device Descriptor Object Pool) for a 16-section sprayer
///   - Creating a TaskControllerClient and initiating connection
///   - Handling value requests (section state, actual work state, area counters)
///   - Handling value commands (section control setpoint)
///   - Running the TC client state machine

#include <agrobus/isobus/tc/client.hpp>
#include <agrobus/isobus/tc/ddi_database.hpp>
#include <agrobus/isobus/tc/ddop.hpp>
#include <agrobus/isobus/tc/objects.hpp>
#include <agrobus/net/network_manager.hpp>
#include <echo/echo.hpp>

using namespace agrobus::net;
using namespace agrobus::isobus;
using namespace agrobus::isobus::tc;
namespace ddi = agrobus::isobus::tc::ddi;

// ─── Sprayer Application State ──────────────────────────────────────────────────
struct SprayerState {
    static constexpr u16 NUM_SECTIONS = 16;

    dp::Array<bool, NUM_SECTIONS> section_states{}; // Current on/off for each section
    dp::Array<i32, NUM_SECTIONS> area_counters{};   // Accumulated area per section (m2)
    i32 total_area = 0;                             // Total sprayed area (m2)
    i32 effective_total_area = 0;                   // Effective total area (m2)
    i32 working_width_mm = 24000;                   // 24m boom width in mm (1.5m per section)

    void set_section(u16 section_index, bool on) {
        if (section_index < NUM_SECTIONS) {
            section_states[section_index] = on;
            echo::category("sprayer").info("Section ", section_index + 1, " -> ", on ? "ON" : "OFF");
        }
    }

    void accumulate_area(u32 elapsed_ms) {
        // Simulate area accumulation: each active section contributes
        // section_width * simulated_speed * dt
        i32 section_width_mm = working_width_mm / static_cast<i32>(NUM_SECTIONS);
        i32 speed_mm_per_s = 3000; // 3 m/s simulated ground speed

        for (u16 i = 0; i < NUM_SECTIONS; ++i) {
            if (section_states[i]) {
                // area in mm2, then convert to m2 (divide by 1e6)
                i32 delta_area_m2 = (section_width_mm * speed_mm_per_s * static_cast<i32>(elapsed_ms)) / 1000000;
                area_counters[i] += delta_area_m2;
                total_area += delta_area_m2;
                effective_total_area += delta_area_m2;
            }
        }
    }

    i32 section_work_state(u16 section_index) const {
        if (section_index < NUM_SECTIONS) {
            return section_states[section_index] ? 1 : 0;
        }
        return 0;
    }
};

// ─── Build the DDOP ─────────────────────────────────────────────────────────────
/// Constructs a realistic Device Descriptor Object Pool for a 16-section sprayer.
///
/// Object hierarchy:
///   DeviceObject (SprayerECU)
///     DeviceElement[Device] (Sprayer, element 0)
///       DeviceElement[Function] (Boom, element 1)
///         DeviceProcessData: Total Area (DDI 0x0074)
///         DeviceProcessData: Effective Total Area (DDI 0x0075)
///         DeviceProperty: Working Width (DDI 0x0043, value=24000mm)
///         DeviceElement[Section] (Section 1, element 2)
///           DeviceProcessData: Setpoint Section Control State (DDI 0x0001)
///           DeviceProcessData: Actual Work State (DDI 0x0087)
///         ... (16 sections, elements 2-17)
///     DeviceValuePresentation: area (m2, scale=1.0, decimals=0)
DDOP build_sprayer_ddop() {
    DDOP ddop;

    // ── Value Presentation for area measurements ────────────────────────────
    auto area_vp_id = ddop.next_id();
    ddop.add_value_presentation(
        DeviceValuePresentation{}.set_id(area_vp_id).set_scale(1.0f).set_decimals(0).set_offset(0).set_unit("m2"));

    // ── Device Object ───────────────────────────────────────────────────────
    auto device_id = ddop.next_id();
    ddop.add_device(DeviceObject{}
                        .set_id(device_id)
                        .set_designator("SprayerECU")
                        .set_software_version("1.0.0")
                        .set_serial_number("SN12345"));

    // ── Device Element: Sprayer (Device type, element number 0) ─────────────
    auto sprayer_elem_id = ddop.next_id();
    ddop.add_element(DeviceElement{}
                         .set_id(sprayer_elem_id)
                         .set_type(DeviceElementType::Device)
                         .set_number(0)
                         .set_designator("Sprayer")
                         .set_parent(device_id));

    // ── Device Element: Boom (Function type, element number 1) ──────────────
    auto boom_elem_id = ddop.next_id();

    // Process data on the boom: Total Area
    auto total_area_pd_id = ddop.next_id();
    ddop.add_process_data(DeviceProcessData{}
                              .set_id(total_area_pd_id)
                              .set_ddi(ddi::TOTAL_AREA)
                              .add_trigger(TriggerMethod::Total)
                              .set_presentation(area_vp_id)
                              .set_designator("TotalArea"));

    // Process data on the boom: Effective Total Area
    auto eff_area_pd_id = ddop.next_id();
    ddop.add_process_data(DeviceProcessData{}
                              .set_id(eff_area_pd_id)
                              .set_ddi(ddi::EFFECTIVE_TOTAL_AREA)
                              .add_trigger(TriggerMethod::Total)
                              .set_presentation(area_vp_id)
                              .set_designator("EffectiveArea"));

    // Property on the boom: Working Width (24000 mm = 24m)
    auto width_prop_id = ddop.next_id();
    ddop.add_property(DeviceProperty{}
                          .set_id(width_prop_id)
                          .set_ddi(ddi::WORKING_WIDTH)
                          .set_value(24000)
                          .set_designator("WorkingWidth"));

    // Now build the boom element with children references
    DeviceElement boom_elem = DeviceElement{}
                                  .set_id(boom_elem_id)
                                  .set_type(DeviceElementType::Function)
                                  .set_number(1)
                                  .set_designator("Boom")
                                  .set_parent(sprayer_elem_id)
                                  .add_child(total_area_pd_id)
                                  .add_child(eff_area_pd_id)
                                  .add_child(width_prop_id);

    // ── 16 Section Elements (Section type, element numbers 2-17) ────────────
    dp::Vector<ObjectID> section_elem_ids;
    dp::Vector<ObjectID> section_pd_ids; // Track all section process data IDs

    for (u16 i = 0; i < SprayerState::NUM_SECTIONS; ++i) {
        // Process data: Setpoint Section Control State (DDI 0x0001)
        auto setpoint_pd_id = ddop.next_id();
        ddop.add_process_data(DeviceProcessData{}
                                  .set_id(setpoint_pd_id)
                                  .set_ddi(ddi::SETPOINT_SECTION_CONTROL_STATE)
                                  .add_trigger(TriggerMethod::TimeInterval)
                                  .add_trigger(TriggerMethod::OnChange)
                                  .set_designator("SectionCtrl" + dp::String(dp::to_string(i + 1).c_str())));

        // Process data: Actual Work State (DDI 0x0087)
        auto work_state_pd_id = ddop.next_id();
        ddop.add_process_data(DeviceProcessData{}
                                  .set_id(work_state_pd_id)
                                  .set_ddi(ddi::ACTUAL_WORK_STATE)
                                  .add_trigger(TriggerMethod::TimeInterval)
                                  .add_trigger(TriggerMethod::OnChange)
                                  .set_designator("WorkState" + dp::String(dp::to_string(i + 1).c_str())));

        // Section element
        auto section_id = ddop.next_id();
        ddop.add_element(DeviceElement{}
                             .set_id(section_id)
                             .set_type(DeviceElementType::Section)
                             .set_number(static_cast<u16>(2 + i))
                             .set_designator("Section " + dp::String(dp::to_string(i + 1).c_str()))
                             .set_parent(boom_elem_id)
                             .add_child(setpoint_pd_id)
                             .add_child(work_state_pd_id));

        section_elem_ids.push_back(section_id);
        boom_elem.add_child(section_id);
    }

    // Add the boom element (after section children are created)
    ddop.add_element(boom_elem);

    echo::info("DDOP built: ", ddop.object_count(), " objects");
    return ddop;
}

// ─── Map element number to section index ────────────────────────────────────────
/// Section elements have element numbers 2..17, mapping to section indices 0..15.
static i16 element_to_section_index(ElementNumber elem_num) {
    if (elem_num >= 2 && elem_num <= 17) {
        return static_cast<i16>(elem_num - 2);
    }
    return -1; // Not a section element
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
int main() {
    echo::box("=== TC Section Control Demo (16-Section Sprayer) ===");

    // ─── Network Setup ──────────────────────────────────────────────────────
    IsoNet nm;

    Name name = Name::build()
                    .set_identity_number(2001)
                    .set_manufacturer_code(64)
                    .set_function_code(130) // Sprayer function
                    .set_device_class(4)    // Sprayers
                    .set_industry_group(2)  // Agriculture
                    .set_self_configurable(true);

    auto cf_result = nm.create_internal(name, 0, 0x28);
    auto *cf = cf_result.value();

    echo::info("Internal CF created: address=0x28, function=Sprayer");

    // ─── Application State ──────────────────────────────────────────────────
    SprayerState sprayer;

    // ─── Build DDOP ─────────────────────────────────────────────────────────
    DDOP ddop = build_sprayer_ddop();

    // Validate the DDOP before use
    auto validation = ddop.validate();
    if (!validation.is_ok()) {
        echo::error("DDOP validation failed: ", validation.error().message);
        return 1;
    }
    echo::info("DDOP validation: OK");

    // Serialize to check binary size
    auto serialized = ddop.serialize();
    if (serialized.is_ok()) {
        echo::info("DDOP binary size: ", serialized.value().size(), " bytes");
    }

    // ─── Create TC Client ───────────────────────────────────────────────────
    TCClientConfig tc_config;
    tc_config.timeout(6000); // 6 second timeout for TC responses

    TaskControllerClient tc_client(nm, cf, tc_config);
    tc_client.set_ddop(std::move(ddop));

    // ─── Value Request Callback ─────────────────────────────────────────────
    // Called when the TC requests a value from our DDOP.
    tc_client.on_value_request([&](ElementNumber elem, DDI ddi) -> Result<i32> {
        echo::category("tc.callback")
            .debug("Value request: element=", elem, " DDI=0x", dp::to_string(static_cast<u32>(ddi)).c_str());

        // Section-level requests (element numbers 2-17)
        i16 section_idx = element_to_section_index(elem);
        if (section_idx >= 0) {
            switch (ddi) {
            case ddi::ACTUAL_WORK_STATE:
                // Return current work state for this section (0=off, 1=on)
                return Result<i32>::ok(sprayer.section_work_state(static_cast<u16>(section_idx)));

            case ddi::SETPOINT_SECTION_CONTROL_STATE:
                // Return the setpoint (same as actual for this demo)
                return Result<i32>::ok(sprayer.section_work_state(static_cast<u16>(section_idx)));

            default:
                break;
            }
        }

        // Boom-level requests (element number 1)
        if (elem == 1) {
            switch (ddi) {
            case ddi::TOTAL_AREA: // also EFFECTIVE_TOTAL_AREA (same DDI)
                return Result<i32>::ok(sprayer.total_area);

            default:
                break;
            }
        }

        echo::category("tc.callback").warn("Unknown value request: elem=", elem, " DDI=", ddi);
        return Result<i32>::err(Error::invalid_state("unhandled DDI"));
    });

    // ─── Value Command Callback ─────────────────────────────────────────────
    // Called when the TC sends a setpoint command to our DDOP.
    tc_client.on_value_command([&](ElementNumber elem, DDI ddi, i32 value) -> Result<void> {
        echo::category("tc.callback")
            .info("Value command: element=", elem, " DDI=0x", dp::to_string(static_cast<u32>(ddi)).c_str(),
                  " value=", value);

        // Section control commands (element numbers 2-17)
        i16 section_idx = element_to_section_index(elem);
        if (section_idx >= 0) {
            if (ddi == ddi::SETPOINT_SECTION_CONTROL_STATE) {
                // value: 0 = off, 1 = on, 2 = error, 3 = not available
                bool on = (value == 1);
                sprayer.set_section(static_cast<u16>(section_idx), on);
                return {};
            }
        }

        echo::category("tc.callback").warn("Unhandled command: elem=", elem, " DDI=", ddi, " value=", value);
        return Result<void>::err(Error::invalid_state("unhandled command DDI"));
    });

    // ─── State Change Event ─────────────────────────────────────────────────
    tc_client.on_state_change.subscribe(
        [](TCState new_state) { echo::info(">>> TC state changed: ", tc_state_name(new_state)); });

    // ─── Connect ────────────────────────────────────────────────────────────
    echo::info("");
    echo::info("--- Initiating TC connection ---");
    auto connect_result = tc_client.connect();
    if (!connect_result.is_ok()) {
        echo::error("TC connect failed: ", connect_result.error().message);
        return 1;
    }
    echo::info("TC connection initiated, state: ", tc_state_name(tc_client.state()));

    // ─── Run State Machine ──────────────────────────────────────────────────
    // Simulate the TC client state machine running for 20 iterations.
    // In a real application, this would be driven by CAN bus messages from the
    // Task Controller server. Here we just demonstrate the state machine stepping
    // and area accumulation.
    echo::info("");
    echo::info("--- Running TC state machine (20 iterations, 200ms each) ---");
    echo::info("");

    // Pre-activate some sections to demonstrate area accumulation
    sprayer.set_section(0, true);
    sprayer.set_section(1, true);
    sprayer.set_section(2, true);
    sprayer.set_section(3, true);
    sprayer.set_section(7, true);
    sprayer.set_section(8, true);
    sprayer.set_section(14, true);
    sprayer.set_section(15, true);

    echo::info("");
    u32 active_count = 0;
    for (u16 i = 0; i < SprayerState::NUM_SECTIONS; ++i) {
        if (sprayer.section_states[i])
            active_count++;
    }
    echo::info("Active sections: ", active_count, " / ", SprayerState::NUM_SECTIONS);
    echo::info("");

    for (u32 tick = 0; tick < 20; ++tick) {
        u32 dt_ms = 200;

        // Accumulate area for active sections
        sprayer.accumulate_area(dt_ms);

        // Update the TC client state machine
        tc_client.update(dt_ms);

        // Report status every 5 ticks
        if (tick % 5 == 0) {
            echo::info("[tick ", tick, "] state=", tc_state_name(tc_client.state()), " total_area=", sprayer.total_area,
                       " m2", " effective_area=", sprayer.effective_total_area, " m2");
        }
    }

    // ─── Final Summary ──────────────────────────────────────────────────────
    echo::info("");
    echo::info("=== Final State ===");
    echo::info("TC state: ", tc_state_name(tc_client.state()));
    echo::info("Total area: ", sprayer.total_area, " m2");
    echo::info("Effective total area: ", sprayer.effective_total_area, " m2");
    echo::info("");

    echo::info("Section states:");
    for (u16 i = 0; i < SprayerState::NUM_SECTIONS; ++i) {
        echo::info("  Section ", i + 1, ": ", sprayer.section_states[i] ? "ON " : "OFF",
                   "  area=", sprayer.area_counters[i], " m2");
    }

    echo::info("");
    echo::info("DDOP summary:");
    echo::info("  Device: SprayerECU v1.0.0 (SN12345)");
    echo::info("  Boom: 24m working width, ", SprayerState::NUM_SECTIONS, " sections");
    echo::info("  Section width: ", sprayer.working_width_mm / SprayerState::NUM_SECTIONS, " mm");
    echo::info("");
    echo::info("=== TC Section Control Demo Complete ===");

    return 0;
}
