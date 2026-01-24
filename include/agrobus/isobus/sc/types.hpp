#pragma once

#include <agrobus/net/pgn_defs.hpp>
#include <agrobus/net/types.hpp>
#include <datapod/datapod.hpp>

namespace agrobus::isobus::sc {
    using namespace agrobus::net;

    // ─── ISO 11783-14 Message Codes (Annex F) ────────────────────────────────────
    inline constexpr u8 SC_MSG_CODE_MASTER = 0x95; // SCMasterStatus message code
    inline constexpr u8 SC_MSG_CODE_CLIENT = 0x96; // SCClientStatus message code

    // ─── ISO 11783-14 Timeouts (Annex F) ─────────────────────────────────────────
    inline constexpr u32 SC_STATUS_TIMEOUT_ACTIVE_MS = 600; // During Recording/PlayBack/Abort
    inline constexpr u32 SC_STATUS_TIMEOUT_READY_MS = 3000; // During Ready state
    inline constexpr u32 SC_STATUS_MIN_SPACING_MS = 100;    // Minimum between status messages
    inline constexpr u32 SC_STATUS_ACTIVE_RATE_MS = 200;    // 5 times per second during active states

    // ─── Sequence Control states ───────────────────────────────────────────────────
    // Internal unified state for the library (maps to ISO byte 2 + byte 4)
    enum class SCState : u8 { Idle = 0, Ready = 1, Active = 2, Paused = 3, Complete = 4, Error = 5 };

    // ─── ISO 11783-14 Master State (byte 2 of SCMasterStatus, F.2) ───────────────
    enum class SCMasterState : u8 { Inactive = 0, Active = 1, Initialization = 2 };

    // ─── ISO 11783-14 Client State (byte 2 of SCClientStatus, F.3) ───────────────
    enum class SCClientState : u8 { Disabled = 0, Enabled = 1, Initialization = 2 };

    // ─── ISO 11783-14 Sequence State (byte 4 of status messages, F.2/F.3) ────────
    enum class SCSequenceState : u8 {
        Reserved = 0,
        Ready = 1,
        Recording = 2,
        RecordingCompletion = 3,
        PlayBack = 4,
        Abort = 5
    };

    // ─── ISO 11783-14 ClientFunctionErrorState (byte 5 of SCClientStatus) ────────
    enum class SCClientFuncError : u8 {
        NoErrors = 0,
        NoChange = 1,    // Error state didn't change since last report
        Changed = 2,     // Error state changed since last report
        NeedsConfirm = 3 // Operator confirmation required
    };

    // ─── Sequence Control commands ─────────────────────────────────────────────────
    enum class SCCommand : u8 { Start = 0, Pause = 1, Resume = 2, Abort = 3 };

    // ─── A single step in a sequence ───────────────────────────────────────────────
    struct SequenceStep {
        u16 step_id = 0;
        dp::String description;
        u32 duration_ms = 0;
        bool completed = false;
    };

    // ─── Master configuration ──────────────────────────────────────────────────────
    struct SCMasterConfig {
        u32 ready_timeout_ms = 3000;
        u32 active_timeout_ms = 600;
        u32 status_interval_ms = 100;

        SCMasterConfig &ready_timeout(u32 ms) {
            ready_timeout_ms = ms;
            return *this;
        }
        SCMasterConfig &active_timeout(u32 ms) {
            active_timeout_ms = ms;
            return *this;
        }
        SCMasterConfig &status_interval(u32 ms) {
            status_interval_ms = ms;
            return *this;
        }
    };

    // ─── Client configuration ──────────────────────────────────────────────────────
    struct SCClientConfig {
        u32 min_status_spacing_ms = 100;
        u32 busy_pause_timeout_ms = 600;

        SCClientConfig &min_spacing(u32 ms) {
            min_status_spacing_ms = ms;
            return *this;
        }
        SCClientConfig &busy_timeout(u32 ms) {
            busy_pause_timeout_ms = ms;
            return *this;
        }
    };
} // namespace agrobus::isobus::sc
