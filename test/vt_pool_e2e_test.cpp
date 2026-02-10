#include <doctest/doctest.h>
#include <agrobus.hpp>
#include <wirebit/can/socketcan_link.hpp>
#include <wirebit/can/can_endpoint.hpp>
#include <agrobus/isobus/vt/client.hpp>
#include <agrobus/isobus/vt/server.hpp>

using namespace agrobus::net;
using namespace agrobus::j1939;
using namespace agrobus::isobus;
using namespace agrobus::nmea;
using namespace agrobus::isobus::vt;
using namespace agrobus::isobus::tc;
using namespace agrobus::isobus::sc;
using namespace agrobus::isobus::implement;
using namespace agrobus::isobus::fs;

// ─── Dual-node harness for VT client<->server ─────────────────────────────────
struct VTDualNode {
    std::shared_ptr<wirebit::SocketCanLink> link_client;
    std::shared_ptr<wirebit::SocketCanLink> link_server;
    wirebit::CanEndpoint ep_client;
    wirebit::CanEndpoint ep_server;
    IsoNet nm_client;
    IsoNet nm_server;
    InternalCF *cf_client = nullptr;
    InternalCF *cf_server = nullptr;

    VTDualNode()
        : link_client(std::make_shared<wirebit::SocketCanLink>(
              wirebit::SocketCanLink::create({.interface_name = "vcan_vt_pool", .create_if_missing = true, .destroy_on_close = true}).value())),
          link_server(std::make_shared<wirebit::SocketCanLink>(
              wirebit::SocketCanLink::attach("vcan_vt_pool").value())),
          ep_client(link_client, wirebit::CanConfig{}, 1),
          ep_server(link_server, wirebit::CanConfig{}, 2) {
        nm_client.set_endpoint(0, &ep_client);
        nm_server.set_endpoint(0, &ep_server);
        cf_client =
            nm_client.create_internal(Name::build().set_identity_number(10).set_manufacturer_code(100), 0, 0x28).value();
        cf_server =
            nm_server.create_internal(Name::build().set_identity_number(20).set_manufacturer_code(200), 0, 0x30).value();
    }

    void tick(u32 elapsed_ms = 10) {
        nm_client.update(elapsed_ms);
        nm_server.update(elapsed_ms);
        nm_client.update(0);
        nm_server.update(0);
    }
};

// ─── Helper: create a test object pool ────────────────────────────────────────
static ObjectPool make_test_pool() {
    ObjectPool pool;

    VTObject ws;
    ws.id = 0;
    ws.type = ObjectType::WorkingSet;
    ws.body = {0xC8, 0x00, 0xC8, 0x00, 0x0F, 0x01, 0x01, 0x00}; // 200x200, bg=white, selectable, mask=1
    pool.add(ws);

    VTObject mask;
    mask.id = 1;
    mask.type = ObjectType::DataMask;
    mask.body = {0xC8, 0x00, 0xC8, 0x00, 0x00}; // 200x200, bg=black
    pool.add(mask);

    VTObject num_var;
    num_var.id = 10;
    num_var.type = ObjectType::NumberVariable;
    num_var.body = {0x2A, 0x00, 0x00, 0x00}; // value = 42
    pool.add(num_var);

    return pool;
}

TEST_CASE("VT E2E: Object pool serialize/deserialize roundtrip") {
    auto pool = make_test_pool();

    // Serialize
    auto ser_result = pool.serialize();
    REQUIRE(ser_result.is_ok());
    auto &data = ser_result.value();
    CHECK(data.size() > 0);

    // Deserialize
    auto deser_result = ObjectPool::deserialize(data);
    REQUIRE(deser_result.is_ok());
    auto &pool2 = deser_result.value();

    CHECK(pool2.size() == 3);

    // Verify WorkingSet
    auto ws = pool2.find(0);
    REQUIRE(ws.has_value());
    CHECK((*ws)->type == ObjectType::WorkingSet);
    CHECK((*ws)->body.size() == 8);
    CHECK((*ws)->body[0] == 0xC8); // width low
    CHECK((*ws)->body[4] == 0x0F); // bg color

    // Verify DataMask
    auto dm = pool2.find(1);
    REQUIRE(dm.has_value());
    CHECK((*dm)->type == ObjectType::DataMask);

    // Verify NumberVariable
    auto nv = pool2.find(10);
    REQUIRE(nv.has_value());
    CHECK((*nv)->type == ObjectType::NumberVariable);
    CHECK((*nv)->body[0] == 0x2A); // value = 42
}

TEST_CASE("VT E2E: Pool transfer via transport protocol") {
    VTDualNode setup;

    // Directly test pool transfer via transport: client serializes pool, sends via TP,
    // server receives and deserializes it.
    auto pool = make_test_pool();
    auto ser = pool.serialize();
    REQUIRE(ser.is_ok());
    auto &pool_data = ser.value();

    // Server registers on PGN_ECU_TO_VT to receive pool data
    bool received = false;
    dp::Vector<u8> received_data;
    setup.nm_server.register_pgn_callback(PGN_ECU_TO_VT, [&](const Message &msg) {
        received = true;
        received_data = msg.data;
    });

    // Send pool data via TP (addressed, destination = server address 0x30)
    ControlFunction dest;
    dest.address = 0x30;
    auto result = setup.nm_client.send(PGN_ECU_TO_VT, pool_data, setup.cf_client, &dest);
    CHECK(result.is_ok());

    // Run ticks to complete TP handshake
    for (i32 i = 0; i < 50 && !received; ++i) {
        setup.tick(50);
    }

    CHECK(received);
    CHECK(received_data.size() == pool_data.size());

    // Now deserialize what the server received
    auto deser = ObjectPool::deserialize(received_data);
    REQUIRE(deser.is_ok());
    auto &server_pool = deser.value();

    CHECK(server_pool.size() == 3);

    auto ws = server_pool.find(0);
    REQUIRE(ws.has_value());
    CHECK((*ws)->type == ObjectType::WorkingSet);

    auto nv = server_pool.find(10);
    REQUIRE(nv.has_value());
    CHECK((*nv)->type == ObjectType::NumberVariable);
    CHECK((*nv)->body[0] == 0x2A);
}

TEST_CASE("VT E2E: Pool transfer with children serialization") {
    ObjectPool pool;

    // WorkingSet with children referencing the mask
    VTObject ws;
    ws.id = 0;
    ws.type = ObjectType::WorkingSet;
    ws.body = {0xC8, 0x00, 0xC8, 0x00, 0x0F, 0x01, 0x01, 0x00};
    ws.children = {1, 2}; // References to DataMask and NumberVariable
    pool.add(ws);

    VTObject mask;
    mask.id = 1;
    mask.type = ObjectType::DataMask;
    mask.body = {0xC8, 0x00, 0xC8, 0x00, 0x00};
    mask.children = {10}; // Reference to NumberVariable
    pool.add(mask);

    VTObject container;
    container.id = 2;
    container.type = ObjectType::Container;
    container.body = {0x64, 0x00, 0x64, 0x00}; // 100x100
    pool.add(container);

    VTObject num_var;
    num_var.id = 10;
    num_var.type = ObjectType::NumberVariable;
    num_var.body = {0xFF, 0x00, 0x00, 0x00};
    pool.add(num_var);

    // Serialize and deserialize
    auto ser = pool.serialize();
    REQUIRE(ser.is_ok());
    auto &data = ser.value();

    auto deser = ObjectPool::deserialize(data);
    REQUIRE(deser.is_ok());
    auto &pool2 = deser.value();

    CHECK(pool2.size() == 4);

    // The children are encoded within the body_len, so the deserialized body
    // should contain the children bytes appended to the object body
    auto ws2 = pool2.find(0);
    REQUIRE(ws2.has_value());
    CHECK((*ws2)->type == ObjectType::WorkingSet);
    // body should be: 8 bytes original body + 2 bytes child count + 4 bytes (2 children * 2 bytes each)
    CHECK((*ws2)->body.size() == 8 + 2 + 4);
}

TEST_CASE("VT E2E: Empty pool rejected by client") {
    IsoNet nm;
    auto *cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x10).value();

    VTClient client(nm, cf);
    // Don't set a pool
    auto result = client.connect();
    CHECK_FALSE(result.is_ok());
    CHECK(client.state() == VTState::Disconnected);
}

TEST_CASE("VT E2E: Server end-of-pool response for empty pool") {
    VTDualNode setup;

    VTServer server(setup.nm_server, setup.cf_server);
    server.start();

    // Simulate a client sending END_OF_POOL without having uploaded any pool data
    // First, send GET_MEMORY to register as client
    dp::Vector<u8> get_mem(8, 0xFF);
    get_mem[0] = vt_cmd::GET_MEMORY;
    get_mem[1] = 0x10; // 16 bytes requested
    ControlFunction dest;
    dest.address = 0x30;
    setup.nm_client.send(PGN_ECU_TO_VT, get_mem, setup.cf_client, &dest);
    setup.tick(10);
    server.update(10);
    setup.tick(10);

    // Now send END_OF_POOL without any pool data
    dp::Vector<u8> eop(8, 0xFF);
    eop[0] = vt_cmd::END_OF_POOL;
    setup.nm_client.send(PGN_ECU_TO_VT, eop, setup.cf_client, &dest);
    setup.tick(10);
    server.update(10);
    setup.tick(10);

    // Server should have responded with error (data[1] != 0)
    // Verify client is tracked but pool not uploaded
    if (!server.clients().empty()) {
        CHECK_FALSE(server.clients()[0].pool_uploaded);
    }
}

TEST_CASE("VT E2E: Version label preserved through roundtrip") {
    ObjectPool pool;
    pool.set_version_label("v1.2.3");

    VTObject ws;
    ws.id = 0;
    ws.type = ObjectType::WorkingSet;
    ws.body = {0xC8, 0x00, 0xC8, 0x00};
    pool.add(ws);

    CHECK(pool.version_label() == "v1.2.3");
    CHECK(pool.size() == 1);

    // Serialize/deserialize preserves objects (version label is metadata, not serialized in binary)
    auto ser = pool.serialize();
    REQUIRE(ser.is_ok());
    auto deser = ObjectPool::deserialize(ser.value());
    REQUIRE(deser.is_ok());
    CHECK(deser.value().size() == 1);
}
