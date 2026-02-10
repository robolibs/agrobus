#pragma once

#include <agrobus/isobus/implement/tractor_facilities.hpp>
#include <agrobus/isobus/tim.hpp>
#include <agrobus/net/constants.hpp>
#include <agrobus/net/control_function.hpp>
#include <agrobus/net/error.hpp>
#include <agrobus/net/event.hpp>
#include <agrobus/net/internal_cf.hpp>
#include <agrobus/net/message.hpp>
#include <agrobus/net/network_manager.hpp>
#include <agrobus/net/state_machine.hpp>
#include <agrobus/net/working_set.hpp>
#include <datapod/datapod.hpp>
#include <echo/echo.hpp>

namespace agrobus::isobus {
    using namespace agrobus::net;
    using namespace agrobus::isobus::implement;

    // ─── Power State (ISO 11783-9 Section 4.6) ──────────────────────────────────
    enum class PowerState {
        PowerOff,          // All power off
        IgnitionOn,        // Normal operation
        ShutdownInitiated, // Key off, 3 min max power available
        FinalShutdown      // Power down complete
    };

    // ─── TECU Classification (ISO 11783-9 Section 4.4.2) ────────────────────────
    struct TECUClassification {
        TECUClass base_class = TECUClass::Class1;

        // Addenda
        bool navigation = false;    // N — GPS position
        bool guidance = false;      // G — Guidance/steering (v2+)
        bool front_mounted = false; // F — Front hitch/PTO
        bool powertrain = false;    // P — Powertrain control (v2+)
        bool motion_init = false;   // M — Motion initiation (v2+)

        // Version
        u8 version = 1;  // 1 or 2
        u8 instance = 0; // 0 = primary, 1+ = secondary

        // Convert to string (e.g., "Class 2NF", "Class 3GP")
        dp::String to_string() const {
            dp::String result = "Class ";
            result += dp::to_string(static_cast<u8>(base_class));
            if (navigation)
                result += "N";
            if (front_mounted)
                result += "F";
            if (guidance)
                result += "G";
            if (powertrain)
                result += "P";
            if (motion_init)
                result += "M";
            return result;
        }
    };

    // ─── Power Configuration ─────────────────────────────────────────────────────
    struct PowerConfig {
        u32 shutdown_max_time_ms = 180000; // 3 minutes max power after key off
        u32 maintain_timeout_ms = 2000;    // 2 seconds minimum hold for maintain power
        u8 ecu_pwr_current_amps = 15;      // ECU_PWR minimum: 15A
        u8 pwr_current_amps = 50;          // PWR minimum: 50A

        PowerConfig &shutdown_time(u32 ms) {
            shutdown_max_time_ms = ms;
            return *this;
        }
        PowerConfig &maintain_timeout(u32 ms) {
            maintain_timeout_ms = ms;
            return *this;
        }
        PowerConfig &ecu_power(u8 amps) {
            ecu_pwr_current_amps = amps;
            return *this;
        }
        PowerConfig &power(u8 amps) {
            pwr_current_amps = amps;
            return *this;
        }
    };

    // ─── TECU Configuration ──────────────────────────────────────────────────────
    struct TECUConfig {
        TECUClassification classification;
        PowerConfig power;
        u32 facilities_broadcast_interval_ms = 2000; // Broadcast facilities every 2s
        u32 status_broadcast_interval_ms = 100;      // Status messages every 100ms
        bool enable_gateway = false;                 // Enable message gateway between ports

        TECUConfig &set_classification(TECUClassification c) {
            classification = c;
            return *this;
        }
        TECUConfig &set_power(PowerConfig p) {
            power = p;
            return *this;
        }
        TECUConfig &broadcast_interval(u32 ms) {
            facilities_broadcast_interval_ms = ms;
            return *this;
        }
        TECUConfig &status_interval(u32 ms) {
            status_broadcast_interval_ms = ms;
            return *this;
        }
        TECUConfig &gateway(bool enable) {
            enable_gateway = enable;
            return *this;
        }
    };

    // ─── Safe Mode Trigger ───────────────────────────────────────────────────────
    enum class SafeModeTrigger { None, PowerLoss, ECUPowerLoss, CANBusFail, TECUCommLoss, ManualTrigger };

    // ─── Maintain Power Request ─────────────────────────────────────────────────
    struct MaintainPowerRequest {
        Address requester;
        bool ecu_pwr = false;
        bool pwr = false;
        u32 timestamp_ms = 0;

        bool is_expired(u32 current_time_ms, u32 timeout_ms) const {
            return (current_time_ms - timestamp_ms) > timeout_ms;
        }
    };

    // ─── Tractor ECU (ISO 11783-9) ───────────────────────────────────────────────
    // Gateway between tractor-bus and implement-bus with TECU classification support
    class TractorECU {
        IsoNet &net_;
        InternalCF *cf_;
        TECUConfig config_;

        // State machines
        StateMachine<PowerState> power_state_{PowerState::PowerOff};
        SafeModeTrigger safe_mode_trigger_ = SafeModeTrigger::None;

        // Timers
        u32 facilities_timer_ms_ = 0;
        u32 status_timer_ms_ = 0;
        u32 shutdown_timer_ms_ = 0;
        u32 last_maintain_request_ms_ = 0;

        // Power management
        dp::Vector<MaintainPowerRequest> maintain_power_requests_;
        bool key_switch_on_ = false;
        bool ecu_pwr_enabled_ = false;
        bool pwr_enabled_ = false;

        // Working set manager
        dp::Optional<WorkingSetManager> working_set_manager_;

        // TIM (Tractor Implement Management)
        dp::Optional<TimServer> tim_server_;

        // Facilities
        TractorFacilitiesInterface facilities_interface_;
        TractorFacilities supported_facilities_;
        dp::Optional<TractorFacilities> primary_tecu_facilities_; // For secondary TECUs

        // Gateway message tracking (if enabled)
        dp::Vector<Message> gateway_queue_;

      public:
        TractorECU(IsoNet &net, InternalCF *cf, TECUConfig config = {})
            : net_(net), cf_(cf), config_(config), facilities_interface_(net, cf) {

            // Initialize supported facilities based on classification
            update_supported_facilities();
        }

        // ─── Initialization ──────────────────────────────────────────────────────
        Result<void> initialize() {
            if (!cf_) {
                return Result<void>::err(Error::invalid_state("control function not set"));
            }

            // Initialize facilities interface
            auto facilities_result = facilities_interface_.initialize();
            if (facilities_result.is_err()) {
                return facilities_result;
            }

            // Subscribe to facilities requests
            facilities_interface_.on_facilities_required.subscribe(
                [this](const TractorFacilities &required) { handle_facilities_request(required); });

            // Subscribe to facilities responses (for secondary TECUs to learn about primary)
            facilities_interface_.on_facilities_response.subscribe(
                [this](const TractorFacilities &response) { handle_facilities_response(response); });

            // Initialize working set manager
            working_set_manager_.emplace(net_, cf_);
            auto ws_result = working_set_manager_->initialize();
            if (ws_result.is_err()) {
                return ws_result;
            }

            echo::category("isobus.tecu").info("Tractor ECU initialized: ", config_.classification.to_string());

            return {};
        }

        // ─── Power Control ───────────────────────────────────────────────────────
        Result<void> set_key_switch(bool on) {
            key_switch_on_ = on;

            if (on) {
                power_state_.transition(PowerState::IgnitionOn);
                ecu_pwr_enabled_ = true;
                pwr_enabled_ = true;
                shutdown_timer_ms_ = 0;
                echo::category("isobus.tecu.power").info("Ignition ON: ECU_PWR and PWR enabled");
                on_power_state_changed.emit(PowerState::IgnitionOn);
            } else {
                power_state_.transition(PowerState::ShutdownInitiated);
                shutdown_timer_ms_ = 0;
                echo::category("isobus.tecu.power")
                    .info("Key OFF: Shutdown initiated, max ", config_.power.shutdown_max_time_ms / 1000,
                          "s available");
                on_power_state_changed.emit(PowerState::ShutdownInitiated);
            }

            return {};
        }

        bool get_key_switch() const { return key_switch_on_; }
        PowerState get_power_state() const { return power_state_.state(); }
        bool get_ecu_pwr_enabled() const { return ecu_pwr_enabled_; }
        bool get_pwr_enabled() const { return pwr_enabled_; }

        // ─── Maintain Power Requests (from CFs during shutdown) ─────────────────
        Result<void> receive_maintain_power_request(Address requester, bool ecu_pwr, bool pwr, u32 current_time_ms) {
            // Find existing request from this CF
            for (auto &req : maintain_power_requests_) {
                if (req.requester == requester) {
                    req.ecu_pwr = ecu_pwr;
                    req.pwr = pwr;
                    req.timestamp_ms = current_time_ms;
                    echo::category("isobus.tecu.power")
                        .debug("Maintain power request updated from 0x", requester, " ECU_PWR=", ecu_pwr, " PWR=", pwr);
                    return {};
                }
            }

            // New request
            maintain_power_requests_.push_back({requester, ecu_pwr, pwr, current_time_ms});
            last_maintain_request_ms_ = current_time_ms;
            echo::category("isobus.tecu.power")
                .debug("New maintain power request from 0x", requester, " ECU_PWR=", ecu_pwr, " PWR=", pwr);

            return {};
        }

        // ─── Safe Mode Control ───────────────────────────────────────────────────
        Result<void> trigger_safe_mode(SafeModeTrigger trigger) {
            safe_mode_trigger_ = trigger;

            echo::category("isobus.tecu.safety").warn("Safe mode triggered: ", static_cast<u8>(trigger));

            // Execute failsafe actions (ISO 11783-9 Section 4.6.5)
            // R1: No unexpected starting
            // R2: Stop command always works
            // R3: No parts fall or eject
            // R4: Manual/auto stop unimpeded
            // R5: Protection devices stay active
            // R6: Auto-stop on detectable failure
            // R7: Operator can always override

            if (tim_server_.has_value()) {
                // Disengage PTOs
                tim_server_->set_front_pto(false, true, 0);
                tim_server_->set_rear_pto(false, true, 0);
                echo::category("isobus.tecu.safety").info("PTOs disengaged");

                // Set hitch to neutral/safe position
                tim_server_->set_front_hitch(false, 0);
                tim_server_->set_rear_hitch(false, 0);
                echo::category("isobus.tecu.safety").info("Hitches set to neutral");

                // Set all aux valves to safe position (block)
                for (u8 i = 0; i < 32; ++i) { // MAX_AUX_VALVES from tim.hpp
                    auto valve = tim_server_->get_aux_valve(i);
                    if (valve.state_supported) {
                        tim_server_->set_aux_valve(i, false, 0);
                    }
                }
                echo::category("isobus.tecu.safety").info("All aux valves blocked");
            }

            // TODO: Report fault via DM1
            // This would require diagnostic module integration

            on_safe_mode_triggered.emit(trigger);

            return {};
        }

        SafeModeTrigger get_safe_mode_trigger() const { return safe_mode_trigger_; }

        Result<void> clear_safe_mode() {
            safe_mode_trigger_ = SafeModeTrigger::None;
            echo::category("isobus.tecu.safety").info("Safe mode cleared");
            on_safe_mode_cleared.emit();
            return {};
        }

        // ─── TIM Server Integration ─────────────────────────────────────────────
        Result<void> attach_tim_server(TimServer &&tim) {
            tim_server_.emplace(std::move(tim));
            echo::category("isobus.tecu").info("TIM server attached");
            return {};
        }

        TimServer *get_tim_server() { return tim_server_.has_value() ? &(*tim_server_) : nullptr; }

        // ─── Classification and Facilities ───────────────────────────────────────
        TECUClassification get_classification() const { return config_.classification; }

        Result<void> set_classification(TECUClassification classification) {
            config_.classification = classification;
            update_supported_facilities();
            echo::category("isobus.tecu").info("Classification updated: ", classification.to_string());
            return {};
        }

        TractorFacilities get_supported_facilities() const { return supported_facilities_; }

        // ─── Working Set Management ──────────────────────────────────────────────
        WorkingSetManager *get_working_set_manager() {
            return working_set_manager_.has_value() ? &(*working_set_manager_) : nullptr;
        }

        // Add a member to the working set
        Result<void> add_working_set_member(Name member_name) {
            if (!working_set_manager_.has_value()) {
                return Result<void>::err(Error::invalid_state("working set not initialized"));
            }
            return working_set_manager_->add_member(member_name);
        }

        // Broadcast working set announcement
        Result<void> broadcast_working_set() {
            if (!working_set_manager_.has_value()) {
                return Result<void>::err(Error::invalid_state("working set not initialized"));
            }
            return working_set_manager_->broadcast_working_set();
        }

        // ─── Events ──────────────────────────────────────────────────────────────
        Event<PowerState> on_power_state_changed;
        Event<SafeModeTrigger> on_safe_mode_triggered;
        Event<> on_safe_mode_cleared;
        Event<const TractorFacilities &> on_facilities_request_received;
        Event<> on_shutdown_complete;

        // ─── Update Loop ─────────────────────────────────────────────────────────
        void update(u32 elapsed_ms) {
            // Update facilities broadcast timer
            facilities_timer_ms_ += elapsed_ms;
            if (facilities_timer_ms_ >= config_.facilities_broadcast_interval_ms) {
                facilities_timer_ms_ = 0;
                send_facilities_broadcast();
            }

            // Update status broadcast timer
            status_timer_ms_ += elapsed_ms;
            if (status_timer_ms_ >= config_.status_broadcast_interval_ms) {
                status_timer_ms_ = 0;
                send_status_messages();
            }

            // Update power management state machine
            update_power_management(elapsed_ms);

            // Update TIM server if attached
            if (tim_server_.has_value()) {
                tim_server_->update(elapsed_ms);
            }

            // Update working set manager if active
            if (working_set_manager_.has_value()) {
                working_set_manager_->update(elapsed_ms);
            }
        }

      private:
        // ─── Update supported facilities based on classification ────────────────
        void update_supported_facilities() {
            supported_facilities_ = TractorFacilities{};

            // Class 1: Basic
            if (config_.classification.base_class >= TECUClass::Class1) {
                supported_facilities_.rear_hitch_position = true;
                supported_facilities_.rear_hitch_in_work = true;
                supported_facilities_.rear_pto_speed = true;
                supported_facilities_.rear_pto_engagement = true;
                supported_facilities_.wheel_based_speed = true;
                supported_facilities_.ground_based_speed = true;
            }

            // Class 2: Full measurements
            if (config_.classification.base_class >= TECUClass::Class2) {
                supported_facilities_.ground_based_distance = true;
                supported_facilities_.ground_based_direction = true;
                supported_facilities_.wheel_based_distance = true;
                supported_facilities_.wheel_based_direction = true;
                supported_facilities_.rear_draft = true;
                supported_facilities_.lighting = true;
                supported_facilities_.aux_valve_flow = true;
            }

            // Class 3: Control
            if (config_.classification.base_class >= TECUClass::Class3) {
                supported_facilities_.rear_hitch_command = true;
                supported_facilities_.rear_pto_command = true;
                supported_facilities_.aux_valve_command = true;

                // v2 additions
                if (config_.classification.version >= 2) {
                    supported_facilities_.rear_hitch_limit_status = true;
                    supported_facilities_.rear_hitch_exit_code = true;
                    supported_facilities_.rear_pto_engagement_request = true;
                    supported_facilities_.rear_pto_speed_limit_status = true;
                    supported_facilities_.rear_pto_exit_code = true;
                    supported_facilities_.aux_valve_limit_status = true;
                    supported_facilities_.aux_valve_exit_code = true;
                }
            }

            // Addenda
            if (config_.classification.navigation) {
                supported_facilities_.navigation = true;
            }

            if (config_.classification.guidance) {
                supported_facilities_.guidance = true;
            }

            if (config_.classification.front_mounted) {
                supported_facilities_.front_hitch_position = true;
                supported_facilities_.front_hitch_in_work = true;
                supported_facilities_.front_pto_speed = true;
                supported_facilities_.front_pto_engagement = true;

                if (config_.classification.base_class >= TECUClass::Class3) {
                    supported_facilities_.front_hitch_command = true;
                    supported_facilities_.front_pto_command = true;

                    if (config_.classification.version >= 2) {
                        supported_facilities_.front_hitch_limit_status = true;
                        supported_facilities_.front_hitch_exit_code = true;
                        supported_facilities_.front_pto_engagement_request = true;
                        supported_facilities_.front_pto_speed_limit_status = true;
                        supported_facilities_.front_pto_exit_code = true;
                    }
                }
            }

            if (config_.classification.powertrain) {
                supported_facilities_.machine_selected_speed = true;
                if (config_.classification.base_class >= TECUClass::Class3) {
                    supported_facilities_.machine_selected_speed_command = true;
                }
            }
        }

        // ─── Send facilities broadcast ───────────────────────────────────────────
        void send_facilities_broadcast() {
            if (power_state_.state() != PowerState::IgnitionOn) {
                return; // Don't broadcast when not in normal operation
            }

            // Secondary TECUs only broadcast after learning about primary's facilities
            if (is_secondary() && !primary_tecu_facilities_.has_value()) {
                echo::category("isobus.tecu").trace("Secondary TECU waiting for primary facilities before broadcast");
                return;
            }

            auto facilities_to_send = get_effective_facilities();
            auto result = facilities_interface_.send_facilities_response(facilities_to_send);
            if (result.is_err()) {
                echo::category("isobus.tecu").error("Failed to send facilities broadcast");
            } else {
                echo::category("isobus.tecu").trace("Facilities broadcast sent");
            }
        }

        // ─── Send status messages (PTO, hitch, etc.) ─────────────────────────────
        void send_status_messages() {
            if (power_state_.state() == PowerState::PowerOff || power_state_.state() == PowerState::FinalShutdown) {
                return;
            }

            // Status messages are sent by TIM server
            // This is just a placeholder for any TECU-specific status messages
        }

        // ─── Handle facilities request from implement ────────────────────────────
        void handle_facilities_request(const TractorFacilities &required) {
            echo::category("isobus.tecu").debug("Received facilities request");
            on_facilities_request_received.emit(required);

            // Send our supported facilities in response
            // For secondary TECUs, send only non-duplicated facilities
            auto facilities_to_send = get_effective_facilities();
            facilities_interface_.send_facilities_response(facilities_to_send);
        }

        // ─── Handle facilities response (secondary TECU learning about primary) ──
        void handle_facilities_response(const TractorFacilities &response) {
            if (config_.classification.instance == 0) {
                // Primary TECU doesn't need to track other TECUs
                return;
            }

            echo::category("isobus.tecu").debug("Secondary TECU received facilities from primary");
            primary_tecu_facilities_ = response;

            // Update our effective facilities to avoid duplication
            update_supported_facilities();
        }

        // ─── Get effective facilities (with deduplication for secondary TECUs) ───
        TractorFacilities get_effective_facilities() const {
            if (config_.classification.instance == 0 || !primary_tecu_facilities_.has_value()) {
                // Primary TECU or no primary info yet: send all supported facilities
                return supported_facilities_;
            }

            // Secondary TECU: only send facilities NOT provided by primary
            TractorFacilities effective = supported_facilities_;
            const auto &primary = *primary_tecu_facilities_;

// Disable any facilities already provided by primary
#define DEDUPE_FACILITY(name)                                                                                          \
    if (primary.name)                                                                                                  \
    effective.name = false

            // Class 1
            DEDUPE_FACILITY(rear_hitch_position);
            DEDUPE_FACILITY(rear_hitch_in_work);
            DEDUPE_FACILITY(rear_pto_speed);
            DEDUPE_FACILITY(rear_pto_engagement);
            DEDUPE_FACILITY(wheel_based_speed);
            DEDUPE_FACILITY(ground_based_speed);

            // Class 2
            DEDUPE_FACILITY(ground_based_distance);
            DEDUPE_FACILITY(ground_based_direction);
            DEDUPE_FACILITY(wheel_based_distance);
            DEDUPE_FACILITY(wheel_based_direction);
            DEDUPE_FACILITY(rear_draft);
            DEDUPE_FACILITY(lighting);
            DEDUPE_FACILITY(aux_valve_flow);

            // Class 3
            DEDUPE_FACILITY(rear_hitch_command);
            DEDUPE_FACILITY(rear_pto_command);
            DEDUPE_FACILITY(aux_valve_command);
            DEDUPE_FACILITY(rear_hitch_limit_status);
            DEDUPE_FACILITY(rear_hitch_exit_code);
            DEDUPE_FACILITY(rear_pto_engagement_request);
            DEDUPE_FACILITY(rear_pto_speed_limit_status);
            DEDUPE_FACILITY(rear_pto_exit_code);
            DEDUPE_FACILITY(aux_valve_limit_status);
            DEDUPE_FACILITY(aux_valve_exit_code);

            // Front
            DEDUPE_FACILITY(front_hitch_position);
            DEDUPE_FACILITY(front_hitch_in_work);
            DEDUPE_FACILITY(front_pto_speed);
            DEDUPE_FACILITY(front_pto_engagement);
            DEDUPE_FACILITY(front_hitch_command);
            DEDUPE_FACILITY(front_pto_command);
            DEDUPE_FACILITY(front_hitch_limit_status);
            DEDUPE_FACILITY(front_hitch_exit_code);
            DEDUPE_FACILITY(front_pto_engagement_request);
            DEDUPE_FACILITY(front_pto_speed_limit_status);
            DEDUPE_FACILITY(front_pto_exit_code);

            // Addenda
            DEDUPE_FACILITY(navigation);
            DEDUPE_FACILITY(guidance);
            DEDUPE_FACILITY(machine_selected_speed);
            DEDUPE_FACILITY(machine_selected_speed_command);

#undef DEDUPE_FACILITY

            echo::category("isobus.tecu").debug("Secondary TECU effective facilities computed (after deduplication)");

            return effective;
        }

        // ─── Check if this TECU is primary ──────────────────────────────────────
        bool is_primary() const { return config_.classification.instance == 0; }

        // ─── Check if this TECU is secondary ────────────────────────────────────
        bool is_secondary() const { return config_.classification.instance > 0; }

        // ─── Power management state machine update ───────────────────────────────
        void update_power_management(u32 elapsed_ms) {
            auto current_state = power_state_.state();

            switch (current_state) {
            case PowerState::PowerOff:
                // Waiting for key ON
                break;

            case PowerState::IgnitionOn:
                // Normal operation
                break;

            case PowerState::ShutdownInitiated: {
                shutdown_timer_ms_ += elapsed_ms;

                // Remove expired maintain power requests
                maintain_power_requests_.erase(
                    std::remove_if(maintain_power_requests_.begin(), maintain_power_requests_.end(),
                                   [this, elapsed_ms](const MaintainPowerRequest &req) {
                                       return req.is_expired(shutdown_timer_ms_, config_.power.maintain_timeout_ms);
                                   }),
                    maintain_power_requests_.end());

                // Check if any CFs request power maintenance
                bool any_ecu_pwr_requested = false;
                bool any_pwr_requested = false;

                for (const auto &req : maintain_power_requests_) {
                    if (req.ecu_pwr)
                        any_ecu_pwr_requested = true;
                    if (req.pwr)
                        any_pwr_requested = true;
                }

                // Update power outputs based on requests
                // During minimum hold period, keep power on to allow CFs to send first request
                if (shutdown_timer_ms_ < config_.power.maintain_timeout_ms) {
                    ecu_pwr_enabled_ = true;
                    pwr_enabled_ = true;
                } else {
                    // After minimum hold, only maintain if requested
                    ecu_pwr_enabled_ = any_ecu_pwr_requested;
                    pwr_enabled_ = any_pwr_requested;
                }

                // Check shutdown conditions
                if (shutdown_timer_ms_ >= config_.power.shutdown_max_time_ms) {
                    // Max time expired
                    echo::category("isobus.tecu.power").warn("Shutdown max time expired, forcing power off");
                    power_state_.transition(PowerState::FinalShutdown);
                    ecu_pwr_enabled_ = false;
                    pwr_enabled_ = false;
                    on_shutdown_complete.emit();
                } else if (maintain_power_requests_.empty() &&
                           shutdown_timer_ms_ >= config_.power.maintain_timeout_ms) {
                    // No requests and minimum hold time met
                    echo::category("isobus.tecu.power").info("No maintain power requests, initiating final shutdown");
                    power_state_.transition(PowerState::FinalShutdown);
                    ecu_pwr_enabled_ = false;
                    pwr_enabled_ = false;
                    on_shutdown_complete.emit();
                }
                break;
            }

            case PowerState::FinalShutdown:
                // Power off complete
                ecu_pwr_enabled_ = false;
                pwr_enabled_ = false;
                break;
            }
        }
    };

} // namespace agrobus::isobus
