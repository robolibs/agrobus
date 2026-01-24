#pragma once

#include "../core/error.hpp"
#include "../core/frame.hpp"
#include "../core/message.hpp"
#include "../transport/etp.hpp"
#include "../transport/fast_packet.hpp"
#include "../transport/tp.hpp"
#include "../util/event.hpp"
#include "address_claimer.hpp"
#include "bus_load.hpp"
#include "control_function.hpp"
#include "internal_cf.hpp"
#include "partner_cf.hpp"
#include <datapod/datapod.hpp>
#include <echo/echo.hpp>
#include <wirebit/can/can_endpoint.hpp>

namespace isobus {
    namespace network {

        // ─── Network configuration ──────────────────────────────────────────────────
        struct NetworkConfig {
            u8 num_ports = 1;
            u32 address_claim_timeout_ms = ADDRESS_CLAIM_TIMEOUT_MS;
            bool enable_bus_load = true;
            bool enable_fast_packet = false; // Enable NMEA2000 fast packet for known PGNs

            // Fluent API
            NetworkConfig &ports(u8 n) {
                num_ports = n;
                return *this;
            }
            NetworkConfig &claim_timeout(u32 ms) {
                address_claim_timeout_ms = ms;
                return *this;
            }
            NetworkConfig &bus_load(bool enable) {
                enable_bus_load = enable;
                return *this;
            }
            NetworkConfig &fast_packet(bool enable) {
                enable_fast_packet = enable;
                return *this;
            }
        };

        // ─── Main network manager ────────────────────────────────────────────────────
        class NetworkManager {
            NetworkConfig config_;
            dp::Vector<InternalCF> internal_cfs_;
            dp::Vector<PartnerCF> partner_cfs_;
            dp::Vector<AddressClaimer> claimers_;
            dp::Map<u8, wirebit::CanEndpoint *> endpoints_;
            dp::Map<u8, BusLoad> bus_loads_;

            // Transport protocol instances
            TransportProtocol tp_;
            ExtendedTransportProtocol etp_;
            FastPacketProtocol fast_packet_;

            // PGN callback registry
            dp::Map<PGN, dp::Vector<std::function<void(const Message &)>>> pgn_callbacks_;

            // Fast packet PGNs (NMEA2000 PGNs that use fast packet)
            dp::Vector<PGN> fast_packet_pgns_;

          public:
            explicit NetworkManager(NetworkConfig config = {}) : config_(config) {
                for (u8 i = 0; i < config_.num_ports; ++i) {
                    if (config_.enable_bus_load) {
                        bus_loads_[i] = BusLoad{};
                    }
                }

                // Subscribe to transport completion events
                tp_.on_complete.subscribe([this](TransportSession &session) { handle_transport_complete(session); });
                etp_.on_complete.subscribe([this](TransportSession &session) { handle_transport_complete(session); });
            }

            // ─── Device management ───────────────────────────────────────────────────
            Result<InternalCF *> create_internal(Name name, u8 port, Address preferred = NULL_ADDRESS) {
                internal_cfs_.emplace_back(name, port, preferred);
                auto *cf = &internal_cfs_.back();
                claimers_.emplace_back(cf, config_.address_claim_timeout_ms);
                echo::category("isobus.network").info("Internal CF created on port ", port);
                return Result<InternalCF *>::ok(cf);
            }

            Result<PartnerCF *> create_partner(u8 port, dp::Vector<NameFilter> filters) {
                partner_cfs_.emplace_back(port, std::move(filters));
                auto *cf = &partner_cfs_.back();
                echo::category("isobus.network").info("Partner CF created on port ", port);
                return Result<PartnerCF *>::ok(cf);
            }

            // ─── Endpoint registration ────────────────────────────────────────────────
            Result<void> set_endpoint(u8 port, wirebit::CanEndpoint *ep) {
                if (!ep) {
                    return Result<void>::err(Error::invalid_state("null endpoint"));
                }
                endpoints_[port] = ep;
                echo::category("isobus.network").debug("endpoint set on port ", port);
                return {};
            }

            // ─── PGN callback registration ──────────────────────────────────────────
            Result<void> register_pgn_callback(PGN pgn, std::function<void(const Message &)> callback) {
                if (!callback) {
                    return Result<void>::err(Error::invalid_state("null callback"));
                }
                pgn_callbacks_[pgn].push_back(std::move(callback));
                return {};
            }

            // ─── Fast packet PGN registration ─────────────────────────────────────────
            // Register PGNs that should use NMEA2000 fast packet protocol for multi-frame
            Result<void> register_fast_packet_pgn(PGN pgn) {
                for (auto p : fast_packet_pgns_) {
                    if (p == pgn)
                        return {};
                }
                fast_packet_pgns_.push_back(pgn);
                return {};
            }

            // ─── Message sending (auto-selects transport) ──────────────────────────────
            Result<void> send(PGN pgn, const dp::Vector<u8> &data, InternalCF *source, ControlFunction *dest = nullptr,
                              Priority priority = Priority::Default) {
                if (!source || !source->cf().address_valid()) {
                    return Result<void>::err(Error::not_connected());
                }

                Address src_addr = source->address();
                Address dst_addr = dest ? dest->address : BROADCAST_ADDRESS;

                if (data.size() <= CAN_DATA_LENGTH) {
                    return send_single_frame(pgn, data, src_addr, dst_addr, priority);
                }

                // Check if this PGN should use fast packet (NMEA2000)
                if (is_fast_packet_pgn(pgn) && data.size() <= FAST_PACKET_MAX_DATA) {
                    auto result = fast_packet_.send(pgn, data, src_addr);
                    if (!result.is_ok()) {
                        return Result<void>::err(result.error());
                    }
                    return send_frames(result.value(), source->port());
                }

                // Use TP for 9-1785 bytes
                if (data.size() <= TP_MAX_DATA_LENGTH) {
                    auto result = tp_.send(pgn, data, src_addr, dst_addr, source->port(), priority);
                    if (!result.is_ok()) {
                        return Result<void>::err(result.error());
                    }
                    return send_frames(result.value(), source->port());
                }

                // Use ETP for >1785 bytes (connection-mode only, no broadcast)
                if (dst_addr == BROADCAST_ADDRESS) {
                    return Result<void>::err(Error::invalid_state("ETP does not support broadcast"));
                }
                auto result = etp_.send(pgn, data, src_addr, dst_addr, source->port(), priority);
                if (!result.is_ok()) {
                    return Result<void>::err(result.error());
                }
                return send_frames(result.value(), source->port());
            }

            Result<void> send_frame(const Frame &frame) { return send_frame(frame, 0); }

            Result<void> send_frame(const Frame &frame, u8 port) {
                auto it = endpoints_.find(port);
                if (it == endpoints_.end() || !it->second) {
                    return Result<void>::err(Error::not_connected());
                }
                can_frame cf = to_can_frame(frame);
                auto result = it->second->send_can(cf);
                if (result.is_ok()) {
                    if (config_.enable_bus_load) {
                        bus_loads_[port].add_frame(frame.length);
                    }
                    return {};
                }
                return Result<void>::err(Error(ErrorCode::DriverError, "send_can failed"));
            }

            // ─── Main update loop ────────────────────────────────────────────────────
            void update(u32 elapsed_ms = 0) {
                // Read from all endpoints
                for (auto &[port, ep] : endpoints_) {
                    if (!ep)
                        continue;

                    while (true) {
                        can_frame cf;
                        auto result = ep->recv_can(cf);
                        if (!result.is_ok())
                            break;

                        Frame frame = from_can_frame(cf);
                        process_frame(frame, port);

                        if (config_.enable_bus_load) {
                            bus_loads_[port].add_frame(frame.length);
                        }
                    }
                }

                // Update transport protocols and send any generated frames
                {
                    auto tp_frames = tp_.update(elapsed_ms);
                    send_transport_frames(tp_frames);

                    auto tp_data_frames = tp_.get_pending_data_frames();
                    send_transport_frames(tp_data_frames);

                    auto etp_frames = etp_.update(elapsed_ms);
                    send_transport_frames(etp_frames);

                    auto etp_data_frames = etp_.get_pending_data_frames();
                    send_transport_frames(etp_data_frames);

                    fast_packet_.update(elapsed_ms);
                }

                // Update address claimers
                for (auto &claimer : claimers_) {
                    auto frames = claimer.update(elapsed_ms);
                    for (const auto &f : frames) {
                        send_frame(f);
                    }
                }

                // Update bus load
                if (config_.enable_bus_load) {
                    for (auto &[port, bl] : bus_loads_) {
                        bl.update(elapsed_ms);
                    }
                }
            }

            // ─── Start address claiming ──────────────────────────────────────────────
            Result<void> start_address_claiming() {
                if (claimers_.empty()) {
                    return Result<void>::err(Error::invalid_state("no control functions registered"));
                }
                for (auto &claimer : claimers_) {
                    auto frames = claimer.start();
                    for (const auto &f : frames) {
                        send_frame(f);
                    }
                }
                echo::category("isobus.network").debug("address claiming started");
                return {};
            }

            // ─── Transport access (for advanced usage) ────────────────────────────────
            TransportProtocol &transport_protocol() noexcept { return tp_; }
            ExtendedTransportProtocol &extended_transport_protocol() noexcept { return etp_; }
            FastPacketProtocol &fast_packet_protocol() noexcept { return fast_packet_; }

            // ─── Diagnostics ─────────────────────────────────────────────────────────
            f32 bus_load(u8 port) const noexcept {
                auto it = bus_loads_.find(port);
                if (it != bus_loads_.end())
                    return it->second.load_percent();
                return 0.0f;
            }

            dp::Vector<ControlFunction *> control_functions() {
                dp::Vector<ControlFunction *> cfs;
                for (auto &icf : internal_cfs_)
                    cfs.push_back(&icf.cf());
                for (auto &pcf : partner_cfs_)
                    cfs.push_back(&pcf.cf());
                return cfs;
            }

            dp::Vector<InternalCF> &internal_cfs() noexcept { return internal_cfs_; }
            dp::Vector<PartnerCF> &partner_cfs() noexcept { return partner_cfs_; }

            // ─── Test injection ──────────────────────────────────────────────────────
            // Inject a message directly into the PGN callback dispatch (for unit testing)
            void inject_message(const Message &msg) { dispatch_message(msg); }

            // ─── Events ──────────────────────────────────────────────────────────────
            Event<const Message &> on_message;
            Event<ControlFunction *, CFState> on_cf_state_change;
            Event<Address> on_address_violation; // Emitted when another device uses our claimed address

          private:
            Result<void> send_single_frame(PGN pgn, const dp::Vector<u8> &data, Address src, Address dst,
                                           Priority prio) {
                dp::Array<u8, 8> frame_data = {};
                for (usize i = 0; i < data.size() && i < 8; ++i) {
                    frame_data[i] = data[i];
                }
                for (usize i = data.size(); i < 8; ++i) {
                    frame_data[i] = 0xFF;
                }

                Frame frame;
                frame.id = Identifier::encode(prio, pgn, src, dst);
                frame.data = frame_data;
                frame.length = 8;

                return send_frame(frame);
            }

            Result<void> send_frames(const dp::Vector<Frame> &frames, u8 port = 0) {
                for (const auto &f : frames) {
                    auto result = send_frame(f, port);
                    if (!result.is_ok()) {
                        return result;
                    }
                }
                return {};
            }

            void send_frames_best_effort(const dp::Vector<Frame> &frames, u8 port = 0) {
                for (const auto &f : frames) {
                    send_frame(f, port);
                }
            }

            bool is_fast_packet_pgn(PGN pgn) const noexcept {
                if (!config_.enable_fast_packet)
                    return false;
                for (auto p : fast_packet_pgns_) {
                    if (p == pgn)
                        return true;
                }
                return false;
            }

            void process_frame(const Frame &frame, u8 port) {
                PGN pgn = frame.pgn();

                // Check for address claim messages
                if (pgn == PGN_ADDRESS_CLAIMED) {
                    handle_address_claim(frame, port);
                    return;
                }

                // Address violation detection (ISO 11783-5 Section 4.4.4.3):
                // If we receive a non-address-claim message from our own claimed address,
                // re-assert our claim and emit a violation event.
                check_address_violation(frame, port);

                // Route transport protocol frames
                if (pgn == PGN_TP_CM || pgn == PGN_TP_DT) {
                    auto responses = tp_.process_frame(frame, port);
                    send_frames_best_effort(responses, port);
                    return;
                }

                if (pgn == PGN_ETP_CM || pgn == PGN_ETP_DT) {
                    auto responses = etp_.process_frame(frame, port);
                    send_frames_best_effort(responses, port);
                    return;
                }

                // Check if this is a fast packet PGN
                if (is_fast_packet_pgn(pgn)) {
                    auto msg = fast_packet_.process_frame(frame);
                    if (msg.has_value()) {
                        dispatch_message(msg.value());
                    }
                    return;
                }

                // Convert single-frame to message and dispatch
                Message msg;
                msg.pgn = pgn;
                msg.source = frame.source();
                msg.destination = frame.destination();
                msg.priority = frame.priority();
                msg.timestamp_us = frame.timestamp_us;
                msg.data.assign(frame.data.begin(), frame.data.begin() + frame.length);

                dispatch_message(msg);
            }

            void handle_transport_complete(TransportSession &session) {
                if (session.direction != TransportDirection::Receive) {
                    return; // Only dispatch received messages
                }

                Message msg;
                msg.pgn = session.pgn;
                msg.source = session.source_address;
                msg.destination = session.destination_address;
                msg.priority = session.priority;
                msg.data = std::move(session.data);

                echo::category("isobus.network").debug("Transport complete: pgn=", msg.pgn, " bytes=", msg.data.size());

                dispatch_message(msg);
            }

            // Send transport-generated frames, routing to the correct port
            void send_transport_frames(const dp::Vector<Frame> &frames) {
                for (const auto &f : frames) {
                    u8 port = port_for_address(f.source());
                    send_frame(f, port);
                }
            }

            // Find the port for a given source address (from our internal CFs)
            u8 port_for_address(Address addr) const {
                for (const auto &icf : internal_cfs_) {
                    if (icf.address() == addr) {
                        return icf.port();
                    }
                }
                return 0;
            }

            void check_address_violation(const Frame &frame, u8 port) {
                Address src = frame.source();
                if (src == NULL_ADDRESS || src == BROADCAST_ADDRESS)
                    return;

                for (usize i = 0; i < internal_cfs_.size(); ++i) {
                    if (internal_cfs_[i].port() == port && internal_cfs_[i].claim_state() == ClaimState::Claimed &&
                        internal_cfs_[i].address() == src) {
                        // Another device is using our claimed address - re-assert
                        echo::category("isobus.network").warn("address violation detected: SA=", src);
                        auto frames = claimers_[i].handle_request_for_claim();
                        for (const auto &f : frames) {
                            send_frame(f, port);
                        }
                        on_address_violation.emit(src);
                        return;
                    }
                }
            }

            void handle_address_claim(const Frame &frame, u8 port) {
                Name claimed_name = Name::from_bytes(frame.data.data());
                Address claimed_addr = frame.source();

                echo::category("isobus.network.claim")
                    .debug("Address claim received: addr=", claimed_addr, " name=", claimed_name.raw);

                // Notify our address claimers
                for (usize i = 0; i < claimers_.size(); ++i) {
                    if (internal_cfs_[i].port() == port) {
                        auto frames = claimers_[i].handle_claim(claimed_addr, claimed_name);
                        for (const auto &f : frames) {
                            send_frame(f, port);
                        }
                    }
                }

                // Check partner matching
                for (auto &partner : partner_cfs_) {
                    if (partner.port() == port && partner.matches_name(claimed_name)) {
                        partner.set_name(claimed_name);
                        partner.set_address(claimed_addr);
                        partner.set_state(CFState::Online);
                        partner.on_partner_found.emit(claimed_addr);
                        on_cf_state_change.emit(&partner.cf(), CFState::Online);
                    }
                }
            }

            void dispatch_message(const Message &msg) {
                on_message.emit(msg);

                auto it = pgn_callbacks_.find(msg.pgn);
                if (it != pgn_callbacks_.end()) {
                    for (auto &cb : it->second) {
                        cb(msg);
                    }
                }
            }

            // ─── Frame conversion helpers ─────────────────────────────────────────────
            static can_frame to_can_frame(const Frame &frame) {
                can_frame cf = {};
                cf.can_id = frame.id.raw | CAN_EFF_FLAG;
                cf.can_dlc = frame.length;
                for (u8 i = 0; i < frame.length && i < 8; ++i) {
                    cf.data[i] = frame.data[i];
                }
                return cf;
            }

            static Frame from_can_frame(const can_frame &cf) {
                Frame frame;
                frame.id = Identifier(cf.can_id & CAN_EFF_MASK);
                frame.length = cf.can_dlc > 8 ? 8 : cf.can_dlc;
                for (u8 i = 0; i < frame.length; ++i) {
                    frame.data[i] = cf.data[i];
                }
                for (u8 i = frame.length; i < 8; ++i) {
                    frame.data[i] = 0xFF;
                }
                return frame;
            }
        };

    } // namespace network
    using namespace network;
} // namespace isobus
