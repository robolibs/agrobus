#pragma once

#include <agrobus/net/constants.hpp>
#include <agrobus/net/control_function.hpp>
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

    // ─── ISO 11783 Tractor Implement Management (TIM) ────────────────────────────

    enum class TimOption : u8 {
        FrontPTODisengagementIsSupported = 0,
        FrontPTOEngagementCCWIsSupported = 1,
        FrontPTOEngagementCWIsSupported = 2,
        FrontPTOSpeedCCWIsSupported = 3,
        FrontPTOSpeedCWIsSupported = 4,
        RearPTODisengagementIsSupported = 5,
        RearPTOEngagementCCWIsSupported = 6,
        RearPTOEngagementCWIsSupported = 7,
        RearPTOSpeedCCWIsSupported = 8,
        RearPTOSpeedCWIsSupported = 9,
        FrontHitchMotionIsSupported = 10,
        FrontHitchPositionIsSupported = 11,
        RearHitchMotionIsSupported = 12,
        RearHitchPositionIsSupported = 13,
        VehicleSpeedInForwardDirectionIsSupported = 14,
        VehicleSpeedInReverseDirectionIsSupported = 15,
        VehicleSpeedStartMotionIsSupported = 16,
        VehicleSpeedStopMotionIsSupported = 17,
        VehicleSpeedForwardSetByServerIsSupported = 18,
        VehicleSpeedReverseSetByServerIsSupported = 19,
        VehicleSpeedChangeDirectionIsSupported = 20,
        GuidanceCurvatureIsSupported = 21
    };

    struct PTOState {
        bool engaged = false;
        bool cw_direction = true;
        u16 speed = 0; // RPM
    };

    struct HitchState {
        bool motion_enabled = false;
        u16 position = 0; // 0-100% (scaled 0-10000 for 0.01% resolution)
    };

    struct AuxValve {
        bool state_supported = false;
        bool flow_supported = false;
        bool state = false;
        u16 flow = 0; // Percentage or L/min (scaled)
    };

    inline constexpr u8 MAX_AUX_VALVES = 32;
    inline constexpr u32 TIM_UPDATE_INTERVAL_MS = 100;

    // ─── TIM Server Config ──────────────────────────────────────────────────────
    struct TimServerConfig {
        u32 update_interval_ms = TIM_UPDATE_INTERVAL_MS;
        bool enable_front_pto = true;
        bool enable_rear_pto = true;
        bool enable_front_hitch = true;
        bool enable_rear_hitch = true;

        TimServerConfig &interval(u32 ms) {
            update_interval_ms = ms;
            return *this;
        }
        TimServerConfig &front_pto(bool enable) {
            enable_front_pto = enable;
            return *this;
        }
        TimServerConfig &rear_pto(bool enable) {
            enable_rear_pto = enable;
            return *this;
        }
        TimServerConfig &front_hitch(bool enable) {
            enable_front_hitch = enable;
            return *this;
        }
        TimServerConfig &rear_hitch(bool enable) {
            enable_rear_hitch = enable;
            return *this;
        }
    };

    // ─── TIM Server (Tractor ECU side) ───────────────────────────────────────────
    class TimServer {
        IsoNet &net_;
        InternalCF *cf_;
        TimServerConfig config_;

        PTOState front_pto_;
        PTOState rear_pto_;
        HitchState front_hitch_;
        HitchState rear_hitch_;
        dp::Array<AuxValve, MAX_AUX_VALVES> aux_valves_{};
        u32 timer_ms_ = 0;

      public:
        TimServer(IsoNet &net, InternalCF *cf, TimServerConfig config = {}) : net_(net), cf_(cf), config_(config) {}

        Result<void> initialize() {
            if (!cf_) {
                return Result<void>::err(Error::invalid_state("control function not set"));
            }
            echo::category("isobus.protocol.tim").info("TIM Server initialized");
            return {};
        }

        // ─── PTO control ─────────────────────────────────────────────────────────
        Result<void> set_front_pto(bool engaged, bool cw, u16 speed) {
            echo::category("isobus.protocol.tim").debug("front PTO: engaged=", engaged, " speed=", speed);
            front_pto_ = {engaged, cw, speed};
            on_front_pto_changed.emit(front_pto_);
            return {};
        }

        Result<void> set_rear_pto(bool engaged, bool cw, u16 speed) {
            echo::category("isobus.protocol.tim").debug("rear PTO: engaged=", engaged, " speed=", speed);
            rear_pto_ = {engaged, cw, speed};
            on_rear_pto_changed.emit(rear_pto_);
            return {};
        }

        PTOState get_front_pto() const noexcept { return front_pto_; }
        PTOState get_rear_pto() const noexcept { return rear_pto_; }

        // ─── Hitch control ───────────────────────────────────────────────────────
        Result<void> set_front_hitch(bool motion, u16 position) {
            front_hitch_ = {motion, position};
            on_front_hitch_changed.emit(front_hitch_);
            return {};
        }

        Result<void> set_rear_hitch(bool motion, u16 position) {
            rear_hitch_ = {motion, position};
            on_rear_hitch_changed.emit(rear_hitch_);
            return {};
        }

        HitchState get_front_hitch() const noexcept { return front_hitch_; }
        HitchState get_rear_hitch() const noexcept { return rear_hitch_; }

        // ─── Aux valve control ───────────────────────────────────────────────────
        Result<void> set_aux_valve(u8 index, bool state, u16 flow) {
            if (index >= MAX_AUX_VALVES) {
                return Result<void>::err(Error::invalid_state("valve index out of range"));
            }
            aux_valves_[index].state = state;
            aux_valves_[index].flow = flow;
            on_aux_valve_changed.emit(index, aux_valves_[index]);
            return {};
        }

        void set_aux_valve_capabilities(u8 index, bool state_supported, bool flow_supported) {
            if (index < MAX_AUX_VALVES) {
                aux_valves_[index].state_supported = state_supported;
                aux_valves_[index].flow_supported = flow_supported;
            }
        }

        AuxValve get_aux_valve(u8 index) const {
            if (index >= MAX_AUX_VALVES)
                return {};
            return aux_valves_[index];
        }

        // ─── Events ──────────────────────────────────────────────────────────────
        Event<const PTOState &> on_front_pto_changed;
        Event<const PTOState &> on_rear_pto_changed;
        Event<const HitchState &> on_front_hitch_changed;
        Event<const HitchState &> on_rear_hitch_changed;
        Event<u8, const AuxValve &> on_aux_valve_changed;

        // ─── Update loop ─────────────────────────────────────────────────────────
        void update(u32 elapsed_ms) {
            timer_ms_ += elapsed_ms;
            if (timer_ms_ >= config_.update_interval_ms) {
                timer_ms_ -= config_.update_interval_ms;
                send_pto_status();
                send_hitch_status();
            }
        }

      private:
        void send_pto_status() {
            echo::category("isobus.protocol.tim").trace("sending PTO status");
            // Front PTO
            {
                dp::Vector<u8> data(8, 0xFF);
                data[0] = front_pto_.engaged ? 0x01 : 0x00;
                data[1] = front_pto_.cw_direction ? 0x01 : 0x00;
                data[2] = static_cast<u8>(front_pto_.speed & 0xFF);
                data[3] = static_cast<u8>((front_pto_.speed >> 8) & 0xFF);
                net_.send(PGN_FRONT_PTO, data, cf_, nullptr, Priority::Default);
            }
            // Rear PTO
            {
                dp::Vector<u8> data(8, 0xFF);
                data[0] = rear_pto_.engaged ? 0x01 : 0x00;
                data[1] = rear_pto_.cw_direction ? 0x01 : 0x00;
                data[2] = static_cast<u8>(rear_pto_.speed & 0xFF);
                data[3] = static_cast<u8>((rear_pto_.speed >> 8) & 0xFF);
                net_.send(PGN_REAR_PTO, data, cf_, nullptr, Priority::Default);
            }
        }

        void send_hitch_status() {
            echo::category("isobus.protocol.tim").trace("sending hitch status");
            // Front hitch
            {
                dp::Vector<u8> data(8, 0xFF);
                data[0] = front_hitch_.motion_enabled ? 0x01 : 0x00;
                data[1] = static_cast<u8>(front_hitch_.position & 0xFF);
                data[2] = static_cast<u8>((front_hitch_.position >> 8) & 0xFF);
                net_.send(PGN_FRONT_HITCH, data, cf_, nullptr, Priority::Default);
            }
            // Rear hitch
            {
                dp::Vector<u8> data(8, 0xFF);
                data[0] = rear_hitch_.motion_enabled ? 0x01 : 0x00;
                data[1] = static_cast<u8>(rear_hitch_.position & 0xFF);
                data[2] = static_cast<u8>((rear_hitch_.position >> 8) & 0xFF);
                net_.send(PGN_REAR_HITCH, data, cf_, nullptr, Priority::Default);
            }
        }
    };

    // ─── TIM Client (Implement ECU side) ─────────────────────────────────────────
    class TimClient {
        IsoNet &net_;
        InternalCF *cf_;
        ControlFunction *tractor_server_ = nullptr;

        PTOState front_pto_;
        PTOState rear_pto_;
        HitchState front_hitch_;
        HitchState rear_hitch_;
        dp::Array<AuxValve, MAX_AUX_VALVES> aux_valves_{};

      public:
        TimClient(IsoNet &net, InternalCF *cf, ControlFunction *tractor = nullptr)
            : net_(net), cf_(cf), tractor_server_(tractor) {}

        Result<void> initialize() {
            if (!cf_) {
                return Result<void>::err(Error::invalid_state("control function not set"));
            }
            net_.register_pgn_callback(PGN_FRONT_PTO, [this](const Message &msg) { handle_front_pto(msg); });
            net_.register_pgn_callback(PGN_REAR_PTO, [this](const Message &msg) { handle_rear_pto(msg); });
            net_.register_pgn_callback(PGN_FRONT_HITCH, [this](const Message &msg) { handle_front_hitch(msg); });
            net_.register_pgn_callback(PGN_REAR_HITCH, [this](const Message &msg) { handle_rear_hitch(msg); });
            echo::category("isobus.protocol.tim").info("TIM Client initialized");
            return {};
        }

        void set_tractor_server(ControlFunction *tractor) { tractor_server_ = tractor; }

        // ─── Current state getters ───────────────────────────────────────────────
        PTOState get_front_pto() const noexcept { return front_pto_; }
        PTOState get_rear_pto() const noexcept { return rear_pto_; }
        HitchState get_front_hitch() const noexcept { return front_hitch_; }
        HitchState get_rear_hitch() const noexcept { return rear_hitch_; }
        AuxValve get_aux_valve(u8 index) const {
            if (index >= MAX_AUX_VALVES)
                return {};
            return aux_valves_[index];
        }

        // ─── Request changes ─────────────────────────────────────────────────────
        Result<void> request_front_pto(bool engaged, bool cw, u16 speed) {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = engaged ? 0x01 : 0x00;
            data[1] = cw ? 0x01 : 0x00;
            data[2] = static_cast<u8>(speed & 0xFF);
            data[3] = static_cast<u8>((speed >> 8) & 0xFF);
            return net_.send(PGN_FRONT_PTO, data, cf_, tractor_server_, Priority::Default);
        }

        Result<void> request_rear_pto(bool engaged, bool cw, u16 speed) {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = engaged ? 0x01 : 0x00;
            data[1] = cw ? 0x01 : 0x00;
            data[2] = static_cast<u8>(speed & 0xFF);
            data[3] = static_cast<u8>((speed >> 8) & 0xFF);
            return net_.send(PGN_REAR_PTO, data, cf_, tractor_server_, Priority::Default);
        }

        Result<void> request_front_hitch(bool motion, u16 position) {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = motion ? 0x01 : 0x00;
            data[1] = static_cast<u8>(position & 0xFF);
            data[2] = static_cast<u8>((position >> 8) & 0xFF);
            return net_.send(PGN_FRONT_HITCH, data, cf_, tractor_server_, Priority::Default);
        }

        Result<void> request_rear_hitch(bool motion, u16 position) {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = motion ? 0x01 : 0x00;
            data[1] = static_cast<u8>(position & 0xFF);
            data[2] = static_cast<u8>((position >> 8) & 0xFF);
            return net_.send(PGN_REAR_HITCH, data, cf_, tractor_server_, Priority::Default);
        }

        Result<void> request_aux_valve(u8 index, bool state, u16 flow) {
            if (index >= MAX_AUX_VALVES)
                return Result<void>::err(Error::invalid_state("valve index out of range"));

            dp::Vector<u8> data(8, 0xFF);
            data[0] = index;
            data[1] = state ? 0x01 : 0x00;
            data[2] = static_cast<u8>(flow & 0xFF);
            data[3] = static_cast<u8>((flow >> 8) & 0xFF);
            return net_.send(PGN_AUX_VALVE_0_7, data, cf_, tractor_server_, Priority::Default);
        }

        // ─── Events ──────────────────────────────────────────────────────────────
        Event<const PTOState &> on_front_pto_updated;
        Event<const PTOState &> on_rear_pto_updated;
        Event<const HitchState &> on_front_hitch_updated;
        Event<const HitchState &> on_rear_hitch_updated;
        Event<u8, const AuxValve &> on_aux_valve_updated;

        void update(u32 /*elapsed_ms*/) {}

      private:
        void handle_front_pto(const Message &msg) {
            echo::category("isobus.protocol.tim").trace("front PTO update received");
            if (msg.data.size() < 4)
                return;
            front_pto_.engaged = (msg.data[0] & 0x01) != 0;
            front_pto_.cw_direction = (msg.data[1] & 0x01) != 0;
            front_pto_.speed = static_cast<u16>(msg.data[2]) | (static_cast<u16>(msg.data[3]) << 8);
            on_front_pto_updated.emit(front_pto_);
        }

        void handle_rear_pto(const Message &msg) {
            echo::category("isobus.protocol.tim").trace("rear PTO update received");
            if (msg.data.size() < 4)
                return;
            rear_pto_.engaged = (msg.data[0] & 0x01) != 0;
            rear_pto_.cw_direction = (msg.data[1] & 0x01) != 0;
            rear_pto_.speed = static_cast<u16>(msg.data[2]) | (static_cast<u16>(msg.data[3]) << 8);
            on_rear_pto_updated.emit(rear_pto_);
        }

        void handle_front_hitch(const Message &msg) {
            echo::category("isobus.protocol.tim").trace("front hitch update received");
            if (msg.data.size() < 3)
                return;
            front_hitch_.motion_enabled = (msg.data[0] & 0x01) != 0;
            front_hitch_.position = static_cast<u16>(msg.data[1]) | (static_cast<u16>(msg.data[2]) << 8);
            on_front_hitch_updated.emit(front_hitch_);
        }

        void handle_rear_hitch(const Message &msg) {
            echo::category("isobus.protocol.tim").trace("rear hitch update received");
            if (msg.data.size() < 3)
                return;
            rear_hitch_.motion_enabled = (msg.data[0] & 0x01) != 0;
            rear_hitch_.position = static_cast<u16>(msg.data[1]) | (static_cast<u16>(msg.data[2]) << 8);
            on_rear_hitch_updated.emit(rear_hitch_);
        }
    };
} // namespace agrobus::isobus
