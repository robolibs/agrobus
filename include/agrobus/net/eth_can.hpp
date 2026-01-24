#pragma once

#include <agrobus/net/error.hpp>
#include <agrobus/net/frame.hpp>
#include <agrobus/net/types.hpp>
#include <cstring>
#include <echo/echo.hpp>
#include <wirebit/can/can_endpoint.hpp>
#include <wirebit/eth/eth_endpoint.hpp>

namespace agrobus::net {

    // ─── ISOBUS-over-Ethernet constants (ISO 11783-13) ──────────────────────────
    // EtherType for ISOBUS Ethernet tunnel (locally assigned, non-standard)
    inline constexpr u16 ETHERTYPE_ISOBUS = 0x88F7;

    // Encapsulated CAN frame header (4 bytes ID + 1 byte DLC + up to 8 bytes data)
    inline constexpr usize ETH_CAN_HEADER_SIZE = 5; // 4 bytes CAN ID + 1 byte DLC
    inline constexpr usize ETH_CAN_MAX_FRAME = 13;  // header + 8 data bytes

    // ─── Configuration for Ethernet CAN bridge ──────────────────────────────────
    struct EthCanConfig {
        wirebit::MacAddr mac = {0x02, 0x49, 0x53, 0x4F, 0x42, 0x00}; // locally administered
        u64 bandwidth_bps = 100000000;                               // 100 Mbps
        u32 endpoint_id = 1;
        bool promiscuous = true; // receive all ISOBUS frames

        EthCanConfig &bandwidth(u64 bps) {
            bandwidth_bps = bps;
            return *this;
        }
        EthCanConfig &endpoint(u32 id) {
            endpoint_id = id;
            return *this;
        }
        EthCanConfig &promisc(bool enable) {
            promiscuous = enable;
            return *this;
        }
        EthCanConfig &mac_addr(wirebit::MacAddr addr) {
            mac = addr;
            return *this;
        }
    };

    // ─── EthCan: ISOBUS CAN frames encapsulated in Ethernet ────────────────────
    // Provides IsoNet-compatible send/recv of CAN frames over Ethernet L2.
    // This implements the concept from ISO 11783-13 where CAN frames are tunneled
    // over Ethernet for high-speed backbone connectivity.
    //
    // Each Ethernet frame carries one or more encapsulated CAN frames:
    //   [Eth Header (14B)] [CAN ID (4B)] [DLC (1B)] [Data (0-8B)] [...]
    //
    // Usage:
    //   auto link = std::make_shared<wirebit::ShmLink>(...); // or TapLink
    //   EthCan eth(link, {.mac = {...}});
    //   nm.set_endpoint(0, &eth.can_endpoint());
    //   // In main loop:
    //   eth.process(); // receive Ethernet -> CAN frames
    class EthCan {
        std::shared_ptr<wirebit::Link> link_;
        wirebit::EthEndpoint eth_ep_;
        EthCanConfig config_;

        // Internal CAN frame buffer for IsoNet compatibility
        dp::Vector<can_frame> rx_can_frames_;

        // CanEndpoint-compatible adapter
        class EthCanEndpointAdapter : public wirebit::Link {
            EthCan *parent_;

          public:
            explicit EthCanEndpointAdapter(EthCan *parent) : parent_(parent) {}

            wirebit::Result<wirebit::Unit, wirebit::Error> send(const wirebit::Frame &frame) override {
                // Extract can_frame from the wirebit frame payload
                if (frame.payload.size() != sizeof(can_frame))
                    return wirebit::Result<wirebit::Unit, wirebit::Error>::err(
                        wirebit::Error::invalid_argument("Not a CAN frame"));

                can_frame cf;
                std::memcpy(&cf, frame.payload.data(), sizeof(can_frame));
                parent_->send_can_via_eth(cf);
                return wirebit::Result<wirebit::Unit, wirebit::Error>::ok(wirebit::Unit{});
            }

            wirebit::Result<wirebit::Frame, wirebit::Error> recv() override {
                if (parent_->rx_can_frames_.empty())
                    return wirebit::Result<wirebit::Frame, wirebit::Error>::err(wirebit::Error::timeout("empty"));

                can_frame cf = parent_->rx_can_frames_[0];
                parent_->rx_can_frames_.erase(parent_->rx_can_frames_.begin());

                wirebit::Bytes payload(sizeof(can_frame));
                std::memcpy(payload.data(), &cf, sizeof(can_frame));
                return wirebit::Result<wirebit::Frame, wirebit::Error>::ok(
                    wirebit::make_frame(wirebit::FrameType::CAN, std::move(payload), 0, 0));
            }

            bool can_send() const override { return true; }
            bool can_recv() const override { return !parent_->rx_can_frames_.empty(); }
            wirebit::String name() const override { return "eth_can_adapter"; }
        };

        std::shared_ptr<EthCanEndpointAdapter> adapter_;
        wirebit::CanEndpoint can_ep_;

      public:
        EthCan(std::shared_ptr<wirebit::Link> link, const EthCanConfig &config = {})
            : link_(link),
              eth_ep_(link_,
                      wirebit::EthConfig{.bandwidth_bps = config.bandwidth_bps, .promiscuous = config.promiscuous},
                      config.endpoint_id, config.mac),
              config_(config), adapter_(std::make_shared<EthCanEndpointAdapter>(this)),
              can_ep_(std::static_pointer_cast<wirebit::Link>(adapter_), wirebit::CanConfig{.bitrate = 250000},
                      config.endpoint_id) {
            echo::category("isobus.eth_can").info("EthCan created: MAC=", wirebit::mac_to_string(config.mac).c_str());
        }

        // Get the CAN endpoint (pass to IsoNet::set_endpoint)
        wirebit::CanEndpoint &can_endpoint() noexcept { return can_ep_; }

        // Get the underlying Ethernet endpoint
        wirebit::EthEndpoint &eth_endpoint() noexcept { return eth_ep_; }

        // Process received Ethernet frames, extracting encapsulated CAN frames.
        // Call this periodically (e.g., in your main loop before nm.update()).
        void process() {
            auto result = eth_ep_.recv_eth();
            if (!result.is_ok())
                return;

            auto &eth_frame = result.value();

            // Parse Ethernet header
            wirebit::MacAddr dst_mac, src_mac;
            u16 ethertype;
            wirebit::Bytes payload;
            auto parse = wirebit::parse_eth_frame(eth_frame, dst_mac, src_mac, ethertype, payload);
            if (!parse.is_ok())
                return;

            if (ethertype != ETHERTYPE_ISOBUS)
                return;

            // Decode encapsulated CAN frames from payload
            usize offset = 0;
            while (offset + ETH_CAN_HEADER_SIZE <= payload.size()) {
                u32 can_id = static_cast<u32>(payload[offset]) | (static_cast<u32>(payload[offset + 1]) << 8) |
                             (static_cast<u32>(payload[offset + 2]) << 16) |
                             (static_cast<u32>(payload[offset + 3]) << 24);
                u8 dlc = payload[offset + 4];
                if (dlc > 8)
                    dlc = 8;
                offset += ETH_CAN_HEADER_SIZE;

                if (offset + dlc > payload.size())
                    break;

                can_frame cf = {};
                cf.can_id = can_id | CAN_EFF_FLAG;
                cf.can_dlc = dlc;
                for (u8 i = 0; i < dlc; ++i)
                    cf.data[i] = payload[offset + i];
                offset += dlc;

                rx_can_frames_.push_back(cf);
                echo::category("isobus.eth_can").trace("ETH->CAN: id=0x", can_id, " dlc=", dlc);
            }
        }

        // Send a CAN frame encapsulated in Ethernet
        void send_can_via_eth(const can_frame &cf) {
            wirebit::Bytes payload(ETH_CAN_HEADER_SIZE + cf.can_dlc);
            u32 can_id = cf.can_id & CAN_EFF_MASK;
            payload[0] = static_cast<u8>(can_id & 0xFF);
            payload[1] = static_cast<u8>((can_id >> 8) & 0xFF);
            payload[2] = static_cast<u8>((can_id >> 16) & 0xFF);
            payload[3] = static_cast<u8>((can_id >> 24) & 0xFF);
            payload[4] = cf.can_dlc;
            for (u8 i = 0; i < cf.can_dlc && i < 8; ++i)
                payload[5 + i] = cf.data[i];

            auto eth_frame = wirebit::make_eth_frame(wirebit::MAC_BROADCAST, config_.mac, ETHERTYPE_ISOBUS, payload);
            eth_ep_.send_eth(eth_frame);
            echo::category("isobus.eth_can").trace("CAN->ETH: id=0x", can_id, " dlc=", cf.can_dlc);
        }

        const wirebit::MacAddr &mac() const noexcept { return config_.mac; }
    };

} // namespace agrobus::net
