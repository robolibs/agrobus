#pragma once

#include "commands.hpp"
#include <agrobus/net/error.hpp>
#include <agrobus/net/event.hpp>
#include <agrobus/net/internal_cf.hpp>
#include <agrobus/net/message.hpp>
#include <agrobus/net/network_manager.hpp>
#include <datapod/datapod.hpp>
#include <echo/echo.hpp>

namespace agrobus::isobus::vt {
    using namespace agrobus::net;

    // ─── VT v5 Auxiliary Channel Capability (ISO 11783-6:2018) ───────────────────
    struct AuxChannelCapability {
        u8 channel_id = 0;
        u8 aux_type = 0;      // 0=boolean, 1=analog, 2=bidirectional
        u16 resolution = 0;   // Steps for analog channels
        u8 function_type = 0; // Channel function type

        static AuxChannelCapability decode(const dp::Vector<u8> &data, usize offset) {
            AuxChannelCapability cap;
            if (offset + 5 <= data.size()) {
                cap.channel_id = data[offset];
                cap.aux_type = data[offset + 1];
                cap.resolution = static_cast<u16>(data[offset + 2]) | (static_cast<u16>(data[offset + 3]) << 8);
                cap.function_type = data[offset + 4];
            }
            return cap;
        }

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data;
            data.push_back(channel_id);
            data.push_back(aux_type);
            data.push_back(static_cast<u8>(resolution & 0xFF));
            data.push_back(static_cast<u8>((resolution >> 8) & 0xFF));
            data.push_back(function_type);
            return data;
        }
    };

    // ─── Aggregated auxiliary capabilities ───────────────────────────────────────
    struct AuxCapabilities {
        dp::Vector<AuxChannelCapability> channels;
        u8 vt_version = 0;
        bool discovery_complete = false;
    };

    // ─── Auxiliary Capability Discovery (VT v5) ──────────────────────────────────
    // Discovers what auxiliary input channels the connected VT supports,
    // their types (boolean/analog/bidirectional), and resolution.
    class AuxCapabilityDiscovery {
        IsoNet &net_;
        InternalCF *cf_;
        AuxCapabilities caps_;
        bool request_pending_ = false;

      public:
        AuxCapabilityDiscovery(IsoNet &net, InternalCF *cf) : net_(net), cf_(cf) {
            // Register callback for VT-to-ECU messages
            net_.register_pgn_callback(PGN_VT_TO_ECU, [this](const Message &msg) {
                if (msg.data.size() >= 2 && msg.data[0] == vt_cmd::GET_SUPPORTED_OBJECTS) {
                    handle_response(msg);
                }
            });
        }

        Result<void> request_capabilities() {
            if (request_pending_) {
                return Result<void>::err(Error::invalid_state("request already pending"));
            }

            // Send auxiliary capability request to VT
            // Uses Get Supported Objects command with aux-specific sub-function
            dp::Vector<u8> data(8, 0xFF);
            data[0] = vt_cmd::GET_SUPPORTED_OBJECTS;
            data[1] = 0x01; // Sub-function: auxiliary capabilities query
            data[2] = 31;   // ObjectType::AuxFunction2
            data[3] = 32;   // ObjectType::AuxInput2

            request_pending_ = true;
            echo::category("isobus.vt.aux_caps").info("Requesting auxiliary capabilities");
            return net_.send(PGN_ECU_TO_VT, data, cf_);
        }

        void handle_response(const Message &msg) {
            if (msg.data.size() < 3)
                return;

            request_pending_ = false;

            // Parse capabilities response
            // Format: [cmd][sub_func][num_channels][channel_data...]
            u8 num_channels = msg.data[2];
            caps_.channels.clear();
            caps_.vt_version = 5; // If responding to this, VT is at least v5

            usize offset = 3;
            for (u8 i = 0; i < num_channels && offset + 5 <= msg.data.size(); ++i) {
                auto cap = AuxChannelCapability::decode(msg.data, offset);
                caps_.channels.push_back(cap);
                offset += 5;
            }

            caps_.discovery_complete = true;
            echo::category("isobus.vt.aux_caps").info("Discovered ", caps_.channels.size(), " auxiliary channels");
            on_capabilities_received.emit(caps_);
        }

        const AuxCapabilities &capabilities() const noexcept { return caps_; }

        bool is_request_pending() const noexcept { return request_pending_; }

        Event<AuxCapabilities> on_capabilities_received;
    };

} // namespace agrobus::isobus::vt
