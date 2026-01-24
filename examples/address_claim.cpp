#include <agrobus/net/network_manager.hpp>
#include <agrobus/net/address_claimer.hpp>
#include <echo/echo.hpp>

using namespace agrobus::net;

int main() {
    echo::info("=== Address Claim Demo ===");

    // Create network manager
    IsoNet nm;

    // Create our device identity
    Name name = Name::build()
        .set_identity_number(1000)
        .set_manufacturer_code(42)
        .set_function_code(25)
        .set_device_class(7)
        .set_industry_group(2)
        .set_self_configurable(true);

    // Create internal control function with preferred address 0x28
    auto result = nm.create_internal(name, 0, 0x28);
    if (!result) {
        echo::error("Failed to create internal CF");
        return 1;
    }

    auto* cf = result.value();
    cf->on_address_claimed.subscribe([](Address addr) {
        echo::info("Address claimed: 0x", addr);
    });
    cf->on_address_lost.subscribe([]() {
        echo::warn("Address lost!");
    });

    // Simulate address claim process (without real CAN driver)
    AddressClaimer claimer(cf);
    auto frames = claimer.start();
    echo::info("Sent ", frames.size(), " frames to start claim");

    // Simulate timeout (claim succeeds)
    for (u32 t = 0; t < 300; t += 50) {
        claimer.update(50);
        if (cf->claim_state() == ClaimState::Claimed) {
            echo::info("Claim successful at t=", t, "ms");
            break;
        }
    }

    echo::info("Final state: address=0x", cf->address(),
               " state=", static_cast<int>(cf->claim_state()));

    return 0;
}
