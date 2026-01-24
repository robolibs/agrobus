#pragma once

#include "objects.hpp"
#include <agrobus/net/constants.hpp>
#include <agrobus/net/control_function.hpp>
#include <agrobus/net/error.hpp>
#include <agrobus/net/event.hpp>
#include <agrobus/net/internal_cf.hpp>
#include <agrobus/net/message.hpp>
#include <agrobus/net/network_manager.hpp>
#include <agrobus/net/types.hpp>
#include <datapod/datapod.hpp>
#include <echo/echo.hpp>

namespace agrobus::isobus::tc {
    using namespace agrobus::net;

    // ─── Peer Control Assignment (ISO 11783-10) ──────────────────────────────────
    // Allows a TC server to assign process data elements on one device to control
    // elements on another device (peer-to-peer control without TC involvement).

    struct PeerControlAssignment {
        ElementNumber source_element = 0;
        DDI source_ddi = 0;
        ElementNumber destination_element = 0;
        DDI destination_ddi = 0;
        Address source_address = NULL_ADDRESS;
        Address destination_address = NULL_ADDRESS;
        bool active = false;

        PeerControlAssignment &from(ElementNumber elem, DDI ddi) {
            source_element = elem;
            source_ddi = ddi;
            return *this;
        }
        PeerControlAssignment &to(ElementNumber elem, DDI ddi) {
            destination_element = elem;
            destination_ddi = ddi;
            return *this;
        }
        PeerControlAssignment &source(Address addr) {
            source_address = addr;
            return *this;
        }
        PeerControlAssignment &destination(Address addr) {
            destination_address = addr;
            return *this;
        }
    };

    class PeerControlInterface {
        IsoNet &net_;
        InternalCF *cf_;
        dp::Vector<PeerControlAssignment> assignments_;

      public:
        PeerControlInterface(IsoNet &net, InternalCF *cf) : net_(net), cf_(cf) {}

        Result<void> initialize() {
            if (!cf_) {
                return Result<void>::err(Error::invalid_state("control function not set"));
            }
            echo::category("isobus.tc.peer_control").info("Peer control interface initialized");
            return {};
        }

        // ─── Assignment management ──────────────────────────────────────────────
        Result<void> add_assignment(const PeerControlAssignment &assignment) {
            // Check for duplicate (unique by source_element + source_ddi)
            for (const auto &a : assignments_) {
                if (a.source_element == assignment.source_element && a.source_ddi == assignment.source_ddi) {
                    return Result<void>::err(Error::invalid_state("duplicate peer control assignment"));
                }
            }
            assignments_.push_back(assignment);
            on_assignment_added.emit(assignments_.back());
            echo::category("isobus.tc.peer_control")
                .debug("Assignment added: src_elem=", assignment.source_element,
                       " dst_elem=", assignment.destination_element);
            return {};
        }

        Result<void> remove_assignment(ElementNumber source_element, DDI source_ddi) {
            for (auto it = assignments_.begin(); it != assignments_.end(); ++it) {
                if (it->source_element == source_element && it->source_ddi == source_ddi) {
                    on_assignment_removed.emit(*it);
                    assignments_.erase(it);
                    return {};
                }
            }
            return Result<void>::err(Error::invalid_state("assignment not found"));
        }

        Result<void> activate_assignment(ElementNumber source_element, DDI source_ddi, bool active) {
            for (auto &a : assignments_) {
                if (a.source_element == source_element && a.source_ddi == source_ddi) {
                    a.active = active;
                    on_assignment_state_changed.emit(a);
                    return {};
                }
            }
            return Result<void>::err(Error::invalid_state("assignment not found"));
        }

        void clear_assignments() { assignments_.clear(); }

        const dp::Vector<PeerControlAssignment> &assignments() const noexcept { return assignments_; }

        // ─── Send peer control command via TC ────────────────────────────────────
        Result<void> send_assignment(const PeerControlAssignment &assignment, ControlFunction *tc_server) {
            echo::category("isobus.tc.peer_control").debug("sending assignment to TC server");
            dp::Vector<u8> data(8, 0xFF);
            // Process data command: PeerControlAssignment = 0x09
            data[0] = (0x09 & 0x0F) | ((static_cast<u8>(assignment.source_element) & 0x0F) << 4);
            data[1] = static_cast<u8>((assignment.source_element >> 4) & 0xFF);
            data[2] = static_cast<u8>(assignment.source_ddi & 0xFF);
            data[3] = static_cast<u8>((assignment.source_ddi >> 8) & 0xFF);
            data[4] = static_cast<u8>(assignment.destination_element & 0xFF);
            data[5] = static_cast<u8>((assignment.destination_element >> 8) & 0xFF);
            data[6] = static_cast<u8>(assignment.destination_ddi & 0xFF);
            data[7] = static_cast<u8>((assignment.destination_ddi >> 8) & 0xFF);

            return net_.send(PGN_ECU_TO_TC, data, cf_, tc_server, Priority::Default);
        }

        // ─── Events ──────────────────────────────────────────────────────────────
        Event<const PeerControlAssignment &> on_assignment_added;
        Event<const PeerControlAssignment &> on_assignment_removed;
        Event<const PeerControlAssignment &> on_assignment_state_changed;

        void update(u32 /*elapsed_ms*/) {}
    };

} // namespace agrobus::isobus::tc
