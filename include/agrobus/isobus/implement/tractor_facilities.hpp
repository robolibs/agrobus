#pragma once

#include <agrobus/net/constants.hpp>
#include <agrobus/net/error.hpp>
#include <agrobus/net/event.hpp>
#include <agrobus/net/internal_cf.hpp>
#include <agrobus/net/message.hpp>
#include <agrobus/net/network_manager.hpp>
#include <datapod/datapod.hpp>
#include <echo/echo.hpp>

namespace agrobus::isobus::implement {
    using namespace agrobus::net;

    // ─── TECU Classification (ISO 11783-9 Section 4.4.2) ───────────────────────
    enum class TECUClass : u8 {
        Class1 = 1, // Basic (speed, hitch, PTO, power management)
        Class2 = 2, // Full measurements (distance, direction, draft, lighting, aux flow)
        Class3 = 3  // Accepts commands (hitch cmd, PTO cmd, aux valve cmd)
    };

    // ─── Facility Flags (packed into bitfields) ────────────────────────────────
    struct TractorFacilities {
        // Class 1 facilities
        bool rear_hitch_position = false;
        bool rear_hitch_in_work = false;
        bool rear_pto_speed = false;
        bool rear_pto_engagement = false;
        bool wheel_based_speed = false;
        bool ground_based_speed = false;

        // Class 2 facilities
        bool ground_based_distance = false;
        bool ground_based_direction = false;
        bool wheel_based_distance = false;
        bool wheel_based_direction = false;
        bool rear_draft = false;
        bool lighting = false;
        bool aux_valve_flow = false;

        // Class 3 facilities
        bool rear_hitch_command = false;
        bool rear_pto_command = false;
        bool aux_valve_command = false;

        // Front-mounted (F addendum)
        bool front_hitch_position = false;
        bool front_hitch_in_work = false;
        bool front_pto_speed = false;
        bool front_pto_engagement = false;
        bool front_hitch_command = false;
        bool front_pto_command = false;

        // Navigation (N addendum)
        bool navigation = false;

        // Guidance (G addendum)
        bool guidance = false;

        // Powertrain (P addendum)
        bool machine_selected_speed = false;
        bool machine_selected_speed_command = false;

        // TECU v2 Class 3: limit status and exit/reason code support
        bool rear_hitch_limit_status = false;
        bool rear_hitch_exit_code = false;
        bool rear_pto_engagement_request = false;
        bool rear_pto_speed_limit_status = false;
        bool rear_pto_exit_code = false;
        bool aux_valve_limit_status = false;
        bool aux_valve_exit_code = false;

        // TECU v2 F addendum: front limit/exit code support
        bool front_hitch_limit_status = false;
        bool front_hitch_exit_code = false;
        bool front_pto_engagement_request = false;
        bool front_pto_speed_limit_status = false;
        bool front_pto_exit_code = false;

        // ─── Encode to 8 bytes (one bit per facility) ───────────────────────────
        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);

            // Byte 0: Class 1 + start of Class 2
            data[0] = 0;
            if (rear_hitch_position)
                data[0] |= (1u << 0);
            if (rear_hitch_in_work)
                data[0] |= (1u << 1);
            if (rear_pto_speed)
                data[0] |= (1u << 2);
            if (rear_pto_engagement)
                data[0] |= (1u << 3);
            if (wheel_based_speed)
                data[0] |= (1u << 4);
            if (ground_based_speed)
                data[0] |= (1u << 5);
            if (ground_based_distance)
                data[0] |= (1u << 6);
            if (ground_based_direction)
                data[0] |= (1u << 7);

            // Byte 1: Class 2 (cont) + Class 3
            data[1] = 0;
            if (wheel_based_distance)
                data[1] |= (1u << 0);
            if (wheel_based_direction)
                data[1] |= (1u << 1);
            if (rear_draft)
                data[1] |= (1u << 2);
            if (lighting)
                data[1] |= (1u << 3);
            if (aux_valve_flow)
                data[1] |= (1u << 4);
            if (rear_hitch_command)
                data[1] |= (1u << 5);
            if (rear_pto_command)
                data[1] |= (1u << 6);
            if (aux_valve_command)
                data[1] |= (1u << 7);

            // Byte 2: Front-mounted + Navigation + Guidance
            data[2] = 0;
            if (front_hitch_position)
                data[2] |= (1u << 0);
            if (front_hitch_in_work)
                data[2] |= (1u << 1);
            if (front_pto_speed)
                data[2] |= (1u << 2);
            if (front_pto_engagement)
                data[2] |= (1u << 3);
            if (front_hitch_command)
                data[2] |= (1u << 4);
            if (front_pto_command)
                data[2] |= (1u << 5);
            if (navigation)
                data[2] |= (1u << 6);
            if (guidance)
                data[2] |= (1u << 7);

            // Byte 3: Powertrain + v2 Class 3 limit/exit bits
            data[3] = 0;
            if (machine_selected_speed)
                data[3] |= (1u << 0);
            if (machine_selected_speed_command)
                data[3] |= (1u << 1);
            if (rear_hitch_limit_status)
                data[3] |= (1u << 2);
            if (rear_hitch_exit_code)
                data[3] |= (1u << 3);
            if (rear_pto_engagement_request)
                data[3] |= (1u << 4);
            if (rear_pto_speed_limit_status)
                data[3] |= (1u << 5);
            if (rear_pto_exit_code)
                data[3] |= (1u << 6);
            if (aux_valve_limit_status)
                data[3] |= (1u << 7);

            // Byte 4: v2 aux valve exit + front F addendum limit/exit bits
            data[4] = 0xC0; // reserved bits 6-7 set to 1
            if (aux_valve_exit_code)
                data[4] |= (1u << 0);
            if (front_hitch_limit_status)
                data[4] |= (1u << 1);
            if (front_hitch_exit_code)
                data[4] |= (1u << 2);
            if (front_pto_engagement_request)
                data[4] |= (1u << 3);
            if (front_pto_speed_limit_status)
                data[4] |= (1u << 4);
            if (front_pto_exit_code)
                data[4] |= (1u << 5);

            // Bytes 5-7: Reserved (0xFF)
            data[5] = 0xFF;
            data[6] = 0xFF;
            data[7] = 0xFF;

            return data;
        }

        // ─── Decode from 8-byte message data ────────────────────────────────────
        static TractorFacilities decode(const dp::Vector<u8> &data) {
            TractorFacilities f;
            if (data.size() < 4)
                return f;

            // Byte 0
            f.rear_hitch_position = (data[0] >> 0) & 0x01;
            f.rear_hitch_in_work = (data[0] >> 1) & 0x01;
            f.rear_pto_speed = (data[0] >> 2) & 0x01;
            f.rear_pto_engagement = (data[0] >> 3) & 0x01;
            f.wheel_based_speed = (data[0] >> 4) & 0x01;
            f.ground_based_speed = (data[0] >> 5) & 0x01;
            f.ground_based_distance = (data[0] >> 6) & 0x01;
            f.ground_based_direction = (data[0] >> 7) & 0x01;

            // Byte 1
            f.wheel_based_distance = (data[1] >> 0) & 0x01;
            f.wheel_based_direction = (data[1] >> 1) & 0x01;
            f.rear_draft = (data[1] >> 2) & 0x01;
            f.lighting = (data[1] >> 3) & 0x01;
            f.aux_valve_flow = (data[1] >> 4) & 0x01;
            f.rear_hitch_command = (data[1] >> 5) & 0x01;
            f.rear_pto_command = (data[1] >> 6) & 0x01;
            f.aux_valve_command = (data[1] >> 7) & 0x01;

            // Byte 2
            f.front_hitch_position = (data[2] >> 0) & 0x01;
            f.front_hitch_in_work = (data[2] >> 1) & 0x01;
            f.front_pto_speed = (data[2] >> 2) & 0x01;
            f.front_pto_engagement = (data[2] >> 3) & 0x01;
            f.front_hitch_command = (data[2] >> 4) & 0x01;
            f.front_pto_command = (data[2] >> 5) & 0x01;
            f.navigation = (data[2] >> 6) & 0x01;
            f.guidance = (data[2] >> 7) & 0x01;

            // Byte 3: Powertrain + v2 Class 3 limit/exit bits
            f.machine_selected_speed = (data[3] >> 0) & 0x01;
            f.machine_selected_speed_command = (data[3] >> 1) & 0x01;
            f.rear_hitch_limit_status = (data[3] >> 2) & 0x01;
            f.rear_hitch_exit_code = (data[3] >> 3) & 0x01;
            f.rear_pto_engagement_request = (data[3] >> 4) & 0x01;
            f.rear_pto_speed_limit_status = (data[3] >> 5) & 0x01;
            f.rear_pto_exit_code = (data[3] >> 6) & 0x01;
            f.aux_valve_limit_status = (data[3] >> 7) & 0x01;

            // Byte 4: v2 front F addendum limit/exit bits
            if (data.size() >= 5) {
                f.aux_valve_exit_code = (data[4] >> 0) & 0x01;
                f.front_hitch_limit_status = (data[4] >> 1) & 0x01;
                f.front_hitch_exit_code = (data[4] >> 2) & 0x01;
                f.front_pto_engagement_request = (data[4] >> 3) & 0x01;
                f.front_pto_speed_limit_status = (data[4] >> 4) & 0x01;
                f.front_pto_exit_code = (data[4] >> 5) & 0x01;
            }

            return f;
        }

        // ─── Fluent setters ─────────────────────────────────────────────────────
        TractorFacilities &set_class1_all() {
            rear_hitch_position = true;
            rear_hitch_in_work = true;
            rear_pto_speed = true;
            rear_pto_engagement = true;
            wheel_based_speed = true;
            ground_based_speed = true;
            return *this;
        }

        TractorFacilities &set_class2_all() {
            ground_based_distance = true;
            ground_based_direction = true;
            wheel_based_distance = true;
            wheel_based_direction = true;
            rear_draft = true;
            lighting = true;
            aux_valve_flow = true;
            return *this;
        }

        TractorFacilities &set_class3_all() {
            rear_hitch_command = true;
            rear_pto_command = true;
            aux_valve_command = true;
            return *this;
        }

        TractorFacilities &set_class3_v2_all() {
            rear_hitch_limit_status = true;
            rear_hitch_exit_code = true;
            rear_pto_engagement_request = true;
            rear_pto_speed_limit_status = true;
            rear_pto_exit_code = true;
            aux_valve_limit_status = true;
            aux_valve_exit_code = true;
            return *this;
        }

        TractorFacilities &set_front_v2_all() {
            front_hitch_limit_status = true;
            front_hitch_exit_code = true;
            front_pto_engagement_request = true;
            front_pto_speed_limit_status = true;
            front_pto_exit_code = true;
            return *this;
        }
    };

    // ─── Tractor Facilities Interface (ISO 11783-9 Section 4.4) ────────────────
    // Provides send/receive interface for:
    //   - Tractor Facilities Response (PGN 0xFE09): TECU broadcasts supported facilities
    //   - Required Tractor Facilities (PGN 0xFE0A): Implement requests specific facilities
    class TractorFacilitiesInterface {
        IsoNet &net_;
        InternalCF *cf_;
        dp::Optional<TractorFacilities> latest_response_;

      public:
        TractorFacilitiesInterface(IsoNet &net, InternalCF *cf) : net_(net), cf_(cf) {}

        Result<void> initialize() {
            // Register callback for incoming Tractor Facilities Response
            auto reg_result = net_.register_pgn_callback(PGN_TRACTOR_FACILITIES_RESPONSE, [this](const Message &msg) {
                auto facilities = TractorFacilities::decode(msg.data);
                latest_response_ = facilities;
                echo::category("isobus.implement.facilities").debug("Tractor Facilities Response received");
                on_facilities_response.emit(facilities);
            });
            if (reg_result.is_err())
                return reg_result;

            // Register callback for incoming Required Tractor Facilities
            reg_result = net_.register_pgn_callback(PGN_REQUIRED_TRACTOR_FACILITIES, [this](const Message &msg) {
                auto facilities = TractorFacilities::decode(msg.data);
                echo::category("isobus.implement.facilities").debug("Required Tractor Facilities received");
                on_facilities_required.emit(facilities);
            });
            if (reg_result.is_err())
                return reg_result;

            echo::category("isobus.implement.facilities").info("Tractor Facilities interface initialized");
            return {};
        }

        // ─── Send Tractor Facilities Response (TECU -> broadcast) ───────────────
        Result<void> send_facilities_response(TractorFacilities facilities) {
            auto data = facilities.encode();
            echo::category("isobus.implement.facilities").debug("Sending Tractor Facilities Response");
            return net_.send(PGN_TRACTOR_FACILITIES_RESPONSE, data, cf_);
        }

        // ─── Send Required Tractor Facilities (implement -> TECU) ───────────────
        Result<void> send_required_facilities(TractorFacilities facilities) {
            auto data = facilities.encode();
            echo::category("isobus.implement.facilities").debug("Sending Required Tractor Facilities");
            return net_.send(PGN_REQUIRED_TRACTOR_FACILITIES, data, cf_);
        }

        // ─── Latest received response ───────────────────────────────────────────
        dp::Optional<TractorFacilities> latest_response() const { return latest_response_; }

        // ─── Events ─────────────────────────────────────────────────────────────
        Event<TractorFacilities> on_facilities_response;
        Event<TractorFacilities> on_facilities_required;
    };

} // namespace agrobus::isobus::implement
