#include <agrobus.hpp>
#include <echo/echo.hpp>
#include <cmath>

using namespace agrobus::net;
using namespace agrobus::j1939;
using namespace agrobus::isobus;
using namespace agrobus::nmea;
using namespace agrobus::isobus::vt;
using namespace agrobus::isobus::tc;
using namespace agrobus::isobus::sc;
using namespace agrobus::isobus::implement;
using namespace agrobus::isobus::fs;
namespace ddi = agrobus::isobus::tc::ddi;


struct SprayerState {
    u32 setpoint_rate = 200; // L/ha
    u32 actual_rate = 0;
    dp::Array<bool, 8> sections_active{};
    f64 total_area_m2 = 0.0;
    f64 speed_mps = 0.0;
    f64 working_width_m = 24.0; // 24m boom
};

int main() {
    echo::info("=== Sprayer Controller (TC + VT + GNSS) ===");

    // ─── Network Setup ─────────────────────────────────────────────────────
    IsoNet nm;
    Name name = Name::build()
        .set_identity_number(100)
        .set_manufacturer_code(42)
        .set_function_code(130) // Sprayer
        .set_device_class(4)   // Sprayers
        .set_industry_group(2); // Agriculture

    auto cf_result = nm.create_internal(name, 0, 0x28);
    auto* cf = cf_result.value();

    SprayerState state;

    // ─── GNSS Interface ────────────────────────────────────────────────────
    NMEAInterface nmea(nm, cf);
    nmea.initialize();

    dp::Geo field_origin{48.1234, 11.5678, 450.0};

    nmea.on_position.subscribe([&](const GNSSPosition& pos) {
        auto enu = pos.to_enu(field_origin);
        echo::category("isobus.nmea").debug("Local: E=", enu.east(), " N=", enu.north());

        if (pos.speed_mps.has_value()) {
            state.speed_mps = *pos.speed_mps;
        }
    });

    // ─── Speed/Distance Interface ──────────────────────────────────────────
    SpeedDistanceInterface sdi(nm, cf);
    sdi.initialize();

    sdi.on_speed_update.subscribe([&](const SpeedData& sd) {
        if (sd.ground_speed_mps.has_value()) {
            state.speed_mps = *sd.ground_speed_mps;
            echo::info("Ground speed: ", state.speed_mps * 3.6, " km/h");
        }
    });

    // ─── Task Controller Client ────────────────────────────────────────────
    DDOP ddop;

    ddop.add_device(DeviceObject{}
        .set_id(1)
        .set_designator("FieldSprayer")
        .set_software_version("2.0"));

    ddop.add_element(DeviceElement{}
        .set_id(2)
        .set_parent(1)
        .set_type(DeviceElementType::Device)
        .set_designator("Sprayer"));

    ddop.add_element(DeviceElement{}
        .set_id(3)
        .set_parent(2)
        .set_type(DeviceElementType::Function)
        .set_designator("Boom"));

    // Section elements
    for (u16 i = 0; i < 8; ++i) {
        ddop.add_element(DeviceElement{}
            .set_id(static_cast<u16>(10 + i))
            .set_parent(3)
            .set_type(DeviceElementType::Section)
            .set_designator("Section" + dp::String(std::to_string(i + 1).c_str())));
    }

    ddop.add_process_data(DeviceProcessData{}
        .set_id(50)
        .set_ddi(ddi::SETPOINT_VOLUME_PER_AREA)
        .set_triggers(static_cast<u8>(TriggerMethod::OnChange)));

    ddop.add_process_data(DeviceProcessData{}
        .set_id(51)
        .set_ddi(ddi::ACTUAL_WORK_STATE)
        .set_triggers(static_cast<u8>(TriggerMethod::OnChange)));

    TaskControllerClient tc(nm, cf);
    tc.set_ddop(std::move(ddop));

    // Value request callback
    tc.on_value_request([&](ElementNumber elem, DDI ddi_val) -> Result<i32> {
        if (ddi_val == ddi::ACTUAL_VOLUME_PER_AREA)
            return Result<i32>::ok(static_cast<i32>(state.actual_rate));
        if (ddi_val == ddi::SETPOINT_VOLUME_PER_AREA)
            return Result<i32>::ok(static_cast<i32>(state.setpoint_rate));
        if (ddi_val == ddi::ACTUAL_WORKING_WIDTH)
            return Result<i32>::ok(static_cast<i32>(state.working_width_m * 1000));
        if (ddi_val == ddi::TOTAL_AREA)
            return Result<i32>::ok(static_cast<i32>(state.total_area_m2));
        return Result<i32>::err(Error::invalid_pgn(static_cast<PGN>(ddi_val)));
    });

    // Value command callback (TC sending setpoints)
    tc.on_value_command([&](ElementNumber elem, DDI ddi_val, i32 value) -> Result<void> {
        if (ddi_val == ddi::SETPOINT_VOLUME_PER_AREA) {
            state.setpoint_rate = static_cast<u32>(value);
            echo::info("Rate setpoint changed: ", state.setpoint_rate, " L/ha");
            return {};
        }
        if (ddi_val == ddi::SETPOINT_WORK_STATE) {
            echo::info("Work state command: ", value);
            return {};
        }
        if (ddi_val == ddi::SECTION_CONTROL_STATE) {
            // Individual section on/off (bitfield)
            for (u8 i = 0; i < 8; ++i) {
                state.sections_active[i] = (value >> i) & 1;
            }
            echo::info("Section states updated");
            return {};
        }
        return Result<void>::err(Error::invalid_state("unknown DDI"));
    });

    tc.on_state_change.subscribe([](TCState s) {
        echo::info("TC state: ", static_cast<int>(s));
    });

    auto tc_result = tc.connect();
    if (tc_result.is_ok()) {
        echo::info("TC client started");
    }

    // ─── Virtual Terminal ──────────────────────────────────────────────────
    ObjectPool vt_pool;

    vt_pool.add(VTObject{}.set_id(0).set_type(ObjectType::WorkingSet)
        .set_body({static_cast<u8>(480 & 0xFF), static_cast<u8>((480 >> 8) & 0xFF),
                   static_cast<u8>(480 & 0xFF), static_cast<u8>((480 >> 8) & 0xFF)}));

    vt_pool.add(VTObject{}.set_id(100).set_type(ObjectType::DataMask)
        .set_body({static_cast<u8>(480 & 0xFF), static_cast<u8>((480 >> 8) & 0xFF),
                   static_cast<u8>(480 & 0xFF), static_cast<u8>((480 >> 8) & 0xFF)}));

    vt_pool.add(VTObject{}.set_id(200).set_type(ObjectType::OutputNumber)
        .set_body({static_cast<u8>(120 & 0xFF), static_cast<u8>((120 >> 8) & 0xFF),
                   static_cast<u8>(40 & 0xFF), static_cast<u8>((40 >> 8) & 0xFF)}));

    vt_pool.add(VTObject{}.set_id(201).set_type(ObjectType::OutputNumber)
        .set_body({static_cast<u8>(120 & 0xFF), static_cast<u8>((120 >> 8) & 0xFF),
                   static_cast<u8>(40 & 0xFF), static_cast<u8>((40 >> 8) & 0xFF)}));

    VTClient vt(nm, cf);
    vt.set_object_pool(std::move(vt_pool));

    vt.on_state_change.subscribe([&](VTState s) {
        if (s == VTState::Connected) {
            echo::info("VT connected, updating display");
            vt.change_numeric_value(200, state.setpoint_rate);
        }
    });

    vt.connect();

    // ─── Main Simulation Loop ──────────────────────────────────────────────
    echo::info("\nRunning simulation (10 iterations)...\n");

    for (u32 tick = 0; tick < 10; ++tick) {
        u32 dt_ms = 100;

        // Simulate speed
        state.speed_mps = 2.5 + 0.1 * tick;

        // Calculate actual rate based on speed and section state
        u32 active_sections = 0;
        for (auto s : state.sections_active) {
            if (s) active_sections++;
        }

        if (state.speed_mps > 0.1 && active_sections > 0) {
            // Flow rate = speed * width_fraction * target_rate
            f64 width_fraction = static_cast<f64>(active_sections) / 8.0;
            state.actual_rate = static_cast<u32>(state.setpoint_rate * width_fraction);
            state.total_area_m2 += state.speed_mps * state.working_width_m * width_fraction * (dt_ms / 1000.0);
        }

        echo::info("[", tick, "] speed=", state.speed_mps * 3.6, " km/h",
                   " rate=", state.actual_rate, "/", state.setpoint_rate, " L/ha",
                   " area=", state.total_area_m2 / 10000.0, " ha");

        // Update protocols
        tc.update(dt_ms);
        vt.update(dt_ms);
    }

    echo::info("\n=== Summary ===");
    echo::info("Total area: ", state.total_area_m2 / 10000.0, " ha");
    echo::info("Final rate: ", state.actual_rate, " L/ha");

    return 0;
}
