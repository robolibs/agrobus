#include <doctest/doctest.h>
#include <agrobus/isobus/vt/client.hpp>
#include <agrobus/net/network_manager.hpp>
#include <agrobus/net/constants.hpp>
#include <wirebit/can/can_endpoint.hpp>
#include <wirebit/link.hpp>
#include <cstring>

using namespace agrobus::net;
using namespace agrobus::isobus;
using namespace agrobus::isobus::vt;

// Mock wirebit Link for VT testing
class VTMockLink : public wirebit::Link {
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
    wirebit::String name() const override { return "vt_mock_vcan0"; }

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

    dp::Vector<Frame> sent() const {
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

    void clear_sent() { tx_log_.clear(); }
};

struct VTTestFixture {
    std::shared_ptr<VTMockLink> link;
    wirebit::CanEndpoint* ep;
    IsoNet nm;
    InternalCF* cf;
    VTClient* vt;

    VTTestFixture() {
        link = std::make_shared<VTMockLink>();
        ep = new wirebit::CanEndpoint(link, wirebit::CanConfig{}, 1);
        nm.set_endpoint(0, ep);

        Name name;
        name.set_identity_number(99);
        cf = nm.create_internal(name, 0, 0x28).value();

        ObjectPool pool;
        VTObject ws;
        ws.id = 1000;
        ws.type = ObjectType::WorkingSet;
        ws.body = {static_cast<u8>(200 & 0xFF), static_cast<u8>((200 >> 8) & 0xFF),
                   static_cast<u8>(200 & 0xFF), static_cast<u8>((200 >> 8) & 0xFF)};
        pool.add(ws);

        VTObject mask;
        mask.id = 2000;
        mask.type = ObjectType::DataMask;
        mask.body = {static_cast<u8>(200 & 0xFF), static_cast<u8>((200 >> 8) & 0xFF),
                     static_cast<u8>(200 & 0xFF), static_cast<u8>((200 >> 8) & 0xFF)};
        pool.add(mask);

        VTObject btn;
        btn.id = 3000;
        btn.type = ObjectType::Button;
        btn.body = {static_cast<u8>(50 & 0xFF), static_cast<u8>((50 >> 8) & 0xFF),
                    static_cast<u8>(30 & 0xFF), static_cast<u8>((30 >> 8) & 0xFF)};
        pool.add(btn);

        VTObject num;
        num.id = 4000;
        num.type = ObjectType::OutputNumber;
        num.body = {static_cast<u8>(80 & 0xFF), static_cast<u8>((80 >> 8) & 0xFF),
                    static_cast<u8>(20 & 0xFF), static_cast<u8>((20 >> 8) & 0xFF)};
        pool.add(num);

        VTObject str;
        str.id = 5000;
        str.type = ObjectType::OutputString;
        str.body = {static_cast<u8>(100 & 0xFF), static_cast<u8>((100 >> 8) & 0xFF),
                    static_cast<u8>(20 & 0xFF), static_cast<u8>((20 >> 8) & 0xFF)};
        pool.add(str);

        vt = new VTClient(nm, cf);
        vt->set_object_pool(std::move(pool));
    }

    void force_connected() {
        vt->connect();

        // Inject VT status frame
        Frame vt_status;
        vt_status.id = Identifier::encode(Priority::Default, PGN_VT_TO_ECU, 0x26, 0x28);
        vt_status.data = {vt_cmd::VT_STATUS, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        vt_status.length = 8;
        link->inject(vt_status);
        nm.update();

        // Run update to transition through SendWorkingSetMaster -> SendGetMemory
        vt->update(0);
        vt->update(0);

        // Inject get memory response (enough memory) - triggers pool upload via TP
        Frame mem_response;
        mem_response.id = Identifier::encode(Priority::Default, PGN_VT_TO_ECU, 0x26, 0x28);
        mem_response.data = {vt_cmd::GET_MEMORY_RESPONSE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        mem_response.length = 8;
        link->inject(mem_response);
        nm.update();

        // Inject End of Pool response (success) to complete activation
        Frame eop_response;
        eop_response.id = Identifier::encode(Priority::Default, PGN_VT_TO_ECU, 0x26, 0x28);
        eop_response.data = {vt_cmd::END_OF_POOL, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        eop_response.length = 8;
        link->inject(eop_response);
        nm.update();

        link->clear_sent();
    }

    ~VTTestFixture() {
        delete vt;
        delete ep;
    }
};

TEST_CASE("VT commands fail when disconnected") {
    VTTestFixture fix;

    SUBCASE("hide_show fails") {
        auto result = fix.vt->hide_show(3000, false);
        CHECK(result.is_err());
    }

    SUBCASE("enable_disable fails") {
        auto result = fix.vt->enable_disable(3000, true);
        CHECK(result.is_err());
    }

    SUBCASE("change_numeric_value fails") {
        auto result = fix.vt->change_numeric_value(4000, 42);
        CHECK(result.is_err());
    }

    SUBCASE("change_string_value fails") {
        auto result = fix.vt->change_string_value(5000, "hello");
        CHECK(result.is_err());
    }

    SUBCASE("change_active_mask fails") {
        auto result = fix.vt->change_active_mask(1000, 2000);
        CHECK(result.is_err());
    }
}

TEST_CASE("VT hide_show command") {
    VTTestFixture fix;
    fix.force_connected();
    CHECK(fix.vt->state() == VTState::Connected);

    SUBCASE("hide object") {
        auto result = fix.vt->hide_show(3000, false);
        CHECK(result.is_ok());
        auto sent = fix.link->sent();
        CHECK(!sent.empty());
        auto& f = sent.back();
        CHECK(f.data[0] == vt_cmd::HIDE_SHOW);
        CHECK(f.data[1] == static_cast<u8>(3000 & 0xFF));
        CHECK(f.data[2] == static_cast<u8>((3000 >> 8) & 0xFF));
        CHECK(f.data[3] == 0); // not visible
    }

    SUBCASE("show object") {
        auto result = fix.vt->hide_show(3000, true);
        CHECK(result.is_ok());
        auto sent = fix.link->sent();
        auto& f = sent.back();
        CHECK(f.data[3] == 1); // visible
    }
}

TEST_CASE("VT enable_disable command") {
    VTTestFixture fix;
    fix.force_connected();

    SUBCASE("disable") {
        auto result = fix.vt->enable_disable(3000, false);
        CHECK(result.is_ok());
        auto sent = fix.link->sent();
        auto& f = sent.back();
        CHECK(f.data[0] == vt_cmd::ENABLE_DISABLE);
        CHECK(f.data[3] == 0); // disabled
    }

    SUBCASE("enable") {
        auto result = fix.vt->enable_disable(3000, true);
        CHECK(result.is_ok());
        auto sent = fix.link->sent();
        auto& f = sent.back();
        CHECK(f.data[3] == 1); // enabled
    }
}

TEST_CASE("VT change_numeric_value command") {
    VTTestFixture fix;
    fix.force_connected();

    SUBCASE("small value") {
        auto result = fix.vt->change_numeric_value(4000, 42);
        CHECK(result.is_ok());
        auto sent = fix.link->sent();
        auto& f = sent.back();
        CHECK(f.data[0] == vt_cmd::CHANGE_NUMERIC_VALUE);
        CHECK(f.data[1] == static_cast<u8>(4000 & 0xFF));
        CHECK(f.data[2] == static_cast<u8>((4000 >> 8) & 0xFF));
        CHECK(f.data[4] == 42); // value low byte
        CHECK(f.data[5] == 0);
        CHECK(f.data[6] == 0);
        CHECK(f.data[7] == 0);
    }

    SUBCASE("max u32 value") {
        auto result = fix.vt->change_numeric_value(4000, 0xFFFFFFFF);
        CHECK(result.is_ok());
        auto sent = fix.link->sent();
        auto& f = sent.back();
        CHECK(f.data[4] == 0xFF);
        CHECK(f.data[5] == 0xFF);
        CHECK(f.data[6] == 0xFF);
        CHECK(f.data[7] == 0xFF);
    }
}

TEST_CASE("VT change_string_value command") {
    VTTestFixture fix;
    fix.force_connected();

    auto result = fix.vt->change_string_value(5000, "Hi");
    CHECK(result.is_ok());
    auto sent = fix.link->sent();
    auto& f = sent.back();
    CHECK(f.data[0] == vt_cmd::CHANGE_STRING_VALUE);
    CHECK(f.data[1] == static_cast<u8>(5000 & 0xFF));
    CHECK(f.data[2] == static_cast<u8>((5000 >> 8) & 0xFF));
    // String length (2 bytes LE)
    CHECK(f.data[3] == 2);
    CHECK(f.data[4] == 0);
    // String data
    CHECK(f.data[5] == 'H');
    CHECK(f.data[6] == 'i');
}

TEST_CASE("VT change_active_mask command") {
    VTTestFixture fix;
    fix.force_connected();

    auto result = fix.vt->change_active_mask(1000, 2000);
    CHECK(result.is_ok());
    auto sent = fix.link->sent();
    auto& f = sent.back();
    CHECK(f.data[0] == vt_cmd::CHANGE_ACTIVE_MASK);
    CHECK(f.data[1] == static_cast<u8>(1000 & 0xFF));
    CHECK(f.data[2] == static_cast<u8>((1000 >> 8) & 0xFF));
    CHECK(f.data[3] == static_cast<u8>(2000 & 0xFF));
    CHECK(f.data[4] == static_cast<u8>((2000 >> 8) & 0xFF));
}

TEST_CASE("VT event dispatch via frames") {
    VTTestFixture fix;
    fix.force_connected();

    SUBCASE("soft key event") {
        ObjectID received_id = 0;
        ActivationCode received_code = ActivationCode::Released;
        fix.vt->on_soft_key.subscribe([&](ObjectID id, ActivationCode code) {
            received_id = id;
            received_code = code;
        });

        Frame msg;
        msg.id = Identifier::encode(Priority::Default, PGN_VT_TO_ECU, 0x26, 0x28);
        msg.data = {vt_cmd::SOFT_KEY_ACTIVATION,
                    0xE8, 0x03,  // key ID = 1000
                    static_cast<u8>(ActivationCode::Pressed),
                    0xFF, 0xFF, 0xFF, 0xFF};
        msg.length = 8;
        fix.link->inject(msg);
        fix.nm.update();

        CHECK(received_id == 1000);
        CHECK(received_code == ActivationCode::Pressed);
    }

    SUBCASE("button event") {
        ObjectID received_id = 0;
        ActivationCode received_code = ActivationCode::Released;
        fix.vt->on_button.subscribe([&](ObjectID id, ActivationCode code) {
            received_id = id;
            received_code = code;
        });

        Frame msg;
        msg.id = Identifier::encode(Priority::Default, PGN_VT_TO_ECU, 0x26, 0x28);
        msg.data = {vt_cmd::BUTTON_ACTIVATION,
                    0xB8, 0x0B,  // button ID = 3000
                    static_cast<u8>(ActivationCode::Held),
                    0xFF, 0xFF, 0xFF, 0xFF};
        msg.length = 8;
        fix.link->inject(msg);
        fix.nm.update();

        CHECK(received_id == 3000);
        CHECK(received_code == ActivationCode::Held);
    }

    SUBCASE("numeric value change event") {
        ObjectID received_id = 0;
        u32 received_value = 0;
        fix.vt->on_numeric_value_change.subscribe([&](ObjectID id, u32 val) {
            received_id = id;
            received_value = val;
        });

        Frame msg;
        msg.id = Identifier::encode(Priority::Default, PGN_VT_TO_ECU, 0x26, 0x28);
        msg.data = {vt_cmd::NUMERIC_VALUE_CHANGE,
                    0xA0, 0x0F,  // object ID = 4000
                    0x2A, 0x00, 0x00, 0x00,  // value = 42
                    0xFF};
        msg.length = 8;
        fix.link->inject(msg);
        fix.nm.update();

        CHECK(received_id == 4000);
        CHECK(received_value == 42);
    }
}
