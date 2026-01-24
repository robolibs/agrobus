// nmea2k_fast_packet.cpp
// Comprehensive example demonstrating the FastPacketProtocol class.
//
// Fast Packet is used on NMEA2000 networks for messages between 9-223 bytes.
// This example shows:
//   1. Generating CAN frames for a 43-byte GNSS Position Data message
//   2. Inspecting the frame encoding (sequence counter, frame counter, data layout)
//   3. Simulating RX by feeding frames into process_frame() for reassembly
//   4. Subscribing to the on_message event for completed reassemblies
//   5. Using update() for timeout-based session cleanup

#include <agrobus/net/fast_packet.hpp>
#include <agrobus/net/frame.hpp>
#include <agrobus/net/identifier.hpp>
#include <echo/echo.hpp>

using namespace agrobus::net;

static constexpr Address SOURCE_ADDR = 0x40;

// Build a realistic 43-byte GNSS Position Data payload.
// Field layout follows NMEA2000 PGN 129029 structure (simplified).
static dp::Vector<u8> build_gnss_payload() {
    dp::Vector<u8> data(43, 0xFF);

    // Byte 0: SID (Sequence Identifier)
    data[0] = 0x01;

    // Bytes 1-2: Days since 1970-01-01 (e.g., 2024-06-15 = 19889 days)
    u16 days = 19889;
    data[1] = static_cast<u8>(days & 0xFF);
    data[2] = static_cast<u8>((days >> 8) & 0xFF);

    // Bytes 3-6: Time since midnight in units of 0.0001s (e.g., 12:30:00 = 450000000)
    u32 time_val = 450000000;
    data[3] = static_cast<u8>(time_val & 0xFF);
    data[4] = static_cast<u8>((time_val >> 8) & 0xFF);
    data[5] = static_cast<u8>((time_val >> 16) & 0xFF);
    data[6] = static_cast<u8>((time_val >> 24) & 0xFF);

    // Bytes 7-14: Latitude (64-bit, 1e-16 deg, e.g., 48.123400 deg)
    i64 lat = 481234000000000LL;
    for (int i = 0; i < 8; ++i) {
        data[7 + i] = static_cast<u8>((lat >> (i * 8)) & 0xFF);
    }

    // Bytes 15-22: Longitude (64-bit, 1e-16 deg, e.g., 11.567800 deg)
    i64 lon = 115678000000000LL;
    for (int i = 0; i < 8; ++i) {
        data[15 + i] = static_cast<u8>((lon >> (i * 8)) & 0xFF);
    }

    // Bytes 23-30: Altitude (64-bit, 1e-6 m, e.g., 450.250 m)
    i64 alt = 450250000LL;
    for (int i = 0; i < 8; ++i) {
        data[23 + i] = static_cast<u8>((alt >> (i * 8)) & 0xFF);
    }

    // Byte 31: GNSS type + method nibbles
    //   Type: GPS (0), Method: GNSS fix (1)
    data[31] = 0x01;

    // Byte 32: Integrity (2 bits) + reserved
    data[32] = 0x00;

    // Byte 33: Number of SVs used
    data[33] = 18;

    // Bytes 34-35: HDOP (0.01 resolution, e.g., 0.85 = 85)
    u16 hdop = 85;
    data[34] = static_cast<u8>(hdop & 0xFF);
    data[35] = static_cast<u8>((hdop >> 8) & 0xFF);

    // Bytes 36-37: PDOP (0.01 resolution, e.g., 1.20 = 120)
    u16 pdop = 120;
    data[36] = static_cast<u8>(pdop & 0xFF);
    data[37] = static_cast<u8>((pdop >> 8) & 0xFF);

    // Bytes 38-41: Geoidal separation (0.01 m, e.g., 47.50 m = 4750)
    i32 geoid_sep = 4750;
    data[38] = static_cast<u8>(geoid_sep & 0xFF);
    data[39] = static_cast<u8>((geoid_sep >> 8) & 0xFF);
    data[40] = static_cast<u8>((geoid_sep >> 16) & 0xFF);
    data[41] = static_cast<u8>((geoid_sep >> 24) & 0xFF);

    // Byte 42: Number of reference stations
    data[42] = 1;

    return data;
}

// Print a single CAN frame with annotations
static void print_frame(usize index, const Frame& frame) {
    u8 seq_counter = (frame.data[0] >> 5) & 0x07;
    u8 frame_counter = frame.data[0] & 0x1F;

    echo::info("  Frame[", index, "]: seq=", static_cast<int>(seq_counter),
               " frame_ctr=", static_cast<int>(frame_counter),
               " id=0x", frame.id.raw);

    if (frame_counter == 0) {
        echo::info("    [FIRST] total_bytes=", static_cast<int>(frame.data[1]),
                   " data=[",
                   static_cast<int>(frame.data[2]), ", ",
                   static_cast<int>(frame.data[3]), ", ",
                   static_cast<int>(frame.data[4]), ", ",
                   static_cast<int>(frame.data[5]), ", ",
                   static_cast<int>(frame.data[6]), ", ",
                   static_cast<int>(frame.data[7]), "]");
    } else {
        echo::info("    [CONT ] data=[",
                   static_cast<int>(frame.data[1]), ", ",
                   static_cast<int>(frame.data[2]), ", ",
                   static_cast<int>(frame.data[3]), ", ",
                   static_cast<int>(frame.data[4]), ", ",
                   static_cast<int>(frame.data[5]), ", ",
                   static_cast<int>(frame.data[6]), ", ",
                   static_cast<int>(frame.data[7]), "]");
    }
}

int main() {
    echo::info("=== NMEA2000 Fast Packet Protocol Demo ===");
    echo::info("PGN: 0x1F805 (129029 - GNSS Position Data)");
    echo::info("Source address: 0x40");
    echo::info("Max fast packet payload: ", FastPacketProtocol::MAX_DATA_LENGTH, " bytes");

    // ─── Step 1: Build GNSS payload ───────────────────────────────────────────────
    echo::info("\n--- Step 1: Build GNSS Position Data payload ---");
    dp::Vector<u8> gnss_data = build_gnss_payload();
    echo::info("Payload size: ", gnss_data.size(), " bytes");
    echo::info("Expected frames: 1 (first, 6 data bytes) + ",
               (gnss_data.size() - FastPacketProtocol::FIRST_FRAME_DATA +
                FastPacketProtocol::SUBSEQUENT_FRAME_DATA - 1) /
                   FastPacketProtocol::SUBSEQUENT_FRAME_DATA,
               " (subsequent, 7 data bytes each)");

    // ─── Step 2: Send (generate TX frames) ────────────────────────────────────────
    echo::info("\n--- Step 2: Generate fast packet frames ---");
    FastPacketProtocol fp_tx;

    auto result = fp_tx.send(PGN_GNSS_POSITION_DATA, gnss_data, SOURCE_ADDR);
    if (!result) {
        echo::error("Send failed: ", result.error().message);
        return 1;
    }

    dp::Vector<Frame>& tx_frames = result.value();
    echo::info("Generated ", tx_frames.size(), " CAN frames:");
    echo::info("  First frame layout:  [seq:3|ctr:5][total_bytes][6 data bytes]");
    echo::info("  Subsequent layout:   [seq:3|ctr:5][7 data bytes]");

    for (usize i = 0; i < tx_frames.size(); ++i) {
        print_frame(i, tx_frames[i]);
    }

    // ─── Step 3: Subscribe to on_message event ────────────────────────────────────
    echo::info("\n--- Step 3: Set up RX with event subscription ---");
    FastPacketProtocol fp_rx;

    bool event_fired = false;
    PGN received_pgn = 0;
    usize received_size = 0;

    fp_rx.on_message.subscribe([&](const Message& msg) {
        event_fired = true;
        received_pgn = msg.pgn;
        received_size = msg.data.size();
        echo::info("  [EVENT] on_message fired: pgn=0x", msg.pgn,
                   " size=", msg.data.size(),
                   " src=0x", static_cast<int>(msg.source));
    });
    echo::info("Subscribed to on_message event");

    // ─── Step 4: Simulate RX by feeding frames into process_frame() ───────────────
    echo::info("\n--- Step 4: Process frames (simulate reception) ---");
    dp::Optional<Message> reassembled;

    for (usize i = 0; i < tx_frames.size(); ++i) {
        echo::info("  Processing frame ", i, "...");
        auto msg_opt = fp_rx.process_frame(tx_frames[i]);

        if (msg_opt.has_value()) {
            reassembled = msg_opt;
            echo::info("  -> Reassembly complete at frame ", i);

            // Manually emit the event (process_frame returns the message;
            // the application layer is responsible for emitting events)
            fp_rx.on_message.emit(msg_opt.value());
        }
    }

    // ─── Step 5: Verify reassembled data ──────────────────────────────────────────
    echo::info("\n--- Step 5: Verify data integrity ---");
    if (!reassembled.has_value()) {
        echo::error("FAIL: No message reassembled!");
        return 1;
    }

    const Message& msg = reassembled.value();
    echo::info("Reassembled message:");
    echo::info("  PGN:         0x", msg.pgn, " (expected 0x", PGN_GNSS_POSITION_DATA, ")");
    echo::info("  Source:      0x", static_cast<int>(msg.source),
               " (expected 0x", static_cast<int>(SOURCE_ADDR), ")");
    echo::info("  Data size:   ", msg.data.size(), " (expected ", gnss_data.size(), ")");

    bool data_match = true;
    if (msg.data.size() != gnss_data.size()) {
        data_match = false;
    } else {
        for (usize i = 0; i < gnss_data.size(); ++i) {
            if (msg.data[i] != gnss_data[i]) {
                echo::warn("  Mismatch at byte ", i, ": got 0x",
                           static_cast<int>(msg.data[i]), " expected 0x",
                           static_cast<int>(gnss_data[i]));
                data_match = false;
                break;
            }
        }
    }

    echo::info("  Data match:  ", data_match ? "PASS" : "FAIL");
    echo::info("  Event fired: ", event_fired ? "YES" : "NO");

    // ─── Step 6: Demonstrate update() for timeout cleanup ─────────────────────────
    echo::info("\n--- Step 6: Timeout cleanup via update() ---");
    FastPacketProtocol fp_timeout;

    // Start a new reassembly by sending only the first frame
    echo::info("Feeding only the first frame (incomplete transfer)...");
    fp_timeout.process_frame(tx_frames[0]);

    // Simulate elapsed time below timeout (should keep session alive)
    echo::info("  update(500ms) - below timeout threshold (", TP_TIMEOUT_T1_MS, "ms)");
    fp_timeout.update(500);

    // Feed nothing more, advance past the timeout
    echo::info("  update(300ms) - now exceeds timeout, session will be cleaned up");
    fp_timeout.update(300);

    // Attempting to continue would fail because the session was removed
    echo::info("  Incomplete session cleaned up after timeout");

    // ─── Summary ──────────────────────────────────────────────────────────────────
    echo::info("\n=== Summary ===");
    echo::info("Fast Packet Protocol handles NMEA2000 messages from ",
               CAN_DATA_LENGTH + 1, " to ", FastPacketProtocol::MAX_DATA_LENGTH, " bytes.");
    echo::info("Frame encoding:");
    echo::info("  - First frame carries ", FastPacketProtocol::FIRST_FRAME_DATA,
               " data bytes + total length");
    echo::info("  - Subsequent frames carry ", FastPacketProtocol::SUBSEQUENT_FRAME_DATA,
               " data bytes each");
    echo::info("  - 3-bit sequence counter distinguishes concurrent transfers");
    echo::info("  - 5-bit frame counter tracks ordering (max 32 frames)");
    echo::info("Timeout: sessions expire after ", TP_TIMEOUT_T1_MS, "ms without progress");

    echo::info("\nDone.");
    return 0;
}
