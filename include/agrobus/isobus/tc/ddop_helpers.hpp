#pragma once

#include "ddi_database.hpp"
#include "ddop.hpp"
#include "objects.hpp"
#include <agrobus/net/types.hpp>
#include <datapod/datapod.hpp>
#include <echo/echo.hpp>

namespace agrobus::isobus::tc {
    using namespace agrobus::net;

    // ─── Section geometry info ────────────────────────────────────────────────────
    struct SectionInfo {
        ObjectID element_id = 0;
        ElementNumber number = 0;
        dp::String designator;
        i32 offset_x_mm = 0; // X offset from connector (positive = forward)
        i32 offset_y_mm = 0; // Y offset from center (positive = left)
        i32 width_mm = 0;    // Section width in mm
    };

    // ─── Implement geometry ───────────────────────────────────────────────────────
    struct ImplementGeometry {
        i32 connector_x_mm = 0;   // Connector pivot to reference point X
        i32 boom_offset_x_mm = 0; // Boom X offset from connector
        i32 boom_offset_y_mm = 0; // Boom Y offset from connector
        i32 total_width_mm = 0;   // Total working width
        dp::Vector<SectionInfo> sections;
    };

    // ─── Rate DDI info ────────────────────────────────────────────────────────────
    struct RateInfo {
        ObjectID process_data_id = 0;
        DDI ddi = 0;
        dp::String designator;
        u8 trigger_methods = 0;
        ObjectID presentation_id = 0xFFFF;
    };

    // ─── DDOP Helpers: extract implement geometry, sections, rates from DDOP ──────
    class DDOPHelpers {
      public:
        // Extract implement geometry from a DDOP
        static ImplementGeometry extract_geometry(const DDOP &ddop) {
            ImplementGeometry geo;

            // Find connector element and its offsets
            for (const auto &elem : ddop.elements()) {
                if (elem.type == DeviceElementType::Connector) {
                    extract_offsets(ddop, elem, geo.connector_x_mm, geo.boom_offset_y_mm);
                    break;
                }
            }

            // Find function element (boom) and its offset
            for (const auto &elem : ddop.elements()) {
                if (elem.type == DeviceElementType::Function) {
                    i32 bx = 0, by = 0;
                    extract_offsets(ddop, elem, bx, by);
                    geo.boom_offset_x_mm = bx;
                    geo.boom_offset_y_mm = by;
                    break;
                }
            }

            // Extract sections
            for (const auto &elem : ddop.elements()) {
                if (elem.type == DeviceElementType::Section) {
                    SectionInfo section;
                    section.element_id = elem.id;
                    section.number = elem.number;
                    section.designator = elem.designator;
                    extract_offsets(ddop, elem, section.offset_x_mm, section.offset_y_mm);

                    // Find width property
                    for (auto child_id : elem.child_objects) {
                        for (const auto &prop : ddop.properties()) {
                            if (prop.id == child_id && prop.ddi == ddi::ACTUAL_WORKING_WIDTH) {
                                section.width_mm = prop.value;
                            }
                        }
                        for (const auto &prop : ddop.properties()) {
                            if (prop.id == child_id && prop.ddi == ddi::MAXIMUM_WORKING_WIDTH) {
                                if (section.width_mm == 0)
                                    section.width_mm = prop.value;
                            }
                        }
                    }

                    geo.sections.push_back(std::move(section));
                }
            }

            // Calculate total width
            for (const auto &sec : geo.sections) {
                geo.total_width_mm += sec.width_mm;
            }

            echo::category("isobus.tc.helpers")
                .debug("geometry: ", geo.sections.size(), " sections, width=", geo.total_width_mm, "mm");
            return geo;
        }

        // Extract all rate-related process data DDIs from a DDOP
        static dp::Vector<RateInfo> extract_rates(const DDOP &ddop) {
            dp::Vector<RateInfo> rates;

            for (const auto &pd : ddop.process_data()) {
                if (DDIDatabase::is_rate_ddi(pd.ddi)) {
                    RateInfo info;
                    info.process_data_id = pd.id;
                    info.ddi = pd.ddi;
                    info.designator = pd.designator;
                    info.trigger_methods = pd.trigger_methods;
                    info.presentation_id = pd.presentation_object_id;
                    rates.push_back(std::move(info));
                }
            }

            echo::category("isobus.tc.helpers").debug("found ", rates.size(), " rate DDIs");
            return rates;
        }

        // Extract total/accumulator DDIs from a DDOP
        static dp::Vector<RateInfo> extract_totals(const DDOP &ddop) {
            dp::Vector<RateInfo> totals;

            for (const auto &pd : ddop.process_data()) {
                if (DDIDatabase::is_total_ddi(pd.ddi)) {
                    RateInfo info;
                    info.process_data_id = pd.id;
                    info.ddi = pd.ddi;
                    info.designator = pd.designator;
                    info.trigger_methods = pd.trigger_methods;
                    info.presentation_id = pd.presentation_object_id;
                    totals.push_back(std::move(info));
                }
            }
            return totals;
        }

        // Get the number of sections in a DDOP
        static u16 section_count(const DDOP &ddop) {
            u16 count = 0;
            for (const auto &elem : ddop.elements()) {
                if (elem.type == DeviceElementType::Section)
                    ++count;
            }
            return count;
        }

        // Find the device element containing a specific process data or property
        static dp::Optional<const DeviceElement *> find_parent_element(const DDOP &ddop, ObjectID child_id) {
            for (const auto &elem : ddop.elements()) {
                for (auto cid : elem.child_objects) {
                    if (cid == child_id)
                        return &elem;
                }
            }
            return dp::nullopt;
        }

      private:
        static void extract_offsets(const DDOP &ddop, const DeviceElement &elem, i32 &x_mm, i32 &y_mm) {
            for (auto child_id : elem.child_objects) {
                for (const auto &prop : ddop.properties()) {
                    if (prop.id == child_id) {
                        if (prop.ddi == ddi::DEVICE_ELEMENT_OFFSET_X || prop.ddi == ddi::CONNECTOR_PIVOT_X_OFFSET) {
                            x_mm = prop.value;
                        } else if (prop.ddi == ddi::DEVICE_ELEMENT_OFFSET_Y) {
                            y_mm = prop.value;
                        }
                    }
                }
            }
        }
    };

} // namespace agrobus::isobus::tc
