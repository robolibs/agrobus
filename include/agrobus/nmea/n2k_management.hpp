#pragma once

#include "definitions.hpp"
#include <agrobus/net/error.hpp>
#include <agrobus/net/event.hpp>
#include <agrobus/net/internal_cf.hpp>
#include <agrobus/net/message.hpp>
#include <agrobus/net/network_manager.hpp>
#include <agrobus/net/types.hpp>
#include <datapod/datapod.hpp>
#include <echo/echo.hpp>

namespace agrobus::nmea {
    using namespace agrobus::net;

    // ─── Product Information (PGN 126996) - Fast Packet ─────────────────────────
    struct N2KProductInfo {
        u16 nmea2000_version = 0x0901; // v2.1
        u16 product_code = 0;
        dp::String model_id;         // max 32 chars
        dp::String software_version; // max 40 chars
        dp::String model_version;    // max 24 chars
        dp::String serial_code;      // max 32 chars
        u8 certification_level = 0;
        u8 load_equivalency = 1;

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data;

            // NMEA2000 version (2 bytes, little-endian)
            data.push_back(static_cast<u8>(nmea2000_version & 0xFF));
            data.push_back(static_cast<u8>((nmea2000_version >> 8) & 0xFF));

            // Product code (2 bytes, little-endian)
            data.push_back(static_cast<u8>(product_code & 0xFF));
            data.push_back(static_cast<u8>((product_code >> 8) & 0xFF));

            // Model ID - fixed 32 bytes, padded with 0xFF
            for (usize i = 0; i < 32; ++i) {
                if (i < model_id.size()) {
                    data.push_back(static_cast<u8>(model_id[i]));
                } else {
                    data.push_back(0xFF);
                }
            }

            // Software version - fixed 40 bytes, padded with 0xFF
            for (usize i = 0; i < 40; ++i) {
                if (i < software_version.size()) {
                    data.push_back(static_cast<u8>(software_version[i]));
                } else {
                    data.push_back(0xFF);
                }
            }

            // Model version - fixed 24 bytes, padded with 0xFF
            for (usize i = 0; i < 24; ++i) {
                if (i < model_version.size()) {
                    data.push_back(static_cast<u8>(model_version[i]));
                } else {
                    data.push_back(0xFF);
                }
            }

            // Serial code - fixed 32 bytes, padded with 0xFF
            for (usize i = 0; i < 32; ++i) {
                if (i < serial_code.size()) {
                    data.push_back(static_cast<u8>(serial_code[i]));
                } else {
                    data.push_back(0xFF);
                }
            }

            // Certification level (1 byte)
            data.push_back(certification_level);

            // Load equivalency (1 byte)
            data.push_back(load_equivalency);

            return data;
        }

        static Result<N2KProductInfo> decode(const dp::Vector<u8> &data) {
            // Minimum size: 2+2+32+40+24+32+1+1 = 134
            if (data.size() < 134) {
                return Result<N2KProductInfo>::err(Error(ErrorCode::InvalidData, "product info too short"));
            }

            N2KProductInfo info;

            // NMEA2000 version
            info.nmea2000_version = static_cast<u16>(data[0]) | (static_cast<u16>(data[1]) << 8);

            // Product code
            info.product_code = static_cast<u16>(data[2]) | (static_cast<u16>(data[3]) << 8);

            // Model ID (32 bytes at offset 4)
            info.model_id.clear();
            for (usize i = 0; i < 32; ++i) {
                u8 c = data[4 + i];
                if (c != 0xFF && c != 0x00) {
                    info.model_id += static_cast<char>(c);
                } else {
                    break;
                }
            }

            // Software version (40 bytes at offset 36)
            info.software_version.clear();
            for (usize i = 0; i < 40; ++i) {
                u8 c = data[36 + i];
                if (c != 0xFF && c != 0x00) {
                    info.software_version += static_cast<char>(c);
                } else {
                    break;
                }
            }

            // Model version (24 bytes at offset 76)
            info.model_version.clear();
            for (usize i = 0; i < 24; ++i) {
                u8 c = data[76 + i];
                if (c != 0xFF && c != 0x00) {
                    info.model_version += static_cast<char>(c);
                } else {
                    break;
                }
            }

            // Serial code (32 bytes at offset 100)
            info.serial_code.clear();
            for (usize i = 0; i < 32; ++i) {
                u8 c = data[100 + i];
                if (c != 0xFF && c != 0x00) {
                    info.serial_code += static_cast<char>(c);
                } else {
                    break;
                }
            }

            // Certification level (offset 132)
            info.certification_level = data[132];

            // Load equivalency (offset 133)
            info.load_equivalency = data[133];

            return Result<N2KProductInfo>::ok(std::move(info));
        }
    };

    // ─── Configuration Information (PGN 126998) - Fast Packet ───────────────────
    struct N2KConfigInfo {
        dp::String installation_desc1; // max 70 chars
        dp::String installation_desc2; // max 70 chars
        dp::String manufacturer_info;  // variable length

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data;

            // Installation description 1: length-prefixed (2 bytes LE length + data)
            u16 len1 = static_cast<u16>(installation_desc1.size());
            data.push_back(static_cast<u8>(len1 & 0xFF));
            data.push_back(static_cast<u8>((len1 >> 8) & 0xFF));
            for (usize i = 0; i < len1; ++i) {
                data.push_back(static_cast<u8>(installation_desc1[i]));
            }

            // Installation description 2: length-prefixed (2 bytes LE length + data)
            u16 len2 = static_cast<u16>(installation_desc2.size());
            data.push_back(static_cast<u8>(len2 & 0xFF));
            data.push_back(static_cast<u8>((len2 >> 8) & 0xFF));
            for (usize i = 0; i < len2; ++i) {
                data.push_back(static_cast<u8>(installation_desc2[i]));
            }

            // Manufacturer info: length-prefixed (2 bytes LE length + data)
            u16 len3 = static_cast<u16>(manufacturer_info.size());
            data.push_back(static_cast<u8>(len3 & 0xFF));
            data.push_back(static_cast<u8>((len3 >> 8) & 0xFF));
            for (usize i = 0; i < len3; ++i) {
                data.push_back(static_cast<u8>(manufacturer_info[i]));
            }

            return data;
        }

        static Result<N2KConfigInfo> decode(const dp::Vector<u8> &data) {
            if (data.size() < 6) {
                return Result<N2KConfigInfo>::err(Error(ErrorCode::InvalidData, "config info too short"));
            }

            N2KConfigInfo info;
            usize offset = 0;

            // Installation description 1
            if (offset + 2 > data.size()) {
                return Result<N2KConfigInfo>::err(Error(ErrorCode::InvalidData, "config info truncated"));
            }
            u16 len1 = static_cast<u16>(data[offset]) | (static_cast<u16>(data[offset + 1]) << 8);
            offset += 2;
            if (offset + len1 > data.size()) {
                return Result<N2KConfigInfo>::err(Error(ErrorCode::InvalidData, "desc1 truncated"));
            }
            info.installation_desc1.clear();
            for (usize i = 0; i < len1; ++i) {
                info.installation_desc1 += static_cast<char>(data[offset + i]);
            }
            offset += len1;

            // Installation description 2
            if (offset + 2 > data.size()) {
                return Result<N2KConfigInfo>::err(Error(ErrorCode::InvalidData, "config info truncated"));
            }
            u16 len2 = static_cast<u16>(data[offset]) | (static_cast<u16>(data[offset + 1]) << 8);
            offset += 2;
            if (offset + len2 > data.size()) {
                return Result<N2KConfigInfo>::err(Error(ErrorCode::InvalidData, "desc2 truncated"));
            }
            info.installation_desc2.clear();
            for (usize i = 0; i < len2; ++i) {
                info.installation_desc2 += static_cast<char>(data[offset + i]);
            }
            offset += len2;

            // Manufacturer info
            if (offset + 2 > data.size()) {
                return Result<N2KConfigInfo>::err(Error(ErrorCode::InvalidData, "config info truncated"));
            }
            u16 len3 = static_cast<u16>(data[offset]) | (static_cast<u16>(data[offset + 1]) << 8);
            offset += 2;
            if (offset + len3 > data.size()) {
                return Result<N2KConfigInfo>::err(Error(ErrorCode::InvalidData, "manufacturer info truncated"));
            }
            info.manufacturer_info.clear();
            for (usize i = 0; i < len3; ++i) {
                info.manufacturer_info += static_cast<char>(data[offset + i]);
            }

            return Result<N2KConfigInfo>::ok(std::move(info));
        }
    };

    // ─── Heartbeat (PGN 126993) - 8 bytes, broadcast ───────────────────────────
    struct N2KHeartbeat {
        u32 update_interval_ms = 60000; // Default 60s
        u8 sequence_counter = 0;
        u8 controller_class1 = 0xFF;
        u8 controller_class2 = 0xFF;

        dp::Vector<u8> encode() const {
            dp::Vector<u8> data(8, 0xFF);

            // Update interval stored as ms/50 in a u16 (bytes 0-1, little-endian)
            u16 interval_raw = static_cast<u16>(update_interval_ms / 50);
            data[0] = static_cast<u8>(interval_raw & 0xFF);
            data[1] = static_cast<u8>((interval_raw >> 8) & 0xFF);

            // Sequence counter (byte 2, lower 4 bits)
            // Upper 4 bits reserved (0xF)
            data[2] = static_cast<u8>((sequence_counter & 0x0F) | 0xF0);

            // Controller class fields (bytes 3 and 4)
            data[3] = controller_class1;
            data[4] = controller_class2;

            // Bytes 5-7 reserved (0xFF already set)

            return data;
        }

        static N2KHeartbeat decode(const dp::Vector<u8> &data) {
            N2KHeartbeat hb;

            if (data.size() >= 2) {
                u16 interval_raw = static_cast<u16>(data[0]) | (static_cast<u16>(data[1]) << 8);
                hb.update_interval_ms = static_cast<u32>(interval_raw) * 50;
            }

            if (data.size() >= 3) {
                hb.sequence_counter = data[2] & 0x0F;
            }

            if (data.size() >= 4) {
                hb.controller_class1 = data[3];
            }

            if (data.size() >= 5) {
                hb.controller_class2 = data[4];
            }

            return hb;
        }
    };

    // ─── N2K Management Configuration ──────────────────────────────────────────
    struct N2KManagementConfig {
        N2KProductInfo product_info;
        N2KConfigInfo config_info;
        u32 heartbeat_interval_ms = 60000;

        N2KManagementConfig &product(N2KProductInfo p) {
            product_info = std::move(p);
            return *this;
        }
        N2KManagementConfig &config(N2KConfigInfo c) {
            config_info = std::move(c);
            return *this;
        }
        N2KManagementConfig &heartbeat_interval(u32 ms) {
            heartbeat_interval_ms = ms;
            return *this;
        }
    };

    // ─── NMEA2000 Request Timeout (Appendix A/B) ──────────────────────────────
    inline constexpr u32 N2K_REQUEST_TIMEOUT_MS = 5000;

    // ─── Pending request tracking ──────────────────────────────────────────────
    struct PendingRequest {
        PGN pgn = 0;
        Address destination = 0xFF;
        u32 elapsed_ms = 0;
        bool active = false;
    };

    // ─── NMEA2000 Network Management handler ───────────────────────────────────
    class N2KManagement {
        IsoNet &net_;
        InternalCF *cf_;
        N2KManagementConfig config_;
        u32 heartbeat_timer_ms_ = 0;
        u8 heartbeat_seq_ = 0;
        dp::Vector<PendingRequest> pending_requests_;

      public:
        N2KManagement(IsoNet &net, InternalCF *cf, N2KManagementConfig config = {})
            : net_(net), cf_(cf), config_(std::move(config)) {}

        Result<void> initialize() {
            if (!cf_) {
                return Result<void>::err(Error::invalid_state("control function not set"));
            }

            // Register as fast packet PGNs for multi-frame transport
            net_.register_fast_packet_pgn(PGN_PRODUCT_INFO);
            net_.register_fast_packet_pgn(PGN_CONFIG_INFO);

            // Register to handle incoming product info
            net_.register_pgn_callback(PGN_PRODUCT_INFO, [this](const Message &msg) {
                clear_pending_request(PGN_PRODUCT_INFO, msg.source);
                auto result = N2KProductInfo::decode(msg.data);
                if (result.is_ok()) {
                    on_product_info_received.emit(result.value(), msg.source);
                }
            });

            // Register to handle incoming config info
            net_.register_pgn_callback(PGN_CONFIG_INFO, [this](const Message &msg) {
                clear_pending_request(PGN_CONFIG_INFO, msg.source);
                auto result = N2KConfigInfo::decode(msg.data);
                if (result.is_ok()) {
                    on_config_info_received.emit(result.value(), msg.source);
                }
            });

            // Register to handle incoming heartbeat
            net_.register_pgn_callback(PGN_HEARTBEAT_N2K, [this](const Message &msg) {
                auto hb = N2KHeartbeat::decode(msg.data);
                on_heartbeat_received.emit(hb, msg.source);
            });

            echo::category("isobus.nmea.n2k_mgmt").info("N2K management initialized");
            return {};
        }

        void update(u32 elapsed_ms) {
            heartbeat_timer_ms_ += elapsed_ms;
            if (heartbeat_timer_ms_ >= config_.heartbeat_interval_ms) {
                heartbeat_timer_ms_ -= config_.heartbeat_interval_ms;
                send_heartbeat();
            }

            // Age pending requests and check for timeouts
            for (auto it = pending_requests_.begin(); it != pending_requests_.end();) {
                if (it->active) {
                    it->elapsed_ms += elapsed_ms;
                    if (it->elapsed_ms >= N2K_REQUEST_TIMEOUT_MS) {
                        echo::category("isobus.nmea.n2k_mgmt")
                            .warn("request timeout: PGN=", it->pgn, " dest=", it->destination);
                        PGN pgn = it->pgn;
                        Address dest = it->destination;
                        it = pending_requests_.erase(it);
                        on_request_timeout.emit(pgn, dest);
                        continue;
                    }
                }
                ++it;
            }
        }

        // ─── Send responses ─────────────────────────────────────────────────────

        Result<void> send_product_info() {
            auto data = config_.product_info.encode();
            echo::category("isobus.nmea.n2k_mgmt").debug("sending product info, ", data.size(), " bytes");
            on_product_info_requested.emit(cf_->address());
            return net_.send(PGN_PRODUCT_INFO, data, cf_);
        }

        Result<void> send_config_info(Address requester) {
            auto data = config_.config_info.encode();
            ControlFunction dest_cf;
            dest_cf.address = requester;
            echo::category("isobus.nmea.n2k_mgmt").debug("sending config info to ", requester);
            on_config_info_requested.emit(requester);
            return net_.send(PGN_CONFIG_INFO, data, cf_, &dest_cf, Priority::Default);
        }

        Result<void> send_heartbeat() {
            N2KHeartbeat hb;
            hb.update_interval_ms = config_.heartbeat_interval_ms;
            hb.sequence_counter = heartbeat_seq_;
            heartbeat_seq_ = static_cast<u8>((heartbeat_seq_ + 1) & 0x0F);

            auto data = hb.encode();
            return net_.send(PGN_HEARTBEAT_N2K, data, cf_);
        }

        // ─── Destination-specific requests (NMEA2000 Appendix A/B) ─────────────

        Result<void> request_product_info(Address target) {
            if (has_pending_request_to(target)) {
                return Result<void>::err(Error::invalid_state("request already pending to destination"));
            }
            echo::category("isobus.nmea.n2k_mgmt").debug("requesting product info from ", target);

            PendingRequest req;
            req.pgn = PGN_PRODUCT_INFO;
            req.destination = target;
            req.elapsed_ms = 0;
            req.active = true;
            pending_requests_.push_back(req);

            // Send ISO request (PGN_REQUEST) for product info, destination-specific
            dp::Vector<u8> data(3, 0);
            data[0] = static_cast<u8>(PGN_PRODUCT_INFO & 0xFF);
            data[1] = static_cast<u8>((PGN_PRODUCT_INFO >> 8) & 0xFF);
            data[2] = static_cast<u8>((PGN_PRODUCT_INFO >> 16) & 0xFF);
            ControlFunction dest_cf;
            dest_cf.address = target;
            return net_.send(PGN_REQUEST, data, cf_, &dest_cf, Priority::Default);
        }

        Result<void> request_config_info(Address target) {
            if (has_pending_request_to(target)) {
                return Result<void>::err(Error::invalid_state("request already pending to destination"));
            }
            echo::category("isobus.nmea.n2k_mgmt").debug("requesting config info from ", target);

            PendingRequest req;
            req.pgn = PGN_CONFIG_INFO;
            req.destination = target;
            req.elapsed_ms = 0;
            req.active = true;
            pending_requests_.push_back(req);

            // Send ISO request (PGN_REQUEST) for config info, destination-specific
            dp::Vector<u8> data(3, 0);
            data[0] = static_cast<u8>(PGN_CONFIG_INFO & 0xFF);
            data[1] = static_cast<u8>((PGN_CONFIG_INFO >> 8) & 0xFF);
            data[2] = static_cast<u8>((PGN_CONFIG_INFO >> 16) & 0xFF);
            ControlFunction dest_cf;
            dest_cf.address = target;
            return net_.send(PGN_REQUEST, data, cf_, &dest_cf, Priority::Default);
        }

        // ─── Runtime setters ────────────────────────────────────────────────────

        void set_product_info(N2KProductInfo info) { config_.product_info = std::move(info); }

        void set_config_info(N2KConfigInfo info) { config_.config_info = std::move(info); }

        // ─── Accessors ──────────────────────────────────────────────────────────

        const N2KProductInfo &product_info() const noexcept { return config_.product_info; }
        const N2KConfigInfo &config_info() const noexcept { return config_.config_info; }
        u32 heartbeat_interval() const noexcept { return config_.heartbeat_interval_ms; }
        u8 heartbeat_sequence() const noexcept { return heartbeat_seq_; }
        const dp::Vector<PendingRequest> &pending_requests() const noexcept { return pending_requests_; }

        bool has_pending_request_to(Address dest) const noexcept {
            for (const auto &r : pending_requests_) {
                if (r.active && r.destination == dest) {
                    return true;
                }
            }
            return false;
        }

        // ─── Events ─────────────────────────────────────────────────────────────

        Event<N2KProductInfo, Address> on_product_info_received;
        Event<N2KConfigInfo, Address> on_config_info_received;
        Event<N2KHeartbeat, Address> on_heartbeat_received;
        Event<Address> on_product_info_requested;
        Event<Address> on_config_info_requested;
        Event<PGN, Address> on_request_timeout;

      private:
        void clear_pending_request(PGN pgn, Address source) {
            for (auto it = pending_requests_.begin(); it != pending_requests_.end(); ++it) {
                if (it->active && it->pgn == pgn && it->destination == source) {
                    pending_requests_.erase(it);
                    return;
                }
            }
        }
    };

} // namespace agrobus::nmea
