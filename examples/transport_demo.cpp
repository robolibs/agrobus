#include <agrobus/net/tp.hpp>
#include <agrobus/net/etp.hpp>
#include <agrobus/net/fast_packet.hpp>
#include <agrobus/net/constants.hpp>
#include <echo/echo.hpp>

using namespace agrobus::net;

int main() {
    echo::info("=== Transport Protocol Demo ===");

    // --- TP (BAM) ---
    TransportProtocol tp;
    tp.on_complete.subscribe([](TransportSession& session) {
        echo::info("TP complete: pgn=0x", session.pgn,
                   " bytes=", session.total_bytes,
                   " progress=", session.progress() * 100, "%");
    });
    tp.on_abort.subscribe([](TransportSession& session, TransportAbortReason reason) {
        echo::warn("TP aborted: pgn=0x", session.pgn,
                   " reason=", static_cast<int>(reason));
    });

    // Send 100 bytes via BAM
    dp::Vector<u8> bam_data(100);
    for (usize i = 0; i < 100; ++i) bam_data[i] = static_cast<u8>(i);

    auto bam_result = tp.send(0xFECA, bam_data, 0x28, BROADCAST_ADDRESS);
    if (bam_result) {
        echo::info("BAM initiated: ", bam_result.value().size(), " frames");
    }

    // --- Fast Packet ---
    echo::info("\n--- Fast Packet ---");
    FastPacketProtocol fp;
    dp::Vector<u8> fp_data(30);
    for (usize i = 0; i < 30; ++i) fp_data[i] = static_cast<u8>(i + 0x40);

    auto fp_result = fp.send(PGN_GNSS_POSITION, fp_data, 0x28);
    if (fp_result) {
        auto& fp_frames = fp_result.value();
        echo::info("Fast Packet: ", fp_frames.size(), " frames for 30 bytes");

        // Receive them back
        FastPacketProtocol fp_rx;
        dp::Optional<Message> msg;
        for (const auto& frame : fp_frames) {
            msg = fp_rx.process_frame(frame);
        }
        if (msg.has_value()) {
            auto& received = msg.value();
            echo::info("Received fast packet: pgn=0x", received.pgn,
                       " size=", received.data.size());
            bool match = true;
            for (usize i = 0; i < 30; ++i) {
                if (received.data[i] != fp_data[i]) { match = false; break; }
            }
            echo::info("Data integrity: ", match ? "OK" : "FAIL");
        }
    }

    // --- Summary ---
    echo::info("\n--- Limits ---");
    echo::info("Single frame: <= ", CAN_DATA_LENGTH, " bytes");
    echo::info("Fast Packet:  <= ", FAST_PACKET_MAX_DATA, " bytes");
    echo::info("TP:           <= ", TP_MAX_DATA_LENGTH, " bytes");
    echo::info("ETP:          <= ", ETP_MAX_DATA_LENGTH, " bytes");

    return 0;
}
