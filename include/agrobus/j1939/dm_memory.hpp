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

namespace agrobus::j1939 {
    using namespace agrobus::net;

    // ─── DM14 Memory Access Request (PGN 0xD900) ────────────────────────────────
    // Used by service tools to request read/write/erase of ECU memory.

    enum class DM14Command : u8 {
        Read = 0,
        Write = 1,
        StatusRequest = 2,
        Erase = 3,
        BootLoad = 4,
        EdcpGeneration = 5,
        Reserved = 0xFF
    };

    enum class DM14PointerType : u8 { DirectPhysical = 0, DirectVirtual = 1, Indirect = 2, NotAvailable = 3 };

    struct DM14Request {
        DM14Command command = DM14Command::Read;
        DM14PointerType pointer_type = DM14PointerType::DirectPhysical;
        u32 address = 0; // Memory address (3 bytes)
        u16 length = 0;  // Number of bytes to access
        u8 key = 0xFF;   // Security key/seed response

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = (static_cast<u8>(command) & 0x07) | ((static_cast<u8>(pointer_type) & 0x03) << 4);
            data[1] = static_cast<u8>(length & 0xFF);
            data[2] = static_cast<u8>((length >> 8) & 0xFF);
            data[3] = static_cast<u8>(address & 0xFF);
            data[4] = static_cast<u8>((address >> 8) & 0xFF);
            data[5] = static_cast<u8>((address >> 16) & 0xFF);
            data[6] = key;
            return data;
        }

        static DM14Request decode(const dp::Vector<u8> &data) {
            DM14Request msg;
            if (data.size() >= 7) {
                msg.command = static_cast<DM14Command>(data[0] & 0x07);
                msg.pointer_type = static_cast<DM14PointerType>((data[0] >> 4) & 0x03);
                msg.length = static_cast<u16>(data[1]) | (static_cast<u16>(data[2]) << 8);
                msg.address =
                    static_cast<u32>(data[3]) | (static_cast<u32>(data[4]) << 8) | (static_cast<u32>(data[5]) << 16);
                msg.key = data[6];
            }
            return msg;
        }
    };

    // ─── DM15 Memory Access Response (PGN 0xD800) ───────────────────────────────

    enum class DM15Status : u8 { Proceed = 0, Busy = 1, Completed = 2, Error = 3, EdcpFault = 4, Reserved = 0xFF };

    struct DM15Response {
        DM15Status status = DM15Status::Proceed;
        u16 length = 0;           // Number of bytes
        u32 address = 0;          // Echo of requested address
        u8 edcp_extension = 0xFF; // Error-Detecting Code Parameter
        u8 seed = 0xFF;           // Security seed

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);
            data[0] = static_cast<u8>(status) & 0x07;
            data[1] = static_cast<u8>(length & 0xFF);
            data[2] = static_cast<u8>((length >> 8) & 0xFF);
            data[3] = static_cast<u8>(address & 0xFF);
            data[4] = static_cast<u8>((address >> 8) & 0xFF);
            data[5] = static_cast<u8>((address >> 16) & 0xFF);
            data[6] = edcp_extension;
            data[7] = seed;
            return data;
        }

        static DM15Response decode(const dp::Vector<u8> &data) {
            DM15Response msg;
            if (data.size() >= 7) {
                msg.status = static_cast<DM15Status>(data[0] & 0x07);
                msg.length = static_cast<u16>(data[1]) | (static_cast<u16>(data[2]) << 8);
                msg.address =
                    static_cast<u32>(data[3]) | (static_cast<u32>(data[4]) << 8) | (static_cast<u32>(data[5]) << 16);
                msg.edcp_extension = data[6];
                if (data.size() >= 8) {
                    msg.seed = data[7];
                }
            }
            return msg;
        }
    };

    // ─── DM16 Binary Data Transfer (PGN 0xD700) ─────────────────────────────────
    // Carries the actual data bytes for memory read/write operations.
    // May use transport protocol if data exceeds 8 bytes.

    struct DM16Transfer {
        u8 num_bytes = 0;    // Number of data bytes in this message
        dp::Vector<u8> data; // Binary data (up to 7 bytes single-frame, more via TP)

        dp::Vector<u8> encode() const {
            dp::Vector<u8> encoded(8, 0xFF);
            encoded[0] = num_bytes;
            for (usize i = 0; i < data.size() && i < 7; ++i) {
                encoded[1 + i] = data[i];
            }
            return encoded;
        }

        static DM16Transfer decode(const dp::Vector<u8> &raw) {
            DM16Transfer msg;
            if (!raw.empty()) {
                msg.num_bytes = raw[0];
                for (usize i = 1; i < raw.size() && i <= msg.num_bytes; ++i) {
                    msg.data.push_back(raw[i]);
                }
            }
            return msg;
        }
    };

    // ─── ECU Identification (PGN 0xFDC5) ────────────────────────────────────────
    // Multi-frame message with '*'-delimited fields.

    struct ECUIdentification {
        dp::String ecu_part_number;
        dp::String ecu_serial_number;
        dp::String ecu_location;
        dp::String ecu_type;
        dp::String ecu_manufacturer;

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data;
            auto append_field = [&data](const dp::String &field) {
                for (char c : field)
                    data.push_back(static_cast<u8>(c));
                data.push_back('*');
            };
            append_field(ecu_part_number);
            append_field(ecu_serial_number);
            append_field(ecu_location);
            append_field(ecu_type);
            append_field(ecu_manufacturer);
            return data;
        }

        static ECUIdentification decode(const dp::Vector<u8> &data) {
            ECUIdentification id;
            usize field = 0;
            dp::String current;
            for (u8 b : data) {
                if (b == '*') {
                    switch (field) {
                    case 0:
                        id.ecu_part_number = std::move(current);
                        break;
                    case 1:
                        id.ecu_serial_number = std::move(current);
                        break;
                    case 2:
                        id.ecu_location = std::move(current);
                        break;
                    case 3:
                        id.ecu_type = std::move(current);
                        break;
                    case 4:
                        id.ecu_manufacturer = std::move(current);
                        break;
                    default:
                        break;
                    }
                    current.clear();
                    ++field;
                } else {
                    current += static_cast<char>(b);
                }
            }
            return id;
        }
    };

    // ─── Memory Access Protocol Handler ──────────────────────────────────────────
    class MemoryAccessHandler {
        IsoNet &net_;
        InternalCF *cf_;
        dp::Optional<ECUIdentification> ecu_id_;

      public:
        MemoryAccessHandler(IsoNet &net, InternalCF *cf) : net_(net), cf_(cf) {}

        Result<void> initialize() {
            if (!cf_) {
                return Result<void>::err(Error::invalid_state("control function not set"));
            }
            net_.register_pgn_callback(PGN_DM14, [this](const Message &msg) {
                on_dm14_request.emit(DM14Request::decode(msg.data), msg.source);
            });
            net_.register_pgn_callback(PGN_DM15, [this](const Message &msg) {
                on_dm15_response.emit(DM15Response::decode(msg.data), msg.source);
            });
            net_.register_pgn_callback(PGN_DM16, [this](const Message &msg) {
                on_dm16_transfer.emit(DM16Transfer::decode(msg.data), msg.source);
            });
            net_.register_pgn_callback(PGN_ECU_IDENTIFICATION, [this](const Message &msg) {
                on_ecu_identification.emit(ECUIdentification::decode(msg.data), msg.source);
            });
            echo::category("isobus.protocol.dm_memory").debug("initialized");
            return {};
        }

        // Send DM14 memory access request
        Result<void> send_request(const DM14Request &req, Address destination) {
            ControlFunction dest_cf;
            dest_cf.address = destination;
            return net_.send(PGN_DM14, req.encode(), cf_, &dest_cf, Priority::Default);
        }

        // Send DM15 memory access response
        Result<void> send_response(const DM15Response &resp, Address destination) {
            ControlFunction dest_cf;
            dest_cf.address = destination;
            return net_.send(PGN_DM15, resp.encode(), cf_, &dest_cf, Priority::Default);
        }

        // Send DM16 binary data
        Result<void> send_data(const DM16Transfer &transfer, Address destination) {
            ControlFunction dest_cf;
            dest_cf.address = destination;
            return net_.send(PGN_DM16, transfer.encode(), cf_, &dest_cf, Priority::Default);
        }

        // ECU Identification
        void set_ecu_id(ECUIdentification id) { ecu_id_ = std::move(id); }
        Result<void> send_ecu_id() {
            if (!ecu_id_) {
                return Result<void>::err(Error::invalid_state("no ECU ID set"));
            }
            return net_.send(PGN_ECU_IDENTIFICATION, ecu_id_->encode(), cf_);
        }

        // Events
        Event<DM14Request, Address> on_dm14_request;
        Event<DM15Response, Address> on_dm15_response;
        Event<DM16Transfer, Address> on_dm16_transfer;
        Event<ECUIdentification, Address> on_ecu_identification;
    };
} // namespace agrobus::j1939
