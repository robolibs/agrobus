#include <agrobus.hpp>
#include <agrobus/net/iop_parser.hpp>
#include <echo/echo.hpp>

using namespace agrobus::net;
using namespace agrobus::j1939;
using namespace agrobus::isobus;
using namespace agrobus::nmea;
using namespace agrobus::isobus::vt;
using namespace agrobus::isobus::tc;
using namespace agrobus::isobus::sc;
using namespace agrobus::isobus::implement;
using namespace agrobus::isobus::fs;

int main() {
    echo::info("=== IOP Parser Demo ===");

    // Create a synthetic IOP binary (since we don't have a real file)
    dp::Vector<u8> iop_data;

    // WorkingSet (ID=0, Type=0, 200x200, bg=white, selectable=1, active_mask=1)
    dp::Vector<u8> ws = {0x00, 0x00, 0x00, 0xC8, 0x00, 0xC8, 0x00, 0x0F, 0x01, 0x01, 0x00};
    iop_data.insert(iop_data.end(), ws.begin(), ws.end());

    // DataMask (ID=1, Type=1, 200x200, bg=black, soft_key_mask=2)
    dp::Vector<u8> dm = {0x01, 0x00, 0x01, 0xC8, 0x00, 0xC8, 0x00, 0x00, 0x02, 0x00};
    iop_data.insert(iop_data.end(), dm.begin(), dm.end());

    // NumberVariable (ID=10, Type=21, 0x0, value=42)
    dp::Vector<u8> nv = {0x0A, 0x00, 0x15, 0x00, 0x00, 0x00, 0x00, 0x2A, 0x00, 0x00, 0x00};
    iop_data.insert(iop_data.end(), nv.begin(), nv.end());

    echo::info("Synthetic IOP data: ", iop_data.size(), " bytes");

    // Validate the data
    auto valid = IOPParser::validate(iop_data);
    if (valid.is_ok()) {
        echo::info("Validation: ", valid.value() ? "PASSED" : "FAILED");
    }

    // Parse into object pool
    auto result = IOPParser::parse_iop_data(iop_data);
    if (result.is_ok()) {
        auto& pool = result.value();
        echo::info("Parsed ", pool.size(), " objects");

        for (const auto& obj : pool.objects()) {
            echo::info("  Object ID=", obj.id, " type=", static_cast<u8>(obj.type),
                       " body_size=", obj.body.size());
        }
    }

    // Generate version hash
    auto version = IOPParser::hash_to_version(iop_data);
    if (version.is_ok()) {
        echo::info("Pool version label: ", version.value());
    }

    // Try reading a nonexistent file (expected to fail)
    auto file_result = IOPParser::read_iop_file("/tmp/nonexistent.iop");
    if (!file_result.is_ok()) {
        echo::info("File read failed as expected (no file)");
    }

    echo::info("=== IOP Parser Demo Complete ===");
    return 0;
}
