#include <doctest/doctest.h>
#include <agrobus/net/network_manager.hpp>
#include <agrobus/net/frame.hpp>
#include <agrobus/net/constants.hpp>
#include <wirebit/can/can_endpoint.hpp>
#include <wirebit/link.hpp>
#include <cstring>

using namespace agrobus::net;

// Mock wirebit Link for testing without hardware
class MockLink : public wirebit::Link {
    dp::Vector<wirebit::Frame> rx_queue_;
    dp::Vector<wirebit::Frame> tx_log_;

public:
    wirebit::Result<wirebit::Unit, wirebit::Error> send(const wirebit::Frame& frame) override {
        tx_log_.push_back(frame);
        return wirebit::Result<wirebit::Unit, wirebit::Error>::ok(wirebit::Unit{});
    }

    wirebit::Result<wirebit::Frame, wirebit::Error> recv() override {
        if (rx_queue_.empty())
            return wirebit::Result<wirebit::Frame, wirebit::Error>::err(
                wirebit::Error::timeout("empty"));
        wirebit::Frame f = std::move(rx_queue_[0]);
        rx_queue_.erase(rx_queue_.begin());
        return wirebit::Result<wirebit::Frame, wirebit::Error>::ok(std::move(f));
    }

    bool can_send() const override { return true; }
    bool can_recv() const override { return !rx_queue_.empty(); }
    wirebit::String name() const override { return "mock_vcan0"; }

    // Test helpers
    void inject_can(const can_frame& cf) {
        wirebit::Bytes payload(sizeof(can_frame));
        std::memcpy(payload.data(), &cf, sizeof(can_frame));
        rx_queue_.push_back(wirebit::make_frame(wirebit::FrameType::CAN, std::move(payload), 0, 0));
    }

    void inject_isobus(const Frame& frame) {
        can_frame cf = {};
        cf.can_id = frame.id.raw | CAN_EFF_FLAG;
        cf.can_dlc = frame.length;
        for (u8 i = 0; i < frame.length && i < 8; ++i)
            cf.data[i] = frame.data[i];
        inject_can(cf);
    }

    dp::Vector<can_frame> transmitted_can() const {
        dp::Vector<can_frame> result;
        for (const auto& f : tx_log_) {
            if (f.payload.size() == sizeof(can_frame)) {
                can_frame cf;
                std::memcpy(&cf, f.payload.data(), sizeof(can_frame));
                result.push_back(cf);
            }
        }
        return result;
    }

    dp::Vector<Frame> transmitted_isobus() const {
        dp::Vector<Frame> result;
        for (const auto& cf : transmitted_can()) {
            Frame frame;
            frame.id = Identifier(cf.can_id & CAN_EFF_MASK);
            frame.length = cf.can_dlc > 8 ? 8 : cf.can_dlc;
            for (u8 i = 0; i < frame.length; ++i)
                frame.data[i] = cf.data[i];
            for (u8 i = frame.length; i < 8; ++i)
                frame.data[i] = 0xFF;
            result.push_back(frame);
        }
        return result;
    }

    void clear_tx() { tx_log_.clear(); }
    usize tx_count() const { return tx_log_.size(); }
};

TEST_CASE("CanEndpoint send and recv") {
    auto link = std::make_shared<MockLink>();
    wirebit::CanEndpoint ep(link, wirebit::CanConfig{}, 1);

    const u8 data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    can_frame cf = wirebit::CanEndpoint::make_ext_frame(
        Identifier::encode(Priority::Default, PGN_VEHICLE_SPEED, 0x30, BROADCAST_ADDRESS).raw,
        data, 8);

    auto send_result = ep.send_can(cf);
    CHECK(send_result.is_ok());
    CHECK(link->tx_count() == 1);
}

TEST_CASE("CanEndpoint recv from MockLink") {
    auto link = std::make_shared<MockLink>();
    wirebit::CanEndpoint ep(link, wirebit::CanConfig{}, 1);

    Frame inject;
    inject.id = Identifier::encode(Priority::Default, PGN_VEHICLE_SPEED, 0x30, BROADCAST_ADDRESS);
    inject.data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    inject.length = 8;
    link->inject_isobus(inject);

    can_frame cf;
    auto result = ep.recv_can(cf);
    CHECK(result.is_ok());
    CHECK((cf.can_id & CAN_EFF_MASK) == inject.id.raw);
    CHECK(cf.data[0] == 0x01);
}

TEST_CASE("CanEndpoint recv when empty returns error") {
    auto link = std::make_shared<MockLink>();
    wirebit::CanEndpoint ep(link, wirebit::CanConfig{}, 1);

    can_frame cf;
    auto result = ep.recv_can(cf);
    CHECK(!result.is_ok());
}

TEST_CASE("CanEndpoint write records frames") {
    auto link = std::make_shared<MockLink>();
    wirebit::CanEndpoint ep(link, wirebit::CanConfig{}, 1);

    const u8 data1[] = {0xCA, 0xFE, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    can_frame cf1 = wirebit::CanEndpoint::make_ext_frame(
        Identifier::encode(Priority::Normal, PGN_REQUEST, 0x28, 0x30).raw,
        data1, 8);
    ep.send_can(cf1);

    const u8 data2[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    can_frame cf2 = wirebit::CanEndpoint::make_ext_frame(
        Identifier::encode(Priority::Default, PGN_DM1, 0x28, BROADCAST_ADDRESS).raw,
        data2, 8);
    ep.send_can(cf2);

    auto sent = link->transmitted_isobus();
    CHECK(sent.size() == 2);
    CHECK(sent[0].id.pgn() == PGN_REQUEST);
    CHECK(sent[1].id.pgn() == PGN_DM1);
}

TEST_CASE("CanEndpoint with IsoNet") {
    auto link = std::make_shared<MockLink>();
    wirebit::CanEndpoint ep(link, wirebit::CanConfig{}, 1);

    IsoNet nm;
    nm.set_endpoint(0, &ep);

    Name name;
    name.set_identity_number(42);
    auto cf_result = nm.create_internal(name, 0, 0x28);
    CHECK(cf_result.is_ok());
}
