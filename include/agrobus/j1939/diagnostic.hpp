#pragma once

#include "acknowledgment.hpp"
#include <agrobus/net/constants.hpp>
#include <agrobus/net/error.hpp>
#include <agrobus/net/event.hpp>
#include <agrobus/net/message.hpp>
#include <agrobus/net/network_manager.hpp>
#include <agrobus/net/types.hpp>
#include <datapod/datapod.hpp>
#include <echo/echo.hpp>

namespace agrobus::j1939 {
    using namespace agrobus::net;

    // ─── Lamp status ─────────────────────────────────────────────────────────────
    enum class LampStatus : u8 { Off = 0, On = 1, Error = 2, NotAvailable = 3 };

    enum class LampFlash : u8 { SlowFlash = 0, FastFlash = 1, Off = 2, NotAvailable = 3 };

    // ─── Failure Mode Identifier (FMI) ──────────────────────────────────────────
    enum class FMI : u8 {
        AboveNormal = 0,
        BelowNormal = 1,
        Erratic = 2,
        VoltageHigh = 3,
        VoltageLow = 4,
        CurrentLow = 5,
        CurrentHigh = 6,
        MechanicalFail = 7,
        AbnormalFrequency = 8,
        AbnormalUpdate = 9,
        AbnormalRateChange = 10,
        RootCauseUnknown = 11,
        BadDevice = 12,
        OutOfCalibration = 13,
        SpecialInstructions = 14,
        AboveNormalLeast = 15,
        AboveNormalModerate = 16,
        BelowNormalLeast = 17,
        BelowNormalModerate = 18,
        ReceivedNetworkData = 19,
        ConditionExists = 31
    };

    // ─── Diagnostic Trouble Code (DTC) ──────────────────────────────────────────
    struct DTC {
        u32 spn = 0; // Suspect Parameter Number (19 bits)
        FMI fmi = FMI::RootCauseUnknown;
        u8 occurrence_count = 0;

        constexpr bool operator==(const DTC &other) const noexcept { return spn == other.spn && fmi == other.fmi; }

        // Encode DTC to 4 bytes (SPN:19 + FMI:5 + OC:7 + CM:1)
        dp::Array<u8, 4> encode() const noexcept {
            dp::Array<u8, 4> bytes{};
            bytes[0] = static_cast<u8>(spn & 0xFF);
            bytes[1] = static_cast<u8>((spn >> 8) & 0xFF);
            bytes[2] = static_cast<u8>(((spn >> 16) & 0x07) << 5) | (static_cast<u8>(fmi) & 0x1F);
            bytes[3] = occurrence_count & 0x7F;
            return bytes;
        }

        static DTC decode(const u8 *data) noexcept {
            DTC dtc;
            dtc.spn = static_cast<u32>(data[0]) | (static_cast<u32>(data[1]) << 8) |
                      (static_cast<u32>((data[2] >> 5) & 0x07) << 16);
            dtc.fmi = static_cast<FMI>(data[2] & 0x1F);
            dtc.occurrence_count = data[3] & 0x7F;
            return dtc;
        }
    };

    // ─── Previously Active DTC (with occurrence tracking) ──────────────────────
    struct PreviouslyActiveDTC {
        DTC dtc;
        u8 occurrence_count = 0;
    };

    // ─── Diagnostic lamp status ──────────────────────────────────────────────────
    struct DiagnosticLamps {
        LampStatus malfunction = LampStatus::Off;
        LampFlash malfunction_flash = LampFlash::Off;
        LampStatus red_stop = LampStatus::Off;
        LampFlash red_stop_flash = LampFlash::Off;
        LampStatus amber_warning = LampStatus::Off;
        LampFlash amber_warning_flash = LampFlash::Off;
        LampStatus engine_protect = LampStatus::Off;
        LampFlash engine_protect_flash = LampFlash::Off;

        dp::Array<u8, 2> encode() const noexcept {
            dp::Array<u8, 2> bytes{};
            bytes[0] = (static_cast<u8>(engine_protect) << 6) | (static_cast<u8>(amber_warning) << 4) |
                       (static_cast<u8>(red_stop) << 2) | static_cast<u8>(malfunction);
            bytes[1] = (static_cast<u8>(engine_protect_flash) << 6) | (static_cast<u8>(amber_warning_flash) << 4) |
                       (static_cast<u8>(red_stop_flash) << 2) | static_cast<u8>(malfunction_flash);
            return bytes;
        }

        static DiagnosticLamps decode(const u8 *data) noexcept {
            DiagnosticLamps lamps;
            lamps.malfunction = static_cast<LampStatus>(data[0] & 0x03);
            lamps.red_stop = static_cast<LampStatus>((data[0] >> 2) & 0x03);
            lamps.amber_warning = static_cast<LampStatus>((data[0] >> 4) & 0x03);
            lamps.engine_protect = static_cast<LampStatus>((data[0] >> 6) & 0x03);
            lamps.malfunction_flash = static_cast<LampFlash>(data[1] & 0x03);
            lamps.red_stop_flash = static_cast<LampFlash>((data[1] >> 2) & 0x03);
            lamps.amber_warning_flash = static_cast<LampFlash>((data[1] >> 4) & 0x03);
            lamps.engine_protect_flash = static_cast<LampFlash>((data[1] >> 6) & 0x03);
            return lamps;
        }
    };

    // ─── Diagnostic configuration ──────────────────────────────────────────────
    struct DiagnosticConfig {
        u32 dm1_interval_ms = 1000;
        bool auto_send = false;
        u8 max_freeze_frames_per_dtc = 3; // Max freeze frames to store per DTC
        bool auto_capture_freeze_frames = true; // Auto-capture on DTC activation

        DiagnosticConfig &interval(u32 ms) {
            dm1_interval_ms = ms;
            return *this;
        }
        DiagnosticConfig &enable_auto_send(bool enable = true) {
            auto_send = enable;
            return *this;
        }
        DiagnosticConfig &freeze_frame_depth(u8 depth) {
            max_freeze_frames_per_dtc = depth;
            return *this;
        }
        DiagnosticConfig &auto_capture_freeze_frames_enabled(bool enable = true) {
            auto_capture_freeze_frames = enable;
            return *this;
        }
    };

    // ─── DM13 suspend/resume signals ─────────────────────────────────────────────
    enum class DM13Command : u8 { SuspendBroadcast = 0, ResumeBroadcast = 1, Undefined = 2, DoNotCare = 3 };

    struct DM13Signals {
        DM13Command hold_signal = DM13Command::DoNotCare;
        DM13Command dm1_signal = DM13Command::DoNotCare;
        DM13Command dm2_signal = DM13Command::DoNotCare;
        DM13Command dm3_signal = DM13Command::DoNotCare;
        u16 suspend_duration_s = 0xFFFF; // 0xFFFF = indefinite
    };

    // ─── DM5 Diagnostic Protocol Identification (ISO 11783-12 B.5, A.6) ───────────
    // Bitmask: multiple protocols can be indicated simultaneously
    enum class DiagProtocol : u8 {
        None = 0x00,        // No additional diagnostic protocols supported
        J1939_73 = 0x01,    // SAE J1939-73
        ISO_14230 = 0x02,   // ISO 14230 (KWP 2000)
        ISO_14229_3 = 0x04, // ISO 14229-3 (UDS on CAN)
    };

    inline constexpr u8 operator|(DiagProtocol a, DiagProtocol b) { return static_cast<u8>(a) | static_cast<u8>(b); }

    struct DiagnosticProtocolID {
        u8 protocols = static_cast<u8>(DiagProtocol::J1939_73); // Default: J1939 diagnostics

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = protocols;
            return data;
        }

        static DiagnosticProtocolID decode(const dp::Vector<u8> &data) {
            DiagnosticProtocolID id;
            if (!data.empty()) {
                id.protocols = data[0];
            }
            return id;
        }
    };

    // ─── DM22 individual DTC clear/reset ──────────────────────────────────────────
    enum class DM22Control : u8 {
        ClearPreviouslyActive = 0x01,
        ClearActive = 0x02,
        AckClearPreviouslyActive = 0x11,
        AckClearActive = 0x12,
        NackClearPreviouslyActive = 0x21,
        NackClearActive = 0x22
    };

    enum class DM22NackReason : u8 {
        GeneralNack = 0x00,
        AccessDenied = 0x01,
        UnknownDTC = 0x02,
        DTCNoLongerActive = 0x03,
        DTCNoLongerPrevious = 0x04
    };

    // ─── Product/Software identification ──────────────────────────────────────────
    struct ProductIdentification {
        dp::String make;
        dp::String model;
        dp::String serial_number;

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data;
            for (char c : make)
                data.push_back(static_cast<u8>(c));
            data.push_back('*');
            for (char c : model)
                data.push_back(static_cast<u8>(c));
            data.push_back('*');
            for (char c : serial_number)
                data.push_back(static_cast<u8>(c));
            data.push_back('*');
            return data;
        }

        static ProductIdentification decode(const dp::Vector<u8> &data) {
            ProductIdentification id;
            usize field = 0;
            for (u8 b : data) {
                if (b == '*') {
                    ++field;
                    continue;
                }
                char c = static_cast<char>(b);
                switch (field) {
                case 0:
                    id.make += c;
                    break;
                case 1:
                    id.model += c;
                    break;
                case 2:
                    id.serial_number += c;
                    break;
                default:
                    break;
                }
            }
            return id;
        }
    };

    struct SoftwareIdentification {
        dp::Vector<dp::String> versions;

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data;
            data.push_back(static_cast<u8>(versions.size()));
            for (const auto &ver : versions) {
                for (char c : ver)
                    data.push_back(static_cast<u8>(c));
                data.push_back('*');
            }
            return data;
        }

        static SoftwareIdentification decode(const dp::Vector<u8> &data) {
            SoftwareIdentification id;
            if (data.empty())
                return id;
            u8 count = data[0];
            dp::String current;
            for (usize i = 1; i < data.size(); ++i) {
                if (data[i] == '*') {
                    id.versions.push_back(std::move(current));
                    current.clear();
                    if (id.versions.size() >= count)
                        break;
                } else {
                    current += static_cast<char>(data[i]);
                }
            }
            return id;
        }
    };

    // ─── Monitor Performance Ratio (DM20) ────────────────────────────────────────
    // Performance ratios track emissions monitor execution vs. opportunities
    // Used for OBD compliance and readiness monitoring
    struct MonitorPerformanceRatio {
        u32 spn = 0;           // Monitor identifier SPN
        u16 numerator = 0;     // Number of times monitor executed
        u16 denominator = 0;   // Number of monitoring opportunities

        dp::Array<u8, 7> encode() const noexcept {
            dp::Array<u8, 7> bytes{};
            // SPN (19 bits)
            bytes[0] = static_cast<u8>(spn & 0xFF);
            bytes[1] = static_cast<u8>((spn >> 8) & 0xFF);
            bytes[2] = static_cast<u8>((spn >> 16) & 0x07);
            // Numerator (16 bits)
            bytes[3] = static_cast<u8>(numerator & 0xFF);
            bytes[4] = static_cast<u8>((numerator >> 8) & 0xFF);
            // Denominator (16 bits)
            bytes[5] = static_cast<u8>(denominator & 0xFF);
            bytes[6] = static_cast<u8>((denominator >> 8) & 0xFF);
            return bytes;
        }

        static MonitorPerformanceRatio decode(const u8 *data) noexcept {
            MonitorPerformanceRatio ratio;
            ratio.spn = static_cast<u32>(data[0]) | (static_cast<u32>(data[1]) << 8) |
                        (static_cast<u32>(data[2] & 0x07) << 16);
            ratio.numerator = static_cast<u16>(data[3]) | (static_cast<u16>(data[4]) << 8);
            ratio.denominator = static_cast<u16>(data[5]) | (static_cast<u16>(data[6]) << 8);
            return ratio;
        }

        // Calculate ratio percentage (0-100)
        u8 percentage() const noexcept {
            if (denominator == 0)
                return 0;
            u32 result = (static_cast<u32>(numerator) * 100) / denominator;
            return static_cast<u8>(result > 100 ? 100 : result);
        }

        // Check if ratio meets minimum threshold (typically 75% for OBD)
        bool meets_threshold(u8 threshold = 75) const noexcept { return percentage() >= threshold; }
    };

    // DM20 (PGN 0xC200) contains multiple performance ratios
    struct DM20Response {
        u8 ignition_cycles = 0;                        // Ignition cycles since DTCs cleared
        u8 obd_monitoring_conditions_met = 0;          // OBD monitoring conditions encounter count
        dp::Vector<MonitorPerformanceRatio> ratios;   // Performance ratios for each monitor

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data;
            data.push_back(ignition_cycles);
            data.push_back(obd_monitoring_conditions_met);
            // Encode all ratios (7 bytes each)
            for (const auto &ratio : ratios) {
                auto ratio_bytes = ratio.encode();
                for (auto b : ratio_bytes)
                    data.push_back(b);
            }
            // Pad to at least 8 bytes
            while (data.size() < 8)
                data.push_back(0xFF);
            return data;
        }

        static DM20Response decode(const dp::Vector<u8> &data) {
            DM20Response resp;
            if (data.size() < 2)
                return resp;

            resp.ignition_cycles = data[0];
            resp.obd_monitoring_conditions_met = data[1];

            // Decode ratios (7 bytes each)
            usize offset = 2;
            while (offset + 7 <= data.size()) {
                resp.ratios.push_back(MonitorPerformanceRatio::decode(data.data() + offset));
                offset += 7;
            }

            return resp;
        }
    };

    // ─── Freeze Frame Data (DM25) ────────────────────────────────────────────────
    // Freeze frame captures SPN values at the time a DTC becomes active
    struct SPNSnapshot {
        u32 spn = 0;
        u32 value = 0;

        dp::Array<u8, 7> encode() const noexcept {
            dp::Array<u8, 7> bytes{};
            // SPN (19 bits) + value (32 bits)
            bytes[0] = static_cast<u8>(spn & 0xFF);
            bytes[1] = static_cast<u8>((spn >> 8) & 0xFF);
            bytes[2] = static_cast<u8>((spn >> 16) & 0x07);
            bytes[3] = static_cast<u8>(value & 0xFF);
            bytes[4] = static_cast<u8>((value >> 8) & 0xFF);
            bytes[5] = static_cast<u8>((value >> 16) & 0xFF);
            bytes[6] = static_cast<u8>((value >> 24) & 0xFF);
            return bytes;
        }

        static SPNSnapshot decode(const u8 *data) noexcept {
            SPNSnapshot snap;
            snap.spn = static_cast<u32>(data[0]) | (static_cast<u32>(data[1]) << 8) |
                       (static_cast<u32>(data[2] & 0x07) << 16);
            snap.value = static_cast<u32>(data[3]) | (static_cast<u32>(data[4]) << 8) |
                         (static_cast<u32>(data[5]) << 16) | (static_cast<u32>(data[6]) << 24);
            return snap;
        }
    };

    struct FreezeFrame {
        DTC dtc;                                // Associated DTC
        u32 timestamp_ms = 0;                   // Capture time
        dp::Vector<SPNSnapshot> snapshots;      // Captured SPN values

        // Encode freeze frame for DM25 response
        dp::Vector<u8> encode() const {
            dp::Vector<u8> data;
            // DTC (4 bytes)
            auto dtc_bytes = dtc.encode();
            for (auto b : dtc_bytes)
                data.push_back(b);
            // Timestamp (4 bytes) - optional, implementation-specific
            data.push_back(static_cast<u8>(timestamp_ms & 0xFF));
            data.push_back(static_cast<u8>((timestamp_ms >> 8) & 0xFF));
            data.push_back(static_cast<u8>((timestamp_ms >> 16) & 0xFF));
            data.push_back(static_cast<u8>((timestamp_ms >> 24) & 0xFF));
            // Number of snapshots (1 byte)
            data.push_back(static_cast<u8>(snapshots.size()));
            // Snapshots (7 bytes each)
            for (const auto &snap : snapshots) {
                auto snap_bytes = snap.encode();
                for (auto b : snap_bytes)
                    data.push_back(b);
            }
            return data;
        }

        static FreezeFrame decode(const dp::Vector<u8> &data) {
            FreezeFrame ff;
            if (data.size() < 9)
                return ff; // Not enough data

            // DTC (4 bytes)
            ff.dtc = DTC::decode(data.data());

            // Timestamp (4 bytes)
            ff.timestamp_ms = static_cast<u32>(data[4]) | (static_cast<u32>(data[5]) << 8) |
                              (static_cast<u32>(data[6]) << 16) | (static_cast<u32>(data[7]) << 24);

            // Number of snapshots (1 byte)
            u8 num_snapshots = data[8];

            // Snapshots (7 bytes each)
            usize offset = 9;
            for (u8 i = 0; i < num_snapshots && offset + 7 <= data.size(); ++i) {
                ff.snapshots.push_back(SPNSnapshot::decode(data.data() + offset));
                offset += 7;
            }

            return ff;
        }
    };

    // DM25 Expanded Freeze Frame Request/Response
    struct DM25Request {
        u32 spn = 0;     // SPN of the DTC to retrieve freeze frame for
        FMI fmi = FMI::RootCauseUnknown;
        u8 frame_number = 0; // 0 = most recent, 1 = next most recent, etc.

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = static_cast<u8>(spn & 0xFF);
            data[1] = static_cast<u8>((spn >> 8) & 0xFF);
            data[2] = static_cast<u8>((spn >> 16) & 0x07);
            data[3] = static_cast<u8>(fmi);
            data[4] = frame_number;
            return data;
        }

        static DM25Request decode(const dp::Vector<u8> &data) {
            DM25Request req;
            if (data.size() < 5)
                return req;
            req.spn = static_cast<u32>(data[0]) | (static_cast<u32>(data[1]) << 8) |
                      (static_cast<u32>(data[2] & 0x07) << 16);
            req.fmi = static_cast<FMI>(data[3]);
            req.frame_number = data[4];
            return req;
        }
    };

    // ─── Diagnostic Protocol (DM1/DM2/DM3/DM11/DM13/DM22/DM25) ──────────────────
    class DiagnosticProtocol {
        IsoNet &net_;
        InternalCF *cf_;
        dp::Vector<DTC> active_dtcs_;
        dp::Vector<DTC> previous_dtcs_;
        dp::Vector<PreviouslyActiveDTC> previously_active_dtcs_;
        DiagnosticLamps lamps_;
        u32 dm1_interval_ms_ = 1000;
        u32 dm1_timer_ms_ = 0;
        bool auto_send_ = false;
        bool dm1_suspended_ = false;
        bool dm2_suspended_ = false;
        // DM13 suspend duration tracking (ISO 11783-12 / J1939-73)
        // 0xFFFF = indefinite, otherwise duration in seconds
        u32 dm1_suspend_remaining_ms_ = 0;
        u32 dm2_suspend_remaining_ms_ = 0;
        dp::Optional<ProductIdentification> product_id_;
        dp::Optional<SoftwareIdentification> software_id_;
        DiagnosticProtocolID diag_protocol_id_; // DM5: supported protocols
        dp::Optional<AcknowledgmentHandler> ack_handler_;

        // Freeze frame support (DM25)
        dp::Map<u32, dp::Vector<FreezeFrame>> freeze_frames_; // Key: (SPN << 8) | FMI
        u8 max_freeze_frames_per_dtc_ = 3;
        bool auto_capture_freeze_frames_ = true;

        // Monitor performance ratios (DM20)
        DM20Response dm20_data_;

      public:
        DiagnosticProtocol(IsoNet &net, InternalCF *cf, DiagnosticConfig config = {})
            : net_(net),
              cf_(cf),
              dm1_interval_ms_(config.dm1_interval_ms),
              auto_send_(config.auto_send),
              max_freeze_frames_per_dtc_(config.max_freeze_frames_per_dtc),
              auto_capture_freeze_frames_(config.auto_capture_freeze_frames) {}

        Result<void> initialize() {
            if (!cf_) {
                return Result<void>::err(Error::invalid_state("control function not set"));
            }
            // Initialize acknowledgment handler
            ack_handler_.emplace(net_, cf_);
            ack_handler_->initialize();

            net_.register_pgn_callback(PGN_DM1, [this](const Message &msg) { handle_dm1(msg); });
            net_.register_pgn_callback(PGN_DM2, [this](const Message &msg) { handle_dm2(msg); });
            net_.register_pgn_callback(PGN_DM3, [this](const Message &msg) { handle_dm3_request(msg); });
            net_.register_pgn_callback(PGN_DM11, [this](const Message &msg) { handle_dm11(msg); });
            net_.register_pgn_callback(PGN_DM13, [this](const Message &msg) { handle_dm13(msg); });
            net_.register_pgn_callback(PGN_DM22, [this](const Message &msg) { handle_dm22(msg); });
            net_.register_pgn_callback(PGN_PRODUCT_IDENTIFICATION,
                                       [this](const Message &msg) { handle_product_id(msg); });
            net_.register_pgn_callback(PGN_SOFTWARE_ID, [this](const Message &msg) { handle_software_id(msg); });
            net_.register_pgn_callback(PGN_DIAGNOSTIC_PROTOCOL_ID, [this](const Message &msg) { handle_dm5(msg); });
            net_.register_pgn_callback(0xC200, [this](const Message &msg) { handle_dm20_request(msg); }); // DM20
            net_.register_pgn_callback(0xD600, [this](const Message &msg) { handle_dm25_request(msg); }); // DM25
            echo::category("isobus.diagnostic").debug("initialized (DM1/DM2/DM3/DM5/DM11/DM13/DM20/DM22/DM25)");
            return {};
        }

        // ─── Acknowledgment handler access ───────────────────────────────────────
        AcknowledgmentHandler *ack_handler() noexcept { return ack_handler_.has_value() ? &(*ack_handler_) : nullptr; }

        // ─── Send NACK for unsupported diagnostic PGN ────────────────────────────
        Result<void> nack_unsupported_pgn(PGN pgn, Address requester) {
            if (!ack_handler_.has_value()) {
                return Result<void>::err(Error::invalid_state("ack handler not initialized"));
            }
            echo::category("isobus.diagnostic").debug("NACKing unsupported PGN ", pgn, " from ", requester);
            return ack_handler_->send_nack(pgn, requester);
        }

        // ─── Previously active DTCs (with occurrence tracking) ───────────────────
        const dp::Vector<PreviouslyActiveDTC> &previously_active_dtcs() const noexcept {
            return previously_active_dtcs_;
        }

        void clear_previously_active_dtcs() {
            previously_active_dtcs_.clear();
            echo::category("isobus.diagnostic").info("previously active DTCs cleared");
        }

        // ─── DTC management ──────────────────────────────────────────────────────
        Result<void> set_active(DTC dtc) {
            bool is_new_dtc = true;
            for (auto &existing : active_dtcs_) {
                if (existing == dtc) {
                    existing.occurrence_count = (existing.occurrence_count < 126) ? existing.occurrence_count + 1 : 126;
                    is_new_dtc = false;
                    break;
                }
            }

            if (is_new_dtc) {
                dtc.occurrence_count = 1;
                active_dtcs_.push_back(dtc);
                echo::category("isobus.diagnostic").info("DTC set active: spn=", dtc.spn);

                // Auto-capture freeze frame on new DTC activation
                if (auto_capture_freeze_frames_) {
                    auto result = capture_freeze_frame(dtc, dp::Vector<SPNSnapshot>());
                    if (result.is_ok()) {
                        echo::category("isobus.diagnostic").debug("Freeze frame captured for DTC spn=", dtc.spn);
                    }
                }
            }
            return {};
        }

        Result<void> clear_active(u32 spn, FMI fmi) {
            for (auto it = active_dtcs_.begin(); it != active_dtcs_.end(); ++it) {
                if (it->spn == spn && it->fmi == fmi) {
                    previous_dtcs_.push_back(*it);
                    // Track in previously_active_dtcs_ with occurrence count
                    track_previously_active(*it);
                    active_dtcs_.erase(it);
                    echo::category("isobus.diagnostic").info("DTC cleared: spn=", spn);
                    return {};
                }
            }
            return Result<void>::err(Error::invalid_state("DTC not found"));
        }

        Result<void> clear_all_active() {
            for (auto &dtc : active_dtcs_) {
                previous_dtcs_.push_back(dtc);
                track_previously_active(dtc);
            }
            active_dtcs_.clear();
            echo::category("isobus.diagnostic").info("all active DTCs cleared");
            return {};
        }

        Result<void> clear_previous() {
            previous_dtcs_.clear();
            echo::category("isobus.diagnostic").info("previous DTCs cleared");
            return {};
        }

        dp::Vector<DTC> active_dtcs() const { return active_dtcs_; }
        dp::Vector<DTC> previous_dtcs() const { return previous_dtcs_; }

        // ─── Suspend state access ─────────────────────────────────────────────────
        bool is_dm1_suspended() const noexcept { return dm1_suspended_; }
        bool is_dm2_suspended() const noexcept { return dm2_suspended_; }

        // ─── Lamp control ────────────────────────────────────────────────────────
        void set_lamps(DiagnosticLamps lamps) { lamps_ = lamps; }
        DiagnosticLamps lamps() const noexcept { return lamps_; }

        // ─── Auto-send DM1 ──────────────────────────────────────────────────────
        Result<void> enable_auto_send(u32 interval_ms = 1000) {
            auto_send_ = true;
            dm1_interval_ms_ = interval_ms;
            echo::category("isobus.diagnostic").debug("auto-send enabled: interval=", interval_ms, "ms");
            return {};
        }

        Result<void> disable_auto_send() {
            auto_send_ = false;
            echo::category("isobus.diagnostic").debug("auto-send disabled");
            return {};
        }

        void update(u32 elapsed_ms) {
            // DM13 suspend duration tracking: auto-resume when timer expires
            if (dm1_suspended_ && dm1_suspend_remaining_ms_ > 0) {
                if (elapsed_ms >= dm1_suspend_remaining_ms_) {
                    dm1_suspend_remaining_ms_ = 0;
                    dm1_suspended_ = false;
                    echo::category("isobus.diagnostic").info("DM1 suspend duration expired, resuming");
                } else {
                    dm1_suspend_remaining_ms_ -= elapsed_ms;
                }
            }
            if (dm2_suspended_ && dm2_suspend_remaining_ms_ > 0) {
                if (elapsed_ms >= dm2_suspend_remaining_ms_) {
                    dm2_suspend_remaining_ms_ = 0;
                    dm2_suspended_ = false;
                    echo::category("isobus.diagnostic").info("DM2 suspend duration expired, resuming");
                } else {
                    dm2_suspend_remaining_ms_ -= elapsed_ms;
                }
            }

            if (auto_send_ && !dm1_suspended_) {
                dm1_timer_ms_ += elapsed_ms;
                if (dm1_timer_ms_ >= dm1_interval_ms_) {
                    dm1_timer_ms_ -= dm1_interval_ms_;
                    send_dm1();
                }
            }
        }

        // ─── Manual send ─────────────────────────────────────────────────────────
        Result<void> send_dm1() {
            auto data = encode_dtc_message(active_dtcs_);
            return net_.send(PGN_DM1, data, cf_);
        }

        Result<void> send_dm2() {
            // DM2: Include ALL previously active DTCs where occurrence_count > 0
            dp::Vector<DTC> dm2_dtcs;
            for (const auto &pa : previously_active_dtcs_) {
                if (pa.occurrence_count > 0) {
                    DTC dtc = pa.dtc;
                    dtc.occurrence_count = pa.occurrence_count;
                    dm2_dtcs.push_back(dtc);
                }
            }
            auto data = encode_dtc_message(dm2_dtcs);
            return net_.send(PGN_DM2, data, cf_);
        }

        // DM3: Clear previously active DTCs (response to DM3 request)
        Result<void> send_dm3() {
            auto data = encode_dtc_message(previous_dtcs_);
            return net_.send(PGN_DM3, data, cf_);
        }

        // DM13: Stop/Start broadcast
        Result<void> send_dm13(const DM13Signals &signals) {
            dp::Vector<u8> data(8, 0xFF);
            // Byte 1: Hold/Release/Suspend signals
            data[0] = (static_cast<u8>(signals.hold_signal) << 6) | (static_cast<u8>(signals.dm1_signal) << 4) |
                      (static_cast<u8>(signals.dm2_signal) << 2) | static_cast<u8>(signals.dm3_signal);
            // Bytes 3-4: Suspend duration
            data[2] = static_cast<u8>(signals.suspend_duration_s & 0xFF);
            data[3] = static_cast<u8>((signals.suspend_duration_s >> 8) & 0xFF);
            return net_.send(PGN_DM13, data, cf_);
        }

        // DM22: Individual clear/reset of a single DTC
        Result<void> send_dm22_clear(DM22Control control, u32 spn, FMI fmi, Address destination) {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = static_cast<u8>(control);
            data[1] = 0xFF; // reserved
            data[2] = 0xFF; // reserved
            data[3] = 0xFF; // reserved
            // SPN (19 bits) + FMI (5 bits) in bytes 5-7
            data[4] = static_cast<u8>(spn & 0xFF);
            data[5] = static_cast<u8>((spn >> 8) & 0xFF);
            data[6] = static_cast<u8>(((spn >> 16) & 0x07) << 5) | (static_cast<u8>(fmi) & 0x1F);
            data[7] = 0xFF;

            ControlFunction dest_cf;
            dest_cf.address = destination;
            return net_.send(PGN_DM22, data, cf_, &dest_cf);
        }

        // Product identification
        void set_product_id(ProductIdentification id) { product_id_ = std::move(id); }
        Result<void> send_product_id() {
            if (!product_id_)
                return Result<void>::err(Error::invalid_state("no product ID set"));
            return net_.send(PGN_PRODUCT_IDENTIFICATION, product_id_->encode(), cf_);
        }

        // Software identification
        void set_software_id(SoftwareIdentification id) { software_id_ = std::move(id); }
        Result<void> send_software_id() {
            if (!software_id_)
                return Result<void>::err(Error::invalid_state("no software ID set"));
            return net_.send(PGN_SOFTWARE_ID, software_id_->encode(), cf_);
        }

        // DM5: Diagnostic Protocol Identification
        void set_diag_protocol(DiagnosticProtocolID id) { diag_protocol_id_ = id; }
        DiagnosticProtocolID diag_protocol_id() const noexcept { return diag_protocol_id_; }
        Result<void> send_dm5() { return net_.send(PGN_DIAGNOSTIC_PROTOCOL_ID, diag_protocol_id_.encode(), cf_); }

        // Register DM5 auto-responder with PGN Request handler
        // Call after both DiagnosticProtocol and PGNRequestProtocol are initialized.
        template <typename PGNReqProto> void register_dm5_responder(PGNReqProto &pgn_req) {
            pgn_req.register_responder(PGN_DIAGNOSTIC_PROTOCOL_ID, [this]() { return diag_protocol_id_.encode(); });
        }

        // ─── Events ──────────────────────────────────────────────────────────────
        // ─── Monitor Performance Ratios (DM20) ────────────────────────────────────
        void set_ignition_cycles(u8 cycles) { dm20_data_.ignition_cycles = cycles; }
        void set_obd_conditions_met(u8 count) { dm20_data_.obd_monitoring_conditions_met = count; }

        void increment_ignition_cycles() {
            if (dm20_data_.ignition_cycles < 255)
                dm20_data_.ignition_cycles++;
        }

        void increment_obd_conditions_met() {
            if (dm20_data_.obd_monitoring_conditions_met < 255)
                dm20_data_.obd_monitoring_conditions_met++;
        }

        void set_performance_ratio(u32 spn, u16 numerator, u16 denominator) {
            // Update existing ratio or add new one
            for (auto &ratio : dm20_data_.ratios) {
                if (ratio.spn == spn) {
                    ratio.numerator = numerator;
                    ratio.denominator = denominator;
                    return;
                }
            }
            dm20_data_.ratios.push_back({spn, numerator, denominator});
        }

        void increment_monitor_execution(u32 spn) {
            for (auto &ratio : dm20_data_.ratios) {
                if (ratio.spn == spn) {
                    if (ratio.numerator < 65535)
                        ratio.numerator++;
                    return;
                }
            }
            // Add new monitor if not found
            dm20_data_.ratios.push_back({spn, 1, 0});
        }

        void increment_monitor_opportunity(u32 spn) {
            for (auto &ratio : dm20_data_.ratios) {
                if (ratio.spn == spn) {
                    if (ratio.denominator < 65535)
                        ratio.denominator++;
                    return;
                }
            }
            // Add new monitor if not found
            dm20_data_.ratios.push_back({spn, 0, 1});
        }

        dp::Optional<MonitorPerformanceRatio> get_performance_ratio(u32 spn) const {
            for (const auto &ratio : dm20_data_.ratios) {
                if (ratio.spn == spn)
                    return ratio;
            }
            return dp::nullopt;
        }

        const DM20Response &dm20_data() const noexcept { return dm20_data_; }

        void clear_performance_ratios() {
            dm20_data_.ratios.clear();
            echo::category("isobus.diagnostic").debug("Performance ratios cleared");
        }

        void reset_dm20_counters() {
            dm20_data_.ignition_cycles = 0;
            dm20_data_.obd_monitoring_conditions_met = 0;
            clear_performance_ratios();
            echo::category("isobus.diagnostic").info("DM20 counters reset");
        }

        // Send DM20 response (typically in response to request)
        Result<void> send_dm20(Address destination = GLOBAL_ADDRESS) {
            auto data = dm20_data_.encode();
            if (destination == GLOBAL_ADDRESS) {
                return net_.send(0xC200, data, cf_); // Broadcast
            } else {
                ControlFunction dest_cf;
                dest_cf.address = destination;
                return net_.send(0xC200, data, cf_, &dest_cf); // Destination-specific
            }
        }

        // ─── Freeze Frame Management (DM25) ──────────────────────────────────────
        Result<void> capture_freeze_frame(const DTC &dtc, const dp::Vector<SPNSnapshot> &snapshots, u32 timestamp_ms = 0) {
            u32 key = make_freeze_frame_key(dtc.spn, dtc.fmi);

            FreezeFrame ff;
            ff.dtc = dtc;
            ff.timestamp_ms = timestamp_ms;
            ff.snapshots = snapshots;

            // Add to storage
            auto &frames = freeze_frames_[key];
            frames.push_back(ff);

            // Limit depth per DTC
            if (frames.size() > max_freeze_frames_per_dtc_) {
                frames.erase(frames.begin()); // Remove oldest
            }

            echo::category("isobus.diagnostic").debug("Freeze frame captured: spn=", dtc.spn, " frames=", frames.size());
            return {};
        }

        dp::Optional<FreezeFrame> get_freeze_frame(u32 spn, FMI fmi, u8 frame_number = 0) const {
            u32 key = make_freeze_frame_key(spn, fmi);
            auto it = freeze_frames_.find(key);
            if (it == freeze_frames_.end() || it->second.empty())
                return dp::nullopt;

            const auto &frames = it->second;
            if (frame_number >= frames.size())
                return dp::nullopt;

            // Most recent first
            usize index = frames.size() - 1 - frame_number;
            return frames[index];
        }

        const dp::Map<u32, dp::Vector<FreezeFrame>> &freeze_frames() const noexcept { return freeze_frames_; }

        void clear_freeze_frames(u32 spn, FMI fmi) {
            u32 key = make_freeze_frame_key(spn, fmi);
            freeze_frames_.erase(key);
            echo::category("isobus.diagnostic").debug("Freeze frames cleared: spn=", spn);
        }

        void clear_all_freeze_frames() {
            freeze_frames_.clear();
            echo::category("isobus.diagnostic").info("All freeze frames cleared");
        }

        // Send DM25 response with freeze frame data
        Result<void> send_dm25_response(u32 spn, FMI fmi, u8 frame_number, Address destination) {
            auto ff = get_freeze_frame(spn, fmi, frame_number);
            if (!ff.has_value()) {
                echo::category("isobus.diagnostic").warn("Freeze frame not found: spn=", spn, " frame=", frame_number);
                return nack_unsupported_pgn(0xD600, destination);
            }

            auto data = ff.value().encode();
            ControlFunction dest_cf;
            dest_cf.address = destination;
            return net_.send(0xD600, data, cf_, &dest_cf); // DM25 PGN
        }

        // ─── Events ──────────────────────────────────────────────────────────────
        Event<const dp::Vector<DTC> &, Address> on_dm1_received;
        Event<const dp::Vector<DTC> &, Address> on_dm2_received;
        Event<const dp::Vector<DTC> &, Address> on_dm3_received; // Previously active DTCs received
        Event<Address> on_dm11_received;                         // Clear/reset all request
        Event<const DM13Signals &, Address> on_dm13_received;    // Stop/start broadcast
        Event<DM22Control, u32, FMI, Address> on_dm22_received;  // Individual DTC clear
        Event<const ProductIdentification &, Address> on_product_id_received;
        Event<const SoftwareIdentification &, Address> on_software_id_received;
        Event<const DiagnosticProtocolID &, Address> on_dm5_received;
        Event<Address> on_dm20_request;                          // Performance ratio request
        Event<const DM20Response &, Address> on_dm20_received;   // Performance ratio data received
        Event<const DM25Request &, Address> on_dm25_request;     // Freeze frame request

      private:
        void track_previously_active(const DTC &dtc) {
            for (auto &pa : previously_active_dtcs_) {
                if (pa.dtc == dtc) {
                    pa.occurrence_count = (pa.occurrence_count < 126) ? pa.occurrence_count + 1 : 126;
                    return;
                }
            }
            PreviouslyActiveDTC pa;
            pa.dtc = dtc;
            pa.occurrence_count = dtc.occurrence_count > 0 ? dtc.occurrence_count : 1;
            previously_active_dtcs_.push_back(pa);
        }

        dp::Vector<u8> encode_dtc_message(const dp::Vector<DTC> &dtcs) const {
            dp::Vector<u8> data;
            auto lamp_bytes = lamps_.encode();
            data.push_back(lamp_bytes[0]);
            data.push_back(lamp_bytes[1]);

            if (dtcs.empty()) {
                // No DTCs - send empty DTC with SPN=0, FMI=0, OC=0
                data.push_back(0x00);
                data.push_back(0x00);
                data.push_back(0x00);
                data.push_back(0x00);
            } else {
                for (const auto &dtc : dtcs) {
                    auto bytes = dtc.encode();
                    data.push_back(bytes[0]);
                    data.push_back(bytes[1]);
                    data.push_back(bytes[2]);
                    data.push_back(bytes[3]);
                }
            }

            // Pad to 8 bytes minimum (single frame); multi-DTC payloads >8 bytes
            // will be sent via TP automatically by IsoNet
            while (data.size() < 8)
                data.push_back(0xFF);
            return data;
        }

        void handle_dm1(const Message &msg) {
            if (msg.data.size() < 6)
                return;
            auto dtcs = decode_dtc_message(msg.data);
            on_dm1_received.emit(dtcs, msg.source);
        }

        void handle_dm2(const Message &msg) {
            if (msg.data.size() < 6)
                return;
            auto dtcs = decode_dtc_message(msg.data);
            on_dm2_received.emit(dtcs, msg.source);
        }

        void handle_dm3_request(const Message &msg) {
            // DM3: Clear/Reset previously active DTCs
            bool is_destination_specific = (msg.destination != BROADCAST_ADDRESS);

            // Clear previously active DTCs
            previously_active_dtcs_.clear();
            previous_dtcs_.clear();
            echo::category("isobus.diagnostic").info("DM3: previously active DTCs cleared by ", msg.source);

            if (is_destination_specific) {
                // Destination-specific DM3: send positive ACK
                if (ack_handler_.has_value()) {
                    ack_handler_->send_ack(PGN_DM3, msg.source);
                }
            }
            // Global DM3: no acknowledgment sent

            // Also notify via event (decode any DTCs included in the message)
            if (msg.data.size() >= 6) {
                auto dtcs = decode_dtc_message(msg.data);
                on_dm3_received.emit(dtcs, msg.source);
            } else {
                dp::Vector<DTC> empty_dtcs;
                on_dm3_received.emit(empty_dtcs, msg.source);
            }
        }

        void handle_dm11(const Message &msg) {
            echo::category("isobus.diagnostic").info("DM11 clear request from ", msg.source);
            on_dm11_received.emit(msg.source);
        }

        void handle_dm13(const Message &msg) {
            if (msg.data.size() < 4)
                return;
            DM13Signals signals;
            signals.dm3_signal = static_cast<DM13Command>(msg.data[0] & 0x03);
            signals.dm2_signal = static_cast<DM13Command>((msg.data[0] >> 2) & 0x03);
            signals.dm1_signal = static_cast<DM13Command>((msg.data[0] >> 4) & 0x03);
            signals.hold_signal = static_cast<DM13Command>((msg.data[0] >> 6) & 0x03);
            signals.suspend_duration_s = static_cast<u16>(msg.data[2]) | (static_cast<u16>(msg.data[3]) << 8);

            // Apply suspend/resume with duration tracking
            if (signals.dm1_signal == DM13Command::SuspendBroadcast) {
                dm1_suspended_ = true;
                // 0xFFFF = indefinite (no auto-resume), otherwise convert seconds to ms
                dm1_suspend_remaining_ms_ =
                    (signals.suspend_duration_s == 0xFFFF) ? 0 : static_cast<u32>(signals.suspend_duration_s) * 1000;
                echo::category("isobus.diagnostic")
                    .info("DM1 broadcast suspended by ", msg.source,
                          " duration=", signals.suspend_duration_s == 0xFFFF ? "indefinite" : "finite");
            } else if (signals.dm1_signal == DM13Command::ResumeBroadcast) {
                dm1_suspended_ = false;
                dm1_suspend_remaining_ms_ = 0;
                echo::category("isobus.diagnostic").info("DM1 broadcast resumed by ", msg.source);
            }
            if (signals.dm2_signal == DM13Command::SuspendBroadcast) {
                dm2_suspended_ = true;
                dm2_suspend_remaining_ms_ =
                    (signals.suspend_duration_s == 0xFFFF) ? 0 : static_cast<u32>(signals.suspend_duration_s) * 1000;
            } else if (signals.dm2_signal == DM13Command::ResumeBroadcast) {
                dm2_suspended_ = false;
                dm2_suspend_remaining_ms_ = 0;
            }

            on_dm13_received.emit(signals, msg.source);
        }

        void handle_dm22(const Message &msg) {
            if (msg.data.size() < 7)
                return;
            auto control = static_cast<DM22Control>(msg.data[0]);
            u32 spn = static_cast<u32>(msg.data[4]) | (static_cast<u32>(msg.data[5]) << 8) |
                      (static_cast<u32>((msg.data[6] >> 5) & 0x07) << 16);
            FMI fmi = static_cast<FMI>(msg.data[6] & 0x1F);

            echo::category("isobus.diagnostic")
                .debug("DM22: control=", static_cast<u8>(control), " spn=", spn, " fmi=", static_cast<u8>(fmi));

            // If this is a clear request directed at us, process it
            if (control == DM22Control::ClearActive) {
                auto result = clear_active(spn, fmi);
                DM22Control response = result.is_ok() ? DM22Control::AckClearActive : DM22Control::NackClearActive;
                send_dm22_clear(response, spn, fmi, msg.source);
            } else if (control == DM22Control::ClearPreviouslyActive) {
                bool found = false;
                for (auto it = previous_dtcs_.begin(); it != previous_dtcs_.end(); ++it) {
                    if (it->spn == spn && it->fmi == fmi) {
                        previous_dtcs_.erase(it);
                        found = true;
                        break;
                    }
                }
                DM22Control response =
                    found ? DM22Control::AckClearPreviouslyActive : DM22Control::NackClearPreviouslyActive;
                send_dm22_clear(response, spn, fmi, msg.source);
            }

            on_dm22_received.emit(control, spn, fmi, msg.source);
        }

        void handle_product_id(const Message &msg) {
            auto id = ProductIdentification::decode(msg.data);
            on_product_id_received.emit(id, msg.source);
            echo::category("isobus.diagnostic").debug("Product ID from ", msg.source, ": ", id.make, " ", id.model);
        }

        void handle_software_id(const Message &msg) {
            auto id = SoftwareIdentification::decode(msg.data);
            on_software_id_received.emit(id, msg.source);
            echo::category("isobus.diagnostic")
                .debug("Software ID from ", msg.source, ": ", id.versions.size(), " versions");
        }

        void handle_dm5(const Message &msg) {
            auto id = DiagnosticProtocolID::decode(msg.data);
            on_dm5_received.emit(id, msg.source);
            echo::category("isobus.diagnostic")
                .debug("DM5 from ", msg.source, ": protocols=0x", static_cast<int>(id.protocols));
        }

        dp::Vector<DTC> decode_dtc_message(const dp::Vector<u8> &data) const {
            dp::Vector<DTC> dtcs;
            // Skip 2 lamp bytes, then read 4-byte DTCs
            for (usize i = 2; i + 3 < data.size(); i += 4) {
                DTC dtc = DTC::decode(&data[i]);
                if (dtc.spn != 0 || static_cast<u8>(dtc.fmi) != 0) {
                    dtcs.push_back(dtc);
                }
            }
            return dtcs;
        }

        // ─── DM20 Handler ─────────────────────────────────────────────────────────
        void handle_dm20_request(const Message &msg) {
            // DM20 request is typically a PGN request (empty or minimal data)
            echo::category("isobus.diagnostic").debug("DM20 request from: ", msg.source);
            on_dm20_request.emit(msg.source);

            // Auto-respond with current performance ratios
            send_dm20(msg.source);

            // If receiving DM20 data (not request), parse it
            if (msg.data.size() >= 2) {
                DM20Response received = DM20Response::decode(msg.data);
                on_dm20_received.emit(received, msg.source);
            }
        }

        // ─── Freeze Frame Helpers ────────────────────────────────────────────────
        static u32 make_freeze_frame_key(u32 spn, FMI fmi) noexcept {
            return (spn << 8) | static_cast<u32>(fmi);
        }

        void handle_dm25_request(const Message &msg) {
            if (msg.data.size() < 5)
                return;

            DM25Request req = DM25Request::decode(msg.data);
            echo::category("isobus.diagnostic")
                .debug("DM25 request: spn=", req.spn, " fmi=", static_cast<u32>(req.fmi), " frame=", req.frame_number);

            on_dm25_request.emit(req, msg.source);

            // Auto-respond with freeze frame if available
            send_dm25_response(req.spn, req.fmi, req.frame_number, msg.source);
        }
    };
} // namespace agrobus::j1939
