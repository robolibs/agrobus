#pragma once

#include "types.hpp"
#include <datapod/datapod.hpp>
#include <string>
#include <utility>

namespace agrobus::net {

    // ─── Error codes ─────────────────────────────────────────────────────────────
    enum class ErrorCode : u32 {
        Ok = 0,
        Timeout,
        AddressClaimFailed,
        AddressConflict,
        TransportAborted,
        TransportTimeout,
        InvalidPGN,
        InvalidAddress,
        InvalidData,
        BufferOverflow,
        NotConnected,
        InvalidState,
        PoolError,
        PoolValidation,
        SessionExists,
        NoResources,
        DriverError,
        SocketError,
        InterfaceDown,
    };

    // ─── Error type ──────────────────────────────────────────────────────────────
    struct Error : dp::Error {
        ErrorCode code = ErrorCode::Ok;

        Error() = default;
        Error(ErrorCode c, dp::String msg = "") : dp::Error{static_cast<dp::u32>(c), std::move(msg)}, code(c) {}

        static Error timeout(dp::String msg = "") noexcept { return Error(ErrorCode::Timeout, std::move(msg)); }
        static Error invalid_pgn(PGN pgn) noexcept {
            return Error(ErrorCode::InvalidPGN, "invalid PGN: " + dp::String(std::to_string(pgn)));
        }
        static Error invalid_address(Address addr) noexcept {
            return Error(ErrorCode::InvalidAddress, "invalid address: " + dp::String(std::to_string(addr)));
        }
        static Error not_connected() noexcept { return Error(ErrorCode::NotConnected, "not connected"); }
        static Error invalid_state(dp::String msg = "") noexcept {
            return Error(ErrorCode::InvalidState, std::move(msg));
        }
        static Error transport_aborted(dp::String msg = "") noexcept {
            return Error(ErrorCode::TransportAborted, std::move(msg));
        }
        static Error buffer_overflow() noexcept { return Error(ErrorCode::BufferOverflow, "buffer overflow"); }
        static Error driver_error(dp::String msg = "") noexcept {
            return Error(ErrorCode::DriverError, std::move(msg));
        }
        static Error socket_error(dp::String msg = "") noexcept {
            return Error(ErrorCode::SocketError, std::move(msg));
        }
    };

    // ─── Result alias ────────────────────────────────────────────────────────────
    template <typename T> using Result = dp::Result<T, Error>;

} // namespace agrobus::net
