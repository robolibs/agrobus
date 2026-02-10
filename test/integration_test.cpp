#include <doctest/doctest.h>
#include <agrobus.hpp>
#include <wirebit/can/can_endpoint.hpp>
#include <wirebit/link.hpp>
#include <cstring>

using namespace agrobus::net;
using namespace agrobus::j1939;
using namespace agrobus::isobus;
using namespace agrobus::nmea;
using namespace agrobus::isobus::vt;
using namespace agrobus::isobus::tc;
using namespace agrobus::isobus::sc;
using namespace agrobus::isobus::implement;
using namespace agrobus::isobus::fs;

// Mock wirebit Link for integration testing
class IntegrationMockLink : public wirebit::Link {
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

    void inject(const Frame& frame) {
        can_frame cf = {};
        cf.can_id = frame.id.raw | CAN_EFF_FLAG;
        cf.can_dlc = frame.length;
        for (u8 i = 0; i < frame.length && i < 8; ++i)
            cf.data[i] = frame.data[i];
        wirebit::Bytes payload(sizeof(can_frame));
        std::memcpy(payload.data(), &cf, sizeof(can_frame));
        rx_queue_.push_back(wirebit::make_frame(wirebit::FrameType::CAN, std::move(payload), 0, 0));
    }

    dp::Vector<Frame> transmitted() const {
        dp::Vector<Frame> result;
        for (const auto& f : tx_log_) {
            if (f.payload.size() == sizeof(can_frame)) {
                can_frame cf;
                std::memcpy(&cf, f.payload.data(), sizeof(can_frame));
                Frame frame;
                frame.id = Identifier(cf.can_id & CAN_EFF_MASK);
                frame.length = cf.can_dlc > 8 ? 8 : cf.can_dlc;
                for (u8 i = 0; i < frame.length; ++i)
                    frame.data[i] = cf.data[i];
                for (u8 i = frame.length; i < 8; ++i)
                    frame.data[i] = 0xFF;
                result.push_back(frame);
            }
        }
        return result;
    }

    void clear_tx() { tx_log_.clear(); }
};

TEST_CASE("Full stack: IsoNet + wirebit + address claim") {
    auto link = std::make_shared<IntegrationMockLink>();
    wirebit::CanEndpoint ep(link, wirebit::CanConfig{}, 1);

    IsoNet nm;
    nm.set_endpoint(0, &ep);

    Name name;
    name.set_identity_number(1234);
    name.set_manufacturer_code(42);
    auto cf_result = nm.create_internal(name, 0, 0x28);
    CHECK(cf_result.is_ok());

    auto* cf = cf_result.value();
    CHECK(cf->address() == 0x28);

    // Start address claiming
    nm.start_address_claiming();

    // Should have transmitted a request for address claim
    CHECK(!link->transmitted().empty());
}

TEST_CASE("Full stack: IsoNet + PGN callbacks") {
    auto link = std::make_shared<IntegrationMockLink>();
    wirebit::CanEndpoint ep(link, wirebit::CanConfig{}, 1);

    IsoNet nm;
    nm.set_endpoint(0, &ep);

    Name name;
    name.set_identity_number(100);
    nm.create_internal(name, 0, 0x28);

    bool received = false;
    PGN received_pgn = 0;
    nm.on_message.subscribe([&](const Message& msg) {
        received = true;
        received_pgn = msg.pgn;
    });

    // Inject a speed message
    Frame speed_frame;
    speed_frame.id = Identifier::encode(Priority::Default, PGN_VEHICLE_SPEED, 0x30, BROADCAST_ADDRESS);
    speed_frame.data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    speed_frame.length = 8;
    link->inject(speed_frame);

    nm.update();

    CHECK(received);
    CHECK(received_pgn == PGN_VEHICLE_SPEED);
}

TEST_CASE("Full stack: IsoNet + DiagnosticProtocol") {
    auto link = std::make_shared<IntegrationMockLink>();
    wirebit::CanEndpoint ep(link, wirebit::CanConfig{}, 1);

    IsoNet nm;
    nm.set_endpoint(0, &ep);

    Name name;
    name.set_identity_number(200);
    auto* cf = nm.create_internal(name, 0, 0x28).value();

    DiagnosticProtocol diag(nm, cf);

    // Set a DTC
    DTC dtc{500, FMI::AboveNormal, 1};
    auto result = diag.set_active(dtc);
    CHECK(result.is_ok());
    CHECK(diag.active_dtcs().size() == 1);

    // Update should generate DM1 broadcast
    diag.update(1500);
}

TEST_CASE("Full stack: IsoNet + TC Client DDOP") {
    auto link = std::make_shared<IntegrationMockLink>();
    wirebit::CanEndpoint ep(link, wirebit::CanConfig{}, 1);

    IsoNet nm;
    nm.set_endpoint(0, &ep);

    Name name;
    name.set_identity_number(300);
    auto* cf = nm.create_internal(name, 0, 0x28).value();

    // Build DDOP
    tc::DDOP ddop;
    tc::DeviceObject dev;
    dev.id = 1;
    dev.designator = "TestDevice";
    dev.software_version = "1.0.0";
    dev.serial_number = "SN001";
    auto dev_result = ddop.add_device(dev);
    CHECK(dev_result.is_ok());

    tc::DeviceElement elem;
    elem.id = 2;
    elem.type = tc::DeviceElementType::Device;
    elem.number = 0;
    elem.parent_id = 1;
    elem.designator = "MainElement";
    auto elem_result = ddop.add_element(elem);
    CHECK(elem_result.is_ok());

    tc::DeviceProcessData pd;
    pd.id = 3;
    pd.ddi = 0x0001;
    pd.trigger_methods = static_cast<u8>(tc::TriggerMethod::OnChange);
    pd.designator = "Rate";
    auto pd_result = ddop.add_process_data(pd);
    CHECK(pd_result.is_ok());

    // Validate
    auto validate = ddop.validate();
    CHECK(validate.is_ok());

    // Serialize
    auto ser = ddop.serialize();
    CHECK(ser.is_ok());
    CHECK(ser.value().size() > 0);

    // Create TC client
    tc::TaskControllerClient tc_client(nm, cf);
    tc_client.set_ddop(std::move(ddop));
}

TEST_CASE("Full stack: IsoNet + VT Client pool") {
    auto link = std::make_shared<IntegrationMockLink>();
    wirebit::CanEndpoint ep(link, wirebit::CanConfig{}, 1);

    IsoNet nm;
    nm.set_endpoint(0, &ep);

    Name name;
    name.set_identity_number(400);
    auto* cf = nm.create_internal(name, 0, 0x28).value();

    // Build object pool
    vt::ObjectPool pool;

    vt::VTObject ws;
    ws.id = 1;
    ws.type = vt::ObjectType::WorkingSet;
    ws.body = {static_cast<u8>(200 & 0xFF), static_cast<u8>((200 >> 8) & 0xFF),
               static_cast<u8>(200 & 0xFF), static_cast<u8>((200 >> 8) & 0xFF)};
    pool.add(ws);

    vt::VTObject mask;
    mask.id = 2;
    mask.type = vt::ObjectType::DataMask;
    mask.body = {static_cast<u8>(200 & 0xFF), static_cast<u8>((200 >> 8) & 0xFF),
                 static_cast<u8>(200 & 0xFF), static_cast<u8>((200 >> 8) & 0xFF)};
    pool.add(mask);

    // Serialize pool
    auto ser = pool.serialize();
    CHECK(ser.is_ok());
    CHECK(ser.value().size() > 0);

    // Create VT client
    vt::VTClient vt_client(nm, cf);
    vt_client.set_object_pool(std::move(pool));

    auto connect = vt_client.connect();
    CHECK(connect.is_ok());
    CHECK(vt_client.state() == vt::VTState::WaitForVTStatus);
}

TEST_CASE("Full stack: Transport + IsoNet") {
    auto link = std::make_shared<IntegrationMockLink>();
    wirebit::CanEndpoint ep(link, wirebit::CanConfig{}, 1);

    IsoNet nm;
    nm.set_endpoint(0, &ep);

    Name name;
    name.set_identity_number(500);
    auto* cf = nm.create_internal(name, 0, 0x28).value();

    // Single frame send (<=8 bytes)
    dp::Vector<u8> small_data = {0x01, 0x02, 0x03};
    auto result = nm.send(PGN_DM1, small_data, cf);
    CHECK(result.is_ok());

    auto sent = link->transmitted();
    CHECK(!sent.empty());

    auto& frame = sent.back();
    CHECK(frame.pgn() == PGN_DM1);
    CHECK(frame.data[0] == 0x01);
    CHECK(frame.data[1] == 0x02);
    CHECK(frame.data[2] == 0x03);
    // Padding
    CHECK(frame.data[3] == 0xFF);
}

TEST_CASE("Full stack: NMEA position + coordinate transforms") {
    GNSSPosition pos;
    pos.wgs = concord::earth::WGS(48.1234, 11.5678, 520.0);
    pos.satellites_used = 14;
    pos.fix_type = GNSSFixType::RTKFixed;

    dp::Geo ref{48.1230, 11.5670, 515.0};

    auto enu = pos.to_enu(ref);
    CHECK(std::abs(enu.east()) > 1.0);
    CHECK(std::abs(enu.north()) > 1.0);

    auto ned = pos.to_ned(ref);
    CHECK(ned.north() > 0.0);

    auto ecf = pos.to_ecf();
    CHECK(ecf.x > 4.0e6);
}

TEST_CASE("Full stack: SpeedDistance + IsoNet") {
    auto link = std::make_shared<IntegrationMockLink>();
    wirebit::CanEndpoint ep(link, wirebit::CanConfig{}, 1);

    IsoNet nm;
    nm.set_endpoint(0, &ep);

    Name name;
    name.set_identity_number(600);
    auto* cf = nm.create_internal(name, 0, 0x28).value();

    SpeedDistanceInterface speed(nm, cf);
    speed.initialize();

    bool got_speed = false;
    speed.on_speed_update.subscribe([&](const SpeedData& data) {
        got_speed = true;
    });

    // Inject a wheel speed message (PGN_WHEEL_SPEED is what SpeedDistanceInterface listens for)
    Frame speed_frame;
    speed_frame.id = Identifier::encode(Priority::Default, PGN_WHEEL_SPEED, 0x30, BROADCAST_ADDRESS);
    speed_frame.data = {0xE8, 0x03, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF};
    speed_frame.length = 8;
    link->inject(speed_frame);
    nm.update();

    CHECK(got_speed);
}

TEST_CASE("Full stack: Multiple modules cooperating") {
    auto link = std::make_shared<IntegrationMockLink>();
    wirebit::CanEndpoint ep(link, wirebit::CanConfig{}, 1);

    IsoNet nm;
    nm.set_endpoint(0, &ep);

    Name name;
    name.set_identity_number(700);
    auto* cf = nm.create_internal(name, 0, 0x28).value();

    // Create diagnostics
    DiagnosticProtocol diag(nm, cf);
    diag.set_active(DTC{100, FMI::AboveNormal, 1});

    // Create speed interface
    SpeedDistanceInterface speed(nm, cf);
    speed.initialize();

    // Create VT client with pool
    vt::ObjectPool pool;
    vt::VTObject ws;
    ws.id = 1;
    ws.type = vt::ObjectType::WorkingSet;
    ws.body = {static_cast<u8>(200 & 0xFF), static_cast<u8>((200 >> 8) & 0xFF),
               static_cast<u8>(200 & 0xFF), static_cast<u8>((200 >> 8) & 0xFF)};
    pool.add(ws);
    vt::VTClient vt_client(nm, cf);
    vt_client.set_object_pool(std::move(pool));
    vt_client.connect();

    // Run update cycles
    nm.update(50);
    diag.update(50);
    vt_client.update(50);

    // All should be in valid states
    CHECK(diag.active_dtcs().size() == 1);
    CHECK(vt_client.state() == vt::VTState::WaitForVTStatus);
}
