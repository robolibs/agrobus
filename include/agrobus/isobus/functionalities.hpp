#pragma once

#include <agrobus/net/constants.hpp>
#include <agrobus/net/error.hpp>
#include <agrobus/net/event.hpp>
#include <agrobus/net/internal_cf.hpp>
#include <agrobus/net/message.hpp>
#include <agrobus/net/network_manager.hpp>
#include <agrobus/net/types.hpp>
#include <datapod/datapod.hpp>
#include <echo/echo.hpp>

namespace agrobus::isobus {
    using namespace agrobus::net;

    // ─── ISO 11783-12 Control Function Functionalities (PGN 0xFC8E) ─────────────
    // Allows ECUs to advertise supported functionalities to other devices on the bus.

    enum class Functionality : u8 {
        MinimumControlFunction = 0,
        UniversalTerminalServer = 1,
        UniversalTerminalWorkingSet = 2,
        AuxOInputs = 3,
        AuxOFunctions = 4,
        AuxNInputs = 5,
        AuxNFunctions = 6,
        TaskControllerBasicServer = 7,
        TaskControllerBasicClient = 8,
        TaskControllerGeoServer = 9,
        TaskControllerGeoClient = 10,
        TaskControllerSectionControlServer = 11,
        TaskControllerSectionControlClient = 12,
        BasicTractorECUServer = 13,
        BasicTractorECUImplementClient = 14,
        TractorImplementManagementServer = 15,
        TractorImplementManagementClient = 16,
        FileServer = 17,
        FileServerClient = 18
    };

    enum class MinimumControlFunctionOptions : u8 {
        NoOptions = 0x00,
        Type1ECUInternalWeakTermination = 0x01,
        Type2ECUInternalEndPointTermination = 0x02,
        SupportOfHeartbeatProducer = 0x04,
        SupportOfHeartbeatConsumer = 0x08
    };

    enum class AuxOOptions : u8 {
        NoOptions = 0x00,
        SupportsType0Function = 0x01,
        SupportsType1Function = 0x02,
        SupportsType2Function = 0x04
    };

    enum class AuxNOptions : u16 {
        NoOptions = 0x0000,
        SupportsType0Function = 0x0001,
        SupportsType1Function = 0x0002,
        SupportsType2Function = 0x0004,
        SupportsType3Function = 0x0008,
        SupportsType4Function = 0x0010,
        SupportsType5Function = 0x0020,
        SupportsType6Function = 0x0040,
        SupportsType7Function = 0x0080,
        SupportsType8Function = 0x0100,
        SupportsType9Function = 0x0200,
        SupportsType10Function = 0x0400,
        SupportsType11Function = 0x0800,
        SupportsType12Function = 0x1000,
        SupportsType13Function = 0x2000,
        SupportsType14Function = 0x4000
    };

    enum class TaskControllerGeoServerOptions : u8 {
        NoOptions = 0x00,
        PolygonBasedPrescriptionMapsAreSupported = 0x01
    };

    enum class BasicTractorECUOptions : u8 {
        TECUNotMeetingCompleteClass1Requirements = 0x00,
        Class1NoOptions = 0x01,
        Class2NoOptions = 0x02,
        ClassRequiredLighting = 0x04,
        NavigationOption = 0x08,
        FrontHitchOption = 0x10,
        GuidanceOption = 0x20
    };

    enum class TractorImplementManagementOptions : u8 {
        NoOptions = 0,
        FrontPTODisengagementIsSupported = 1,
        FrontPTOEngagementCCWIsSupported = 2,
        FrontPTOEngagementCWIsSupported = 3,
        FrontPTOSpeedCCWIsSupported = 4,
        FrontPTOSpeedCWIsSupported = 5,
        RearPTODisengagementIsSupported = 6,
        RearPTOEngagementCCWIsSupported = 7,
        RearPTOEngagementCWIsSupported = 8,
        RearPTOSpeedCCWIsSupported = 9,
        RearPTOSpeedCWIsSupported = 10,
        FrontHitchMotionIsSupported = 11,
        FrontHitchPositionIsSupported = 12,
        RearHitchMotionIsSupported = 13,
        RearHitchPositionIsSupported = 14,
        VehicleSpeedInForwardDirectionIsSupported = 15,
        VehicleSpeedInReverseDirectionIsSupported = 16,
        VehicleSpeedStartMotionIsSupported = 17,
        VehicleSpeedStopMotionIsSupported = 18,
        VehicleSpeedForwardSetByServerIsSupported = 19,
        VehicleSpeedReverseSetByServerIsSupported = 20,
        VehicleSpeedChangeDirectionIsSupported = 21,
        GuidanceCurvatureIsSupported = 22
    };

    // ─── Bitwise operators for option enums ──────────────────────────────────────
    inline u8 operator|(MinimumControlFunctionOptions a, MinimumControlFunctionOptions b) {
        return static_cast<u8>(a) | static_cast<u8>(b);
    }
    inline u8 operator|(AuxOOptions a, AuxOOptions b) { return static_cast<u8>(a) | static_cast<u8>(b); }
    inline u16 operator|(AuxNOptions a, AuxNOptions b) { return static_cast<u16>(a) | static_cast<u16>(b); }

    // ─── Control Function Functionalities ────────────────────────────────────────
    class ControlFunctionFunctionalities {
      public:
        struct FunctionalityData {
            Functionality functionality = Functionality::MinimumControlFunction;
            u8 generation = 1;
            dp::Vector<u8> option_bytes;
        };

      private:
        IsoNet &net_;
        InternalCF *cf_;
        dp::Vector<FunctionalityData> supported_;

        // Separate option storage for complex options
        u8 min_cf_options_ = 0;
        u8 aux_o_inputs_options_ = 0;
        u8 aux_o_functions_options_ = 0;
        u16 aux_n_inputs_options_ = 0;
        u16 aux_n_functions_options_ = 0;
        u8 tc_geo_server_options_ = 0;
        u8 tc_geo_client_channels_ = 0;
        u8 tc_sc_server_booms_ = 0;
        u8 tc_sc_server_sections_ = 0;
        u8 tc_sc_client_booms_ = 0;
        u8 tc_sc_client_sections_ = 0;
        u8 basic_tecu_server_options_ = 0;
        u8 basic_tecu_client_options_ = 0;

        // TIM options: stored as bitfields (3 bytes for 23 options)
        dp::Array<u8, 3> tim_server_options_{};
        dp::Array<u8, 3> tim_client_options_{};

        // TIM aux valve support (up to 32 valves, 2 bits each: state, flow)
        dp::Array<u8, 8> tim_server_aux_valves_{};
        dp::Array<u8, 8> tim_client_aux_valves_{};

      public:
        ControlFunctionFunctionalities(IsoNet &net, InternalCF *cf) : net_(net), cf_(cf) {
            // Always support MinimumControlFunction
            set_functionality_is_supported(Functionality::MinimumControlFunction, 1, true);
        }

        Result<void> initialize() {
            if (!cf_) {
                return Result<void>::err(Error::invalid_state("control function not set"));
            }
            net_.register_pgn_callback(PGN_REQUEST, [this](const Message &msg) { handle_request(msg); });
            return {};
        }

        // ─── Functionality management ────────────────────────────────────────────
        void set_functionality_is_supported(Functionality functionality, u8 generation, bool is_supported) {
            if (is_supported) {
                for (auto &f : supported_) {
                    if (f.functionality == functionality) {
                        f.generation = generation;
                        return;
                    }
                }
                supported_.push_back({functionality, generation, {}});
                echo::category("isobus.protocol.functionalities")
                    .debug("Functionality added: ", static_cast<u8>(functionality), " gen=", generation);
            } else {
                for (auto it = supported_.begin(); it != supported_.end(); ++it) {
                    if (it->functionality == functionality) {
                        supported_.erase(it);
                        return;
                    }
                }
            }
        }

        bool get_functionality_is_supported(Functionality functionality) const {
            for (const auto &f : supported_) {
                if (f.functionality == functionality)
                    return true;
            }
            return false;
        }

        u8 get_functionality_generation(Functionality functionality) const {
            for (const auto &f : supported_) {
                if (f.functionality == functionality)
                    return f.generation;
            }
            return 0;
        }

        // ─── Minimum CF options ──────────────────────────────────────────────────
        void set_minimum_control_function_option_state(MinimumControlFunctionOptions option, bool state) {
            if (state)
                min_cf_options_ |= static_cast<u8>(option);
            else
                min_cf_options_ &= ~static_cast<u8>(option);
        }

        bool get_minimum_control_function_option_state(MinimumControlFunctionOptions option) const {
            return (min_cf_options_ & static_cast<u8>(option)) != 0;
        }

        // ─── AUX-O options ───────────────────────────────────────────────────────
        void set_aux_O_inputs_option_state(AuxOOptions option, bool state) {
            if (state)
                aux_o_inputs_options_ |= static_cast<u8>(option);
            else
                aux_o_inputs_options_ &= ~static_cast<u8>(option);
        }

        bool get_aux_O_inputs_option_state(AuxOOptions option) const {
            return (aux_o_inputs_options_ & static_cast<u8>(option)) != 0;
        }

        void set_aux_O_functions_option_state(AuxOOptions option, bool state) {
            if (state)
                aux_o_functions_options_ |= static_cast<u8>(option);
            else
                aux_o_functions_options_ &= ~static_cast<u8>(option);
        }

        bool get_aux_O_functions_option_state(AuxOOptions option) const {
            return (aux_o_functions_options_ & static_cast<u8>(option)) != 0;
        }

        // ─── AUX-N options ───────────────────────────────────────────────────────
        void set_aux_N_inputs_option_state(AuxNOptions option, bool state) {
            if (state)
                aux_n_inputs_options_ |= static_cast<u16>(option);
            else
                aux_n_inputs_options_ &= ~static_cast<u16>(option);
        }

        bool get_aux_N_inputs_option_state(AuxNOptions option) const {
            return (aux_n_inputs_options_ & static_cast<u16>(option)) != 0;
        }

        void set_aux_N_functions_option_state(AuxNOptions option, bool state) {
            if (state)
                aux_n_functions_options_ |= static_cast<u16>(option);
            else
                aux_n_functions_options_ &= ~static_cast<u16>(option);
        }

        bool get_aux_N_functions_option_state(AuxNOptions option) const {
            return (aux_n_functions_options_ & static_cast<u16>(option)) != 0;
        }

        // ─── TC GEO options ──────────────────────────────────────────────────────
        void set_task_controller_geo_server_option_state(TaskControllerGeoServerOptions option, bool state) {
            if (state)
                tc_geo_server_options_ |= static_cast<u8>(option);
            else
                tc_geo_server_options_ &= ~static_cast<u8>(option);
        }

        bool get_task_controller_geo_server_option_state(TaskControllerGeoServerOptions option) const {
            return (tc_geo_server_options_ & static_cast<u8>(option)) != 0;
        }

        void set_task_controller_geo_client_option(u8 number_of_control_channels) {
            tc_geo_client_channels_ = number_of_control_channels;
        }

        u8 get_task_controller_geo_client_option() const { return tc_geo_client_channels_; }

        // ─── TC Section Control options ──────────────────────────────────────────
        void set_task_controller_section_control_server_option_state(u8 num_booms, u8 num_sections) {
            tc_sc_server_booms_ = num_booms;
            tc_sc_server_sections_ = num_sections;
        }

        u8 get_task_controller_section_control_server_number_supported_booms() const { return tc_sc_server_booms_; }
        u8 get_task_controller_section_control_server_number_supported_sections() const {
            return tc_sc_server_sections_;
        }

        void set_task_controller_section_control_client_option_state(u8 num_booms, u8 num_sections) {
            tc_sc_client_booms_ = num_booms;
            tc_sc_client_sections_ = num_sections;
        }

        u8 get_task_controller_section_control_client_number_supported_booms() const { return tc_sc_client_booms_; }
        u8 get_task_controller_section_control_client_number_supported_sections() const {
            return tc_sc_client_sections_;
        }

        // ─── Basic Tractor ECU options ───────────────────────────────────────────
        void set_basic_tractor_ECU_server_option_state(BasicTractorECUOptions option, bool state) {
            if (state)
                basic_tecu_server_options_ |= static_cast<u8>(option);
            else
                basic_tecu_server_options_ &= ~static_cast<u8>(option);
        }

        bool get_basic_tractor_ECU_server_option_state(BasicTractorECUOptions option) const {
            return (basic_tecu_server_options_ & static_cast<u8>(option)) != 0;
        }

        void set_basic_tractor_ECU_implement_client_option_state(BasicTractorECUOptions option, bool state) {
            if (state)
                basic_tecu_client_options_ |= static_cast<u8>(option);
            else
                basic_tecu_client_options_ &= ~static_cast<u8>(option);
        }

        bool get_basic_tractor_ECU_implement_client_option_state(BasicTractorECUOptions option) const {
            return (basic_tecu_client_options_ & static_cast<u8>(option)) != 0;
        }

        // ─── TIM options ─────────────────────────────────────────────────────────
        void set_tractor_implement_management_server_option_state(TractorImplementManagementOptions option,
                                                                  bool state) {
            u8 bit = static_cast<u8>(option);
            u8 byte_idx = bit / 8;
            u8 bit_idx = bit % 8;
            if (byte_idx < 3) {
                if (state)
                    tim_server_options_[byte_idx] |= (1u << bit_idx);
                else
                    tim_server_options_[byte_idx] &= ~(1u << bit_idx);
            }
        }

        bool get_tractor_implement_management_server_option_state(TractorImplementManagementOptions option) const {
            u8 bit = static_cast<u8>(option);
            u8 byte_idx = bit / 8;
            u8 bit_idx = bit % 8;
            if (byte_idx >= 3)
                return false;
            return (tim_server_options_[byte_idx] & (1u << bit_idx)) != 0;
        }

        void set_tractor_implement_management_server_aux_valve_option(u8 valve_index, bool state_supported,
                                                                      bool flow_supported) {
            if (valve_index >= 32)
                return;
            u8 byte_idx = valve_index / 4;
            u8 bit_offset = (valve_index % 4) * 2;
            tim_server_aux_valves_[byte_idx] &= ~(0x03u << bit_offset);
            if (state_supported)
                tim_server_aux_valves_[byte_idx] |= (0x01u << bit_offset);
            if (flow_supported)
                tim_server_aux_valves_[byte_idx] |= (0x02u << bit_offset);
        }

        bool get_tractor_implement_management_server_aux_valve_state_supported(u8 valve_index) const {
            if (valve_index >= 32)
                return false;
            u8 byte_idx = valve_index / 4;
            u8 bit_offset = (valve_index % 4) * 2;
            return (tim_server_aux_valves_[byte_idx] & (0x01u << bit_offset)) != 0;
        }

        bool get_tractor_implement_management_server_aux_valve_flow_supported(u8 valve_index) const {
            if (valve_index >= 32)
                return false;
            u8 byte_idx = valve_index / 4;
            u8 bit_offset = (valve_index % 4) * 2;
            return (tim_server_aux_valves_[byte_idx] & (0x02u << bit_offset)) != 0;
        }

        void set_tractor_implement_management_client_option_state(TractorImplementManagementOptions option,
                                                                  bool state) {
            u8 bit = static_cast<u8>(option);
            u8 byte_idx = bit / 8;
            u8 bit_idx = bit % 8;
            if (byte_idx < 3) {
                if (state)
                    tim_client_options_[byte_idx] |= (1u << bit_idx);
                else
                    tim_client_options_[byte_idx] &= ~(1u << bit_idx);
            }
        }

        bool get_tractor_implement_management_client_option_state(TractorImplementManagementOptions option) const {
            u8 bit = static_cast<u8>(option);
            u8 byte_idx = bit / 8;
            u8 bit_idx = bit % 8;
            if (byte_idx >= 3)
                return false;
            return (tim_client_options_[byte_idx] & (1u << bit_idx)) != 0;
        }

        void set_tractor_implement_management_client_aux_valve_option(u8 valve_index, bool state_supported,
                                                                      bool flow_supported) {
            if (valve_index >= 32)
                return;
            u8 byte_idx = valve_index / 4;
            u8 bit_offset = (valve_index % 4) * 2;
            tim_client_aux_valves_[byte_idx] &= ~(0x03u << bit_offset);
            if (state_supported)
                tim_client_aux_valves_[byte_idx] |= (0x01u << bit_offset);
            if (flow_supported)
                tim_client_aux_valves_[byte_idx] |= (0x02u << bit_offset);
        }

        bool get_tractor_implement_management_client_aux_valve_state_supported(u8 valve_index) const {
            if (valve_index >= 32)
                return false;
            u8 byte_idx = valve_index / 4;
            u8 bit_offset = (valve_index % 4) * 2;
            return (tim_client_aux_valves_[byte_idx] & (0x01u << bit_offset)) != 0;
        }

        bool get_tractor_implement_management_client_aux_valve_flow_supported(u8 valve_index) const {
            if (valve_index >= 32)
                return false;
            u8 byte_idx = valve_index / 4;
            u8 bit_offset = (valve_index % 4) * 2;
            return (tim_client_aux_valves_[byte_idx] & (0x02u << bit_offset)) != 0;
        }

        // ─── Fluent API ────────────────────────────────────────────────────────────
        ControlFunctionFunctionalities &with_minimum_cf(u8 generation = 1) {
            set_functionality_is_supported(Functionality::MinimumControlFunction, generation, true);
            return *this;
        }

        ControlFunctionFunctionalities &with_ut_server(u8 generation = 4) {
            set_functionality_is_supported(Functionality::UniversalTerminalServer, generation, true);
            return *this;
        }

        ControlFunctionFunctionalities &with_ut_working_set(u8 generation = 4) {
            set_functionality_is_supported(Functionality::UniversalTerminalWorkingSet, generation, true);
            return *this;
        }

        ControlFunctionFunctionalities &with_aux_o_inputs(u8 generation = 1) {
            set_functionality_is_supported(Functionality::AuxOInputs, generation, true);
            return *this;
        }

        ControlFunctionFunctionalities &with_aux_o_functions(u8 generation = 1) {
            set_functionality_is_supported(Functionality::AuxOFunctions, generation, true);
            return *this;
        }

        ControlFunctionFunctionalities &with_aux_n_inputs(u8 generation = 1) {
            set_functionality_is_supported(Functionality::AuxNInputs, generation, true);
            return *this;
        }

        ControlFunctionFunctionalities &with_aux_n_functions(u8 generation = 1) {
            set_functionality_is_supported(Functionality::AuxNFunctions, generation, true);
            return *this;
        }

        ControlFunctionFunctionalities &with_tc_basic_server(u8 generation = 4) {
            set_functionality_is_supported(Functionality::TaskControllerBasicServer, generation, true);
            return *this;
        }

        ControlFunctionFunctionalities &with_tc_basic_client(u8 generation = 4) {
            set_functionality_is_supported(Functionality::TaskControllerBasicClient, generation, true);
            return *this;
        }

        ControlFunctionFunctionalities &with_tc_geo_server(u8 generation = 4) {
            set_functionality_is_supported(Functionality::TaskControllerGeoServer, generation, true);
            return *this;
        }

        ControlFunctionFunctionalities &with_tc_geo_client(u8 generation = 4) {
            set_functionality_is_supported(Functionality::TaskControllerGeoClient, generation, true);
            return *this;
        }

        ControlFunctionFunctionalities &with_tc_section_control_server(u8 generation = 4) {
            set_functionality_is_supported(Functionality::TaskControllerSectionControlServer, generation, true);
            return *this;
        }

        ControlFunctionFunctionalities &with_tc_section_control_client(u8 generation = 4) {
            set_functionality_is_supported(Functionality::TaskControllerSectionControlClient, generation, true);
            return *this;
        }

        ControlFunctionFunctionalities &with_basic_tecu_server(u8 generation = 1) {
            set_functionality_is_supported(Functionality::BasicTractorECUServer, generation, true);
            return *this;
        }

        ControlFunctionFunctionalities &with_basic_tecu_implement_client(u8 generation = 1) {
            set_functionality_is_supported(Functionality::BasicTractorECUImplementClient, generation, true);
            return *this;
        }

        ControlFunctionFunctionalities &with_tim_server(u8 generation = 1) {
            set_functionality_is_supported(Functionality::TractorImplementManagementServer, generation, true);
            return *this;
        }

        ControlFunctionFunctionalities &with_tim_client(u8 generation = 1) {
            set_functionality_is_supported(Functionality::TractorImplementManagementClient, generation, true);
            return *this;
        }

        ControlFunctionFunctionalities &with_file_server(u8 generation = 1) {
            set_functionality_is_supported(Functionality::FileServer, generation, true);
            return *this;
        }

        ControlFunctionFunctionalities &with_file_server_client(u8 generation = 1) {
            set_functionality_is_supported(Functionality::FileServerClient, generation, true);
            return *this;
        }

        // ─── Serialization ───────────────────────────────────────────────────────
        dp::Vector<u8> serialize() const {
            dp::Vector<u8> data;
            // Byte 0: Number of functionalities
            data.push_back(static_cast<u8>(supported_.size()));

            for (const auto &func : supported_) {
                // Functionality byte
                data.push_back(static_cast<u8>(func.functionality));
                // Generation byte
                data.push_back(func.generation);
                // Option bytes (functionality-dependent)
                auto opts = get_option_bytes(func.functionality);
                for (auto b : opts)
                    data.push_back(b);
            }
            return data;
        }

        const dp::Vector<FunctionalityData> &supported_functionalities() const { return supported_; }

        void update(u32 /*elapsed_ms*/) {
            // Nothing periodic needed; responds to PGN requests
        }

        // Events
        Event<const dp::Vector<u8> &> on_functionalities_request;

      private:
        dp::Vector<u8> get_option_bytes(Functionality func) const {
            dp::Vector<u8> opts;
            switch (func) {
            case Functionality::MinimumControlFunction:
                opts.push_back(min_cf_options_);
                break;
            case Functionality::AuxOInputs:
                opts.push_back(aux_o_inputs_options_);
                break;
            case Functionality::AuxOFunctions:
                opts.push_back(aux_o_functions_options_);
                break;
            case Functionality::AuxNInputs:
                opts.push_back(static_cast<u8>(aux_n_inputs_options_ & 0xFF));
                opts.push_back(static_cast<u8>((aux_n_inputs_options_ >> 8) & 0xFF));
                break;
            case Functionality::AuxNFunctions:
                opts.push_back(static_cast<u8>(aux_n_functions_options_ & 0xFF));
                opts.push_back(static_cast<u8>((aux_n_functions_options_ >> 8) & 0xFF));
                break;
            case Functionality::TaskControllerGeoServer:
                opts.push_back(tc_geo_server_options_);
                break;
            case Functionality::TaskControllerGeoClient:
                opts.push_back(tc_geo_client_channels_);
                break;
            case Functionality::TaskControllerSectionControlServer:
                opts.push_back(tc_sc_server_booms_);
                opts.push_back(tc_sc_server_sections_);
                break;
            case Functionality::TaskControllerSectionControlClient:
                opts.push_back(tc_sc_client_booms_);
                opts.push_back(tc_sc_client_sections_);
                break;
            case Functionality::BasicTractorECUServer:
                opts.push_back(basic_tecu_server_options_);
                break;
            case Functionality::BasicTractorECUImplementClient:
                opts.push_back(basic_tecu_client_options_);
                break;
            case Functionality::TractorImplementManagementServer:
                for (auto b : tim_server_options_)
                    opts.push_back(b);
                for (auto b : tim_server_aux_valves_)
                    opts.push_back(b);
                break;
            case Functionality::TractorImplementManagementClient:
                for (auto b : tim_client_options_)
                    opts.push_back(b);
                for (auto b : tim_client_aux_valves_)
                    opts.push_back(b);
                break;
            default:
                // UniversalTerminal, TaskControllerBasic, FileServer: 1 byte (0x00)
                opts.push_back(0x00);
                break;
            }
            return opts;
        }

        void handle_request(const Message &msg) {
            if (msg.data.size() < 3)
                return;

            PGN requested_pgn = static_cast<u32>(msg.data[0]) | (static_cast<u32>(msg.data[1]) << 8) |
                                (static_cast<u32>(msg.data[2]) << 16);

            if (requested_pgn != PGN_CF_FUNCTIONALITIES)
                return;

            echo::category("isobus.protocol.functionalities").debug("Functionalities request from ", msg.source);

            auto data = serialize();
            on_functionalities_request.emit(data);
            net_.send(PGN_CF_FUNCTIONALITIES, data, cf_, nullptr, Priority::Default);
        }
    };
} // namespace agrobus::isobus
