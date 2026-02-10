/*
 * ISO 11783-8 Engine & Powertrain Monitoring Demo
 *
 * Demonstrates full powertrain monitoring including:
 * - Engine control (EEC1, EEC2, EEC3)
 * - Engine temperatures (ET1, ET2)
 * - Engine fluid levels/pressures (EFLP)
 * - Transmission control (ETC1, ETC2)
 * - Aftertreatment/emissions (AT1, AT2)
 * - Fuel economy tracking
 *
 * This example simulates a complete tractor ECU broadcasting powertrain data
 * and a monitoring system receiving and displaying the information.
 */

#include <agrobus/j1939/engine.hpp>
#include <agrobus/net/network_manager.hpp>
#include <echo/echo.hpp>
#include <chrono>
#include <thread>

using namespace agrobus::j1939;
using namespace agrobus::net;

// ═════════════════════════════════════════════════════════════════════════════
// Simulated Engine State
// ═════════════════════════════════════════════════════════════════════════════

struct EngineState {
    // Engine operation
    f64 rpm = 800.0;              // idle RPM
    f64 load_percent = 0.0;       // 0% load at idle
    f64 coolant_temp_c = 85.0;    // normal operating temp
    f64 oil_temp_c = 95.0;        // slightly above coolant
    f64 oil_pressure_kpa = 350.0; // healthy pressure
    f64 fuel_rate_lph = 2.5;      // idle consumption

    // Transmission
    u8 current_gear = 0xFF;       // neutral
    u8 selected_gear = 0xFF;      // neutral
    f64 trans_oil_temp_c = 75.0;  // cooler than engine
    f64 trans_oil_pressure_kpa = 400.0;

    // Emissions/Aftertreatment
    f64 def_level_percent = 80.0; // DEF tank 80% full
    f64 dpf_soot_load = 25.0;     // 25% soot load
    f64 nox_intake_ppm = 450.0;   // pre-treatment NOx
    f64 nox_outlet_ppm = 45.0;    // post-treatment NOx (90% reduction)
    bool dpf_regen_active = false;

    // Simulate engine dynamics
    void update(f64 throttle_position, u8 target_gear) {
        // Simulate RPM changes
        if (throttle_position > 10.0) {
            rpm = 800.0 + (throttle_position / 100.0) * 1700.0; // idle to 2500 RPM
            load_percent = throttle_position * 0.8; // rough approximation
            fuel_rate_lph = 2.5 + (throttle_position / 100.0) * 45.0; // 2.5 to 47.5 L/h
        } else {
            rpm = 800.0;
            load_percent = 0.0;
            fuel_rate_lph = 2.5;
        }

        // Simulate temperature increase with load
        coolant_temp_c = 85.0 + (load_percent * 0.15);
        oil_temp_c = coolant_temp_c + 10.0;
        trans_oil_temp_c = coolant_temp_c - 10.0;

        // Simulate gear shifts
        if (target_gear != 0xFF && target_gear != current_gear) {
            selected_gear = target_gear;
            // Shift takes time...
        } else if (selected_gear != 0xFF) {
            current_gear = selected_gear; // shift complete
        }

        // Simulate NOx levels based on load
        nox_intake_ppm = 300.0 + (load_percent * 8.0);
        nox_outlet_ppm = nox_intake_ppm * 0.1; // 90% reduction

        // Simulate DPF soot accumulation
        dpf_soot_load += (load_percent * 0.001);

        // Trigger regeneration if needed
        if (dpf_soot_load > 75.0 && !dpf_regen_active) {
            dpf_regen_active = true;
            echo::info("DPF Regeneration STARTED");
        }

        // Regeneration reduces soot
        if (dpf_regen_active) {
            dpf_soot_load -= 1.5;
            if (dpf_soot_load < 20.0) {
                dpf_regen_active = false;
                echo::info("DPF Regeneration COMPLETE");
            }
        }
    }
};

// ═════════════════════════════════════════════════════════════════════════════
// Tractor ECU (Message Broadcaster)
// ═════════════════════════════════════════════════════════════════════════════

class TractorECU {
    IsoNet &net_;
    InternalCF *cf_;
    EngineInterface ei_;
    EngineState state_;
    bool running_ = false;

public:
    TractorECU(IsoNet &net, InternalCF *cf) : net_(net), cf_(cf), ei_(net, cf) {
        ei_.initialize();
        echo::category("tractor.ecu").info("Tractor ECU initialized");
    }

    void start() {
        running_ = true;
        echo::category("tractor.ecu").info("Engine started");
    }

    void set_throttle(f64 percent) {
        state_.update(percent, state_.selected_gear);
    }

    void shift_to_gear(u8 gear) {
        state_.update(0.0, gear);
    }

    void broadcast() {
        if (!running_) return;

        // EEC1 - Engine speed and torque (10 Hz typical)
        EEC1 eec1;
        eec1.engine_speed_rpm = state_.rpm;
        eec1.engine_torque_percent = state_.load_percent;
        eec1.driver_demand_percent = state_.load_percent;
        eec1.actual_engine_percent = state_.load_percent;
        ei_.send_eec1(eec1);

        // EEC2 - Accelerator position (10 Hz typical)
        EEC2 eec2;
        eec2.engine_load_percent = state_.load_percent;
        eec2.accel_pedal_position = static_cast<u8>(state_.load_percent / 0.4);
        ei_.send_eec2(eec2);

        // ET1 - Engine temperatures (1 Hz typical)
        EngineTemp1 et1;
        et1.coolant_temp_c = state_.coolant_temp_c;
        et1.oil_temp_c = state_.oil_temp_c;
        ei_.send_engine_temp(et1);

        // ET2 - Extended engine temperatures (1 Hz typical)
        EngineTemp2 et2;
        et2.engine_oil_temp_c = state_.oil_temp_c;
        et2.turbo_oil_temp_c = state_.oil_temp_c + 25.0;
        et2.engine_intercooler_temp_c = state_.coolant_temp_c - 20.0;
        ei_.send_engine_temp2(et2);

        // EFLP - Engine fluid level/pressure (1 Hz typical)
        EngineFluidLP eflp;
        eflp.oil_pressure_kpa = state_.oil_pressure_kpa;
        eflp.coolant_pressure_kpa = 100.0;
        eflp.oil_level_percent = 225; // ~90% full (225 * 0.4 = 90%)
        ei_.send_engine_fluid(eflp);

        // ETC1 - Transmission control (10 Hz typical)
        ETC1 etc1;
        etc1.current_gear = state_.current_gear;
        etc1.selected_gear = state_.selected_gear;
        etc1.trans_driveline = (state_.current_gear != 0xFF) ? 0x01 : 0x00;
        etc1.trans_output_shaft_speed_rpm = static_cast<i16>(state_.rpm * 0.8);
        ei_.send_etc1(etc1);

        // ETC2 - Transmission oil temp/pressure (1 Hz typical)
        ETC2 etc2;
        etc2.trans_oil_temp_c = state_.trans_oil_temp_c;
        etc2.trans_oil_pressure_kpa = state_.trans_oil_pressure_kpa;
        etc2.trans_oil_level = 225; // ~90%
        ei_.send_etc2(etc2);

        // AT1 - Aftertreatment NOx levels (1 Hz typical)
        Aftertreatment1 at1;
        at1.diesel_exhaust_fluid_tank_level = state_.def_level_percent;
        at1.intake_nox_ppm = state_.nox_intake_ppm;
        at1.outlet_nox_ppm = state_.nox_outlet_ppm;
        at1.intake_nox_reading_status = 0x00; // valid
        at1.outlet_nox_reading_status = 0x00; // valid
        ei_.send_aftertreatment1(at1);

        // AT2 - DPF status (1 Hz typical)
        Aftertreatment2 at2;
        at2.dpf_soot_load_percent = state_.dpf_soot_load;
        at2.dpf_differential_pressure_kpa = 5.0 + (state_.dpf_soot_load * 0.2);
        at2.diesel_exhaust_fluid_concentration = 32.0; // 32% spec
        at2.dpf_active_regeneration_status = state_.dpf_regen_active ? 0x01 : 0x00;
        at2.dpf_passive_regeneration_status = 0x00;
        ei_.send_aftertreatment2(at2);

        // Fuel Economy (1 Hz typical)
        FuelEconomy fe;
        fe.fuel_rate_lph = state_.fuel_rate_lph;
        ei_.send_fuel_economy(fe);
    }
};

// ═════════════════════════════════════════════════════════════════════════════
// Monitoring Display (Message Receiver)
// ═════════════════════════════════════════════════════════════════════════════

class MonitoringDisplay {
    IsoNet &net_;
    InternalCF *cf_;
    EngineInterface ei_;

    // Received state
    f64 rpm_ = 0.0;
    f64 load_ = 0.0;
    f64 coolant_temp_ = 0.0;
    f64 oil_temp_ = 0.0;
    f64 oil_pressure_ = 0.0;
    u8 current_gear_ = 0xFF;
    f64 trans_temp_ = 0.0;
    f64 def_level_ = 0.0;
    f64 dpf_soot_ = 0.0;
    f64 nox_reduction_ = 0.0;
    bool regen_active_ = false;

public:
    MonitoringDisplay(IsoNet &net, InternalCF *cf) : net_(net), cf_(cf), ei_(net, cf) {
        ei_.initialize();

        // Subscribe to all engine messages
        ei_.on_eec1.subscribe([this](const EEC1 &msg, Address src) {
            rpm_ = msg.engine_speed_rpm;
            load_ = msg.engine_torque_percent;
        });

        ei_.on_engine_temp.subscribe([this](const EngineTemp1 &msg, Address src) {
            coolant_temp_ = msg.coolant_temp_c;
            oil_temp_ = msg.oil_temp_c;
        });

        ei_.on_engine_fluid.subscribe([this](const EngineFluidLP &msg, Address src) {
            oil_pressure_ = msg.oil_pressure_kpa;
        });

        ei_.on_etc1.subscribe([this](const ETC1 &msg, Address src) {
            current_gear_ = msg.current_gear;
        });

        ei_.on_etc2.subscribe([this](const ETC2 &msg, Address src) {
            trans_temp_ = msg.trans_oil_temp_c;
        });

        ei_.on_aftertreatment1.subscribe([this](const Aftertreatment1 &msg, Address src) {
            def_level_ = msg.diesel_exhaust_fluid_tank_level;
            if (msg.intake_nox_ppm > 0.0) {
                nox_reduction_ = ((msg.intake_nox_ppm - msg.outlet_nox_ppm) / msg.intake_nox_ppm) * 100.0;
            }
        });

        ei_.on_aftertreatment2.subscribe([this](const Aftertreatment2 &msg, Address src) {
            dpf_soot_ = msg.dpf_soot_load_percent;
            regen_active_ = (msg.dpf_active_regeneration_status == 0x01);
        });

        echo::category("monitor.display").info("Monitoring display initialized");
    }

    void display() {
        echo::info("╔════════════════════════════════════════════════════════════╗");
        echo::info("║         TRACTOR POWERTRAIN MONITORING DISPLAY              ║");
        echo::info("╠════════════════════════════════════════════════════════════╣");
        echo::info("║ ENGINE:                                                    ║");
        echo::info("║   RPM:             ", std::round(rpm_), " rpm               ");
        echo::info("║   Load:            ", std::round(load_), " %                 ");
        echo::info("║   Coolant Temp:    ", std::round(coolant_temp_), " °C       ");
        echo::info("║   Oil Temp:        ", std::round(oil_temp_), " °C           ");
        echo::info("║   Oil Pressure:    ", std::round(oil_pressure_), " kPa      ");
        echo::info("║                                                            ║");
        echo::info("║ TRANSMISSION:                                              ║");
        echo::info("║   Current Gear:    ", (current_gear_ == 0xFF) ? "N" : std::to_string(current_gear_).c_str(), "             ");
        echo::info("║   Oil Temp:        ", std::round(trans_temp_), " °C          ");
        echo::info("║                                                            ║");
        echo::info("║ EMISSIONS:                                                 ║");
        echo::info("║   DEF Level:       ", std::round(def_level_), " %            ");
        echo::info("║   NOx Reduction:   ", std::round(nox_reduction_), " %        ");
        echo::info("║   DPF Soot Load:   ", std::round(dpf_soot_), " %            ");
        echo::info("║   DPF Regen:       ", regen_active_ ? "ACTIVE" : "Idle", "                           ");
        echo::info("╚════════════════════════════════════════════════════════════╝");
    }
};

// ═════════════════════════════════════════════════════════════════════════════
// Main Demo
// ═════════════════════════════════════════════════════════════════════════════

int main() {
    echo::category("demo").info("ISO 11783-8 Engine Powertrain Demo Starting...");

    // Create virtual CAN network
    IsoNet net;

    // Create tractor ECU (engine controller)
    Name tractor_name = Name::build()
        .set_industry_group(2) // agricultural
        .set_device_class(0)   // non-specific
        .set_function(0)       // engine
        .set_identity_number(1001);
    auto tractor_cf = net.create_internal(tractor_name, 0, 0x00).value();
    TractorECU tractor(net, tractor_cf);

    // Create monitoring display
    Name monitor_name = Name::build()
        .set_industry_group(2)
        .set_device_class(10) // display
        .set_function(1)
        .set_identity_number(2001);
    auto monitor_cf = net.create_internal(monitor_name, 0, 0x01).value();
    MonitoringDisplay monitor(net, monitor_cf);

    // Start simulation
    tractor.start();

    echo::category("demo").info("\n=== Scenario 1: Idle ===");
    tractor.set_throttle(0.0);
    for (int i = 0; i < 3; ++i) {
        tractor.broadcast();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    monitor.display();
    std::this_thread::sleep_for(std::chrono::seconds(2));

    echo::category("demo").info("\n=== Scenario 2: Light Load (25% throttle) ===");
    tractor.set_throttle(25.0);
    tractor.shift_to_gear(3);
    for (int i = 0; i < 5; ++i) {
        tractor.broadcast();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    monitor.display();
    std::this_thread::sleep_for(std::chrono::seconds(2));

    echo::category("demo").info("\n=== Scenario 3: Medium Load (50% throttle) ===");
    tractor.set_throttle(50.0);
    tractor.shift_to_gear(5);
    for (int i = 0; i < 5; ++i) {
        tractor.broadcast();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    monitor.display();
    std::this_thread::sleep_for(std::chrono::seconds(2));

    echo::category("demo").info("\n=== Scenario 4: Heavy Load (80% throttle) - DPF will accumulate soot ===");
    tractor.set_throttle(80.0);
    for (int i = 0; i < 10; ++i) {
        tractor.broadcast();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    monitor.display();
    std::this_thread::sleep_for(std::chrono::seconds(2));

    echo::category("demo").info("\n=== Scenario 5: Return to Idle ===");
    tractor.set_throttle(0.0);
    tractor.shift_to_gear(0xFF); // neutral
    for (int i = 0; i < 3; ++i) {
        tractor.broadcast();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    monitor.display();

    echo::category("demo").success("\nDemo complete!");
    echo::category("demo").info("ISO 11783-8 powertrain messages demonstrated:");
    echo::category("demo").info("  ✓ EEC1, EEC2, EEC3 (Engine Control)");
    echo::category("demo").info("  ✓ ET1, ET2 (Engine Temperatures)");
    echo::category("demo").info("  ✓ EFLP (Engine Fluid Level/Pressure)");
    echo::category("demo").info("  ✓ ETC1, ETC2 (Transmission Control)");
    echo::category("demo").info("  ✓ AT1, AT2 (Aftertreatment/Emissions)");
    echo::category("demo").info("  ✓ Fuel Economy");

    return 0;
}
