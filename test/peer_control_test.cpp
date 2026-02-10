#include <doctest/doctest.h>
#include <agrobus/isobus/tc/peer_control.hpp>
#include <agrobus.hpp>

using namespace agrobus::net;
using namespace agrobus::j1939;
using namespace agrobus::isobus;
using namespace agrobus::nmea;
using namespace agrobus::isobus::tc;
using namespace agrobus::isobus::vt;
using namespace agrobus::isobus::sc;
using namespace agrobus::isobus::implement;
using namespace agrobus::isobus::fs;

TEST_CASE("PeerControlInterface - initialization") {
    IsoNet nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x10).value();

    PeerControlInterface pc(nm, cf);
    pc.initialize();

    CHECK(pc.assignments().empty());
}

TEST_CASE("PeerControlInterface - add assignment") {
    IsoNet nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x10).value();

    PeerControlInterface pc(nm, cf);

    PeerControlAssignment assignment;
    assignment.source_element = 1;
    assignment.source_ddi = 100;
    assignment.destination_element = 2;
    assignment.destination_ddi = 200;
    assignment.source_address = 0x28;
    assignment.destination_address = 0x30;

    auto result = pc.add_assignment(assignment);
    CHECK(result.is_ok());
    CHECK(pc.assignments().size() == 1);
    CHECK(pc.assignments()[0].source_element == 1);
    CHECK(pc.assignments()[0].destination_element == 2);
}

TEST_CASE("PeerControlInterface - duplicate detection") {
    IsoNet nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x10).value();

    PeerControlInterface pc(nm, cf);

    PeerControlAssignment a1;
    a1.source_element = 1;
    a1.source_ddi = 100;
    a1.destination_element = 2;

    pc.add_assignment(a1);

    // Same source element + DDI = duplicate
    PeerControlAssignment a2;
    a2.source_element = 1;
    a2.source_ddi = 100;
    a2.destination_element = 3;

    auto result = pc.add_assignment(a2);
    CHECK_FALSE(result.is_ok());
    CHECK(pc.assignments().size() == 1);
}

TEST_CASE("PeerControlInterface - remove assignment") {
    IsoNet nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x10).value();

    PeerControlInterface pc(nm, cf);

    PeerControlAssignment a;
    a.source_element = 5;
    a.source_ddi = 50;
    a.destination_element = 10;
    pc.add_assignment(a);

    auto result = pc.remove_assignment(5, 50);
    CHECK(result.is_ok());
    CHECK(pc.assignments().empty());

    // Remove non-existent
    result = pc.remove_assignment(99, 99);
    CHECK_FALSE(result.is_ok());
}

TEST_CASE("PeerControlInterface - activate/deactivate") {
    IsoNet nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x10).value();

    PeerControlInterface pc(nm, cf);

    PeerControlAssignment a;
    a.source_element = 1;
    a.source_ddi = 10;
    a.destination_element = 2;
    a.active = false;
    pc.add_assignment(a);

    CHECK_FALSE(pc.assignments()[0].active);

    pc.activate_assignment(1, 10, true);
    CHECK(pc.assignments()[0].active);

    pc.activate_assignment(1, 10, false);
    CHECK_FALSE(pc.assignments()[0].active);
}

TEST_CASE("PeerControlInterface - clear assignments") {
    IsoNet nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x10).value();

    PeerControlInterface pc(nm, cf);

    for (u16 i = 0; i < 5; ++i) {
        PeerControlAssignment a;
        a.source_element = i;
        a.source_ddi = i * 10;
        a.destination_element = i + 10;
        pc.add_assignment(a);
    }
    CHECK(pc.assignments().size() == 5);

    pc.clear_assignments();
    CHECK(pc.assignments().empty());
}

TEST_CASE("PeerControlInterface - events") {
    IsoNet nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x10).value();

    PeerControlInterface pc(nm, cf);

    bool added = false;
    pc.on_assignment_added.subscribe([&](const PeerControlAssignment& a) {
        added = true;
        CHECK(a.source_element == 7);
    });

    PeerControlAssignment a;
    a.source_element = 7;
    a.source_ddi = 70;
    a.destination_element = 14;
    pc.add_assignment(a);
    CHECK(added);

    bool removed = false;
    pc.on_assignment_removed.subscribe([&](const PeerControlAssignment& a) {
        removed = true;
        CHECK(a.source_element == 7);
    });
    pc.remove_assignment(7, 70);
    CHECK(removed);
}
