#include <doctest/doctest.h>
#include <agrobus.hpp>
#include <wirebit/can/socketcan_link.hpp>
#include <wirebit/can/can_endpoint.hpp>
#include <agrobus/nmea/n2k_management.hpp>

using namespace agrobus::net;
using namespace agrobus::j1939;
using namespace agrobus::isobus;
using namespace agrobus::nmea;
using namespace agrobus::isobus::vt;
using namespace agrobus::isobus::tc;
using namespace agrobus::isobus::sc;
using namespace agrobus::isobus::implement;
using namespace agrobus::isobus::fs;

// Helper: create a frame that looks like a product info response (fast packet first frame)
static Frame make_product_info_response(Address source, Address dest) {
    // Product info is a fast packet PGN. For the purposes of this test,
    // we simulate a single-frame response (the NM callback is triggered
    // by the PGN dispatch regardless of how the data arrived).
    N2KProductInfo info;
    info.product_code = 42;
    info.model_id = "TestModel";
    info.software_version = "1.0.0";
    info.model_version = "A";
    info.serial_code = "SN001";
    auto encoded = info.encode();

    // Build a frame with the PGN_PRODUCT_INFO; for testing we send as single frame
    // (in reality this would be fast packet, but we just need the PGN callback to fire)
    dp::Array<u8, 8> data = {};
    for (u8 i = 0; i < 8 && i < encoded.size(); ++i) {
        data[i] = encoded[i];
    }
    return Frame(Identifier::encode(Priority::Default, PGN_PRODUCT_INFO, source, dest), data);
}

static Frame make_config_info_response(Address source, Address dest) {
    N2KConfigInfo info;
    info.installation_desc1 = "Desc1";
    info.installation_desc2 = "Desc2";
    info.manufacturer_info = "Mfr";
    auto encoded = info.encode();

    dp::Array<u8, 8> data = {};
    for (u8 i = 0; i < 8 && i < encoded.size(); ++i) {
        data[i] = encoded[i];
    }
    return Frame(Identifier::encode(Priority::Default, PGN_CONFIG_INFO, source, dest), data);
}

struct N2KSetup {
    std::shared_ptr<wirebit::SocketCanLink> dut_link;
    std::shared_ptr<wirebit::SocketCanLink> harness_link;
    wirebit::CanEndpoint dut_ep;
    wirebit::CanEndpoint harness_ep;
    IsoNet nm;
    InternalCF *cf = nullptr;
    N2KManagement *mgmt = nullptr;

    N2KSetup()
        : dut_link(std::make_shared<wirebit::SocketCanLink>(
              wirebit::SocketCanLink::create({.interface_name = "vcan_n2k_req", .create_if_missing = true, .destroy_on_close = true}).value())),
          harness_link(std::make_shared<wirebit::SocketCanLink>(
              wirebit::SocketCanLink::attach("vcan_n2k_req").value())),
          dut_ep(dut_link, wirebit::CanConfig{}, 1),
          harness_ep(harness_link, wirebit::CanConfig{}, 2),
          nm(NetworkConfig{}.fast_packet(false)) {
        nm.set_endpoint(0, &dut_ep);
        cf = nm.create_internal(Name::build().set_identity_number(1).set_manufacturer_code(100), 0, 0x28).value();
        mgmt = new N2KManagement(nm, cf);
        mgmt->initialize();
    }

    ~N2KSetup() { delete mgmt; }

    void inject_product_info_from(Address source) {
        Frame f = make_product_info_response(source, 0x28);
        can_frame cf = wirebit::CanEndpoint::make_ext_frame(f.id.raw, f.data.data(), f.length);
        harness_ep.send_can(cf);
        nm.update(0);
    }

    void inject_config_info_from(Address source) {
        Frame f = make_config_info_response(source, 0x28);
        can_frame cf = wirebit::CanEndpoint::make_ext_frame(f.id.raw, f.data.data(), f.length);
        harness_ep.send_can(cf);
        nm.update(0);
    }
};

TEST_CASE("N2KManagement - request sequencing: single request to destination succeeds") {
    N2KSetup setup;

    auto result = setup.mgmt->request_product_info(0x30);
    CHECK(result.is_ok());
    CHECK(setup.mgmt->pending_requests().size() == 1);
    CHECK(setup.mgmt->has_pending_request_to(0x30));
}

TEST_CASE("N2KManagement - request sequencing: duplicate request to same destination blocked") {
    N2KSetup setup;

    auto r1 = setup.mgmt->request_product_info(0x30);
    CHECK(r1.is_ok());

    // Second request to same destination should fail
    auto r2 = setup.mgmt->request_config_info(0x30);
    CHECK(r2.is_err());
    CHECK(r2.error().code == ErrorCode::InvalidState);
}

TEST_CASE("N2KManagement - request sequencing: request to different destination allowed") {
    N2KSetup setup;

    auto r1 = setup.mgmt->request_product_info(0x30);
    CHECK(r1.is_ok());

    // Request to a different destination should succeed
    auto r2 = setup.mgmt->request_product_info(0x40);
    CHECK(r2.is_ok());

    CHECK(setup.mgmt->pending_requests().size() == 2);
    CHECK(setup.mgmt->has_pending_request_to(0x30));
    CHECK(setup.mgmt->has_pending_request_to(0x40));
}

TEST_CASE("N2KManagement - request sequencing: response clears pending") {
    N2KSetup setup;

    auto r1 = setup.mgmt->request_product_info(0x30);
    CHECK(r1.is_ok());
    CHECK(setup.mgmt->has_pending_request_to(0x30));

    // Inject product info response from address 0x30
    setup.inject_product_info_from(0x30);

    // Pending should be cleared
    CHECK_FALSE(setup.mgmt->has_pending_request_to(0x30));
    CHECK(setup.mgmt->pending_requests().empty());

    // New request to same destination should now succeed
    auto r2 = setup.mgmt->request_config_info(0x30);
    CHECK(r2.is_ok());
}

TEST_CASE("N2KManagement - request sequencing: timeout clears pending after 5s") {
    N2KSetup setup;

    auto r1 = setup.mgmt->request_product_info(0x30);
    CHECK(r1.is_ok());
    CHECK(setup.mgmt->has_pending_request_to(0x30));

    // Advance time but not enough for timeout
    setup.mgmt->update(2500);
    CHECK(setup.mgmt->has_pending_request_to(0x30));

    // Still under threshold
    setup.mgmt->update(2499);
    CHECK(setup.mgmt->has_pending_request_to(0x30));

    // Now exceed the 5000ms threshold
    setup.mgmt->update(2);
    CHECK_FALSE(setup.mgmt->has_pending_request_to(0x30));
    CHECK(setup.mgmt->pending_requests().empty());
}

TEST_CASE("N2KManagement - request sequencing: on_request_timeout event fires") {
    N2KSetup setup;

    bool timeout_fired = false;
    PGN timed_out_pgn = 0;
    Address timed_out_dest = 0;

    setup.mgmt->on_request_timeout.subscribe([&](PGN pgn, Address dest) {
        timeout_fired = true;
        timed_out_pgn = pgn;
        timed_out_dest = dest;
    });

    auto r1 = setup.mgmt->request_product_info(0x30);
    CHECK(r1.is_ok());

    // Advance past timeout
    setup.mgmt->update(5001);

    CHECK(timeout_fired);
    CHECK(timed_out_pgn == PGN_PRODUCT_INFO);
    CHECK(timed_out_dest == 0x30);

    // After timeout, new request should succeed
    auto r2 = setup.mgmt->request_product_info(0x30);
    CHECK(r2.is_ok());
}
