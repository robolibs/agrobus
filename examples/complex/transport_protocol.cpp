// transport_protocol.cpp
//
// Comprehensive example demonstrating the TransportProtocol class from the
// isobus library. Shows both BAM (broadcast) and Connection-Mode (RTS/CTS/EOMA)
// multi-frame message transfer, event subscription, update loops, and simulated
// peer responses.
//
// TransportProtocol is standalone -- no IsoNet dependency for frame
// generation. It produces CAN frames that the application is responsible for
// transmitting on the physical bus.

#include <agrobus/net/tp.hpp>
#include <agrobus/net/frame.hpp>
#include <agrobus/net/identifier.hpp>
#include <echo/echo.hpp>

using namespace agrobus::net;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Build a sequential byte pattern for easy verification.
static dp::Vector<u8> make_sequential_data(u32 size) {
    dp::Vector<u8> data(size);
    for (u32 i = 0; i < size; ++i) {
        data[i] = static_cast<u8>(i & 0xFF);
    }
    return data;
}

// Print a summary of a CAN frame.
static void print_frame(const Frame &f, const char *label) {
    echo::info(label, ": id=0x", f.id.raw,
               " pgn=0x", f.pgn(),
               " src=0x", static_cast<u8>(f.source()),
               " dst=0x", static_cast<u8>(f.destination()),
               " [", f.data[0], " ", f.data[1], " ", f.data[2], " ",
               f.data[3], " ", f.data[4], " ", f.data[5], " ",
               f.data[6], " ", f.data[7], "]");
}

// Simulate building a CTS frame from a remote peer.
static Frame build_cts(Address peer_addr, Address our_addr, u8 num_packets,
                        u8 next_seq, PGN pgn) {
    Frame f;
    f.id = Identifier::encode(Priority::Lowest, PGN_TP_CM, peer_addr, our_addr);
    f.data[0] = tp_cm::CTS;
    f.data[1] = num_packets;
    f.data[2] = next_seq;
    f.data[3] = 0xFF;
    f.data[4] = 0xFF;
    f.data[5] = static_cast<u8>(pgn & 0xFF);
    f.data[6] = static_cast<u8>((pgn >> 8) & 0xFF);
    f.data[7] = static_cast<u8>((pgn >> 16) & 0xFF);
    f.length = 8;
    return f;
}

// Simulate building an EOMA frame from a remote peer.
static Frame build_eoma(Address peer_addr, Address our_addr, u32 total_bytes,
                         u8 total_packets, PGN pgn) {
    Frame f;
    f.id = Identifier::encode(Priority::Lowest, PGN_TP_CM, peer_addr, our_addr);
    f.data[0] = tp_cm::EOMA;
    f.data[1] = static_cast<u8>(total_bytes & 0xFF);
    f.data[2] = static_cast<u8>((total_bytes >> 8) & 0xFF);
    f.data[3] = total_packets;
    f.data[4] = 0xFF;
    f.data[5] = static_cast<u8>(pgn & 0xFF);
    f.data[6] = static_cast<u8>((pgn >> 8) & 0xFF);
    f.data[7] = static_cast<u8>((pgn >> 16) & 0xFF);
    f.length = 8;
    return f;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main() {
    echo::info("=== Transport Protocol Comprehensive Example ===");
    echo::info("MAX_DATA_LENGTH = ", TransportProtocol::MAX_DATA_LENGTH, " bytes");
    echo::info("BYTES_PER_FRAME = ", TransportProtocol::BYTES_PER_FRAME, " bytes/frame");
    echo::info("");

    // Addresses used throughout the example.
    constexpr Address SOURCE_ADDR = 0x80;
    constexpr Address DEST_ADDR = 0x20;
    constexpr PGN TEST_PGN = 0xFECA; // PGN_DM1 (Active Diagnostic Trouble Codes)

    // Track completion/abort counts for verification.
    u32 complete_count = 0;
    u32 abort_count = 0;

    // -----------------------------------------------------------------------
    // 1. BAM (Broadcast Announce Message) Transfer -- 100 bytes
    // -----------------------------------------------------------------------
    echo::info("--- Part 1: BAM Broadcast Transfer (100 bytes) ---");

    TransportProtocol tp;

    // Subscribe to completion events.
    tp.on_complete.subscribe([&complete_count](TransportSession &session) {
        echo::info("[EVENT] Transfer complete: pgn=0x", session.pgn,
                   " bytes=", session.total_bytes,
                   " direction=", session.direction == TransportDirection::Transmit
                       ? "TX" : "RX");
        ++complete_count;
    });

    // Subscribe to abort events.
    tp.on_abort.subscribe([&abort_count](TransportSession &session,
                                         TransportAbortReason reason) {
        echo::warn("[EVENT] Transfer aborted: pgn=0x", session.pgn,
                   " reason=", static_cast<int>(reason));
        ++abort_count;
    });

    // Prepare 100 bytes of sequential data.
    auto bam_data = make_sequential_data(100);
    u32 bam_total_packets = (100 + TransportProtocol::BYTES_PER_FRAME - 1)
                            / TransportProtocol::BYTES_PER_FRAME; // = 15 packets
    echo::info("Data size: 100 bytes -> ", bam_total_packets, " DT packets expected");

    // Initiate the BAM send. This returns the BAM announcement frame.
    auto bam_result = tp.send(TEST_PGN, bam_data, SOURCE_ADDR, BROADCAST_ADDRESS);
    if (!bam_result) {
        echo::error("BAM send failed");
        return 1;
    }

    auto &bam_frames = bam_result.value();
    echo::info("send() returned ", bam_frames.size(), " frame(s) -- BAM announcement");
    print_frame(bam_frames[0], "  BAM");

    // Simulate the update loop. BAM requires inter-packet delay of 50ms.
    // Each call to update() with >= 50ms elapsed generates one DT frame.
    echo::info("Running update loop to generate DT frames...");
    u32 dt_count = 0;
    for (u32 tick = 0; tick < bam_total_packets + 2; ++tick) {
        auto dt_frames = tp.update(TP_BAM_INTER_PACKET_MS); // 50ms per tick
        for (auto &f : dt_frames) {
            ++dt_count;
            if (dt_count <= 3 || dt_count == bam_total_packets) {
                print_frame(f, "  DT");
            } else if (dt_count == 4) {
                echo::info("  ... (", bam_total_packets - 4, " more DT frames) ...");
            }
        }
    }
    echo::info("BAM complete: ", dt_count, " DT frames generated");
    echo::info("Completions so far: ", complete_count);
    echo::info("");

    // -----------------------------------------------------------------------
    // 2. Connection-Mode Transfer (RTS/CTS/EOMA) -- 200 bytes
    // -----------------------------------------------------------------------
    echo::info("--- Part 2: Connection-Mode Transfer (200 bytes) ---");

    TransportProtocol tp_cm_proto;

    // Re-subscribe events on the new instance.
    tp_cm_proto.on_complete.subscribe([&complete_count](TransportSession &session) {
        echo::info("[EVENT] CM Transfer complete: pgn=0x", session.pgn,
                   " bytes=", session.total_bytes);
        ++complete_count;
    });
    tp_cm_proto.on_abort.subscribe([&abort_count](TransportSession &session,
                                                   TransportAbortReason reason) {
        echo::warn("[EVENT] CM Transfer aborted: pgn=0x", session.pgn,
                   " reason=", static_cast<int>(reason));
        ++abort_count;
    });

    auto cm_data = make_sequential_data(200);
    u32 cm_total_packets = (200 + TransportProtocol::BYTES_PER_FRAME - 1)
                           / TransportProtocol::BYTES_PER_FRAME; // = 29 packets
    echo::info("Data size: 200 bytes -> ", cm_total_packets, " DT packets expected");

    // Step 2a: Initiate the connection-mode send. Returns the RTS frame.
    auto cm_result = tp_cm_proto.send(TEST_PGN, cm_data, SOURCE_ADDR, DEST_ADDR,
                                       0, Priority::Default);
    if (!cm_result) {
        echo::error("CM send failed");
        return 1;
    }

    auto &rts_frames = cm_result.value();
    echo::info("send() returned ", rts_frames.size(), " frame(s) -- RTS");
    print_frame(rts_frames[0], "  RTS");

    // Step 2b: Simulate the remote peer responding with CTS.
    // The peer allows up to 16 packets per CTS window (TP_MAX_PACKETS_PER_CTS).
    echo::info("Simulating peer CTS responses and data transfer...");

    u32 total_data_frames = 0;
    u8 next_seq = 1;

    while (next_seq <= cm_total_packets) {
        // Determine how many packets the peer will request in this CTS.
        u8 remaining = static_cast<u8>(cm_total_packets - (next_seq - 1));
        u8 window_size = remaining < TP_MAX_PACKETS_PER_CTS
                             ? remaining
                             : static_cast<u8>(TP_MAX_PACKETS_PER_CTS);

        echo::info("  Peer sends CTS: num_packets=", window_size,
                   " next_seq=", next_seq);

        // Build the CTS from the remote peer and feed it into our protocol.
        Frame cts_frame = build_cts(DEST_ADDR, SOURCE_ADDR, window_size,
                                     next_seq, TEST_PGN);
        auto cts_responses = tp_cm_proto.process_frame(cts_frame);
        // CTS processing triggers internal state change; no immediate response.

        // Step 2c: Retrieve the pending data frames for this CTS window.
        auto data_frames = tp_cm_proto.get_pending_data_frames();
        echo::info("  get_pending_data_frames() returned ", data_frames.size(),
                   " DT frame(s)");

        for (auto &f : data_frames) {
            ++total_data_frames;
            if (total_data_frames <= 2) {
                print_frame(f, "    DT");
            } else if (total_data_frames == 3 && data_frames.size() > 3) {
                echo::info("    ... (", data_frames.size() - 3, " more in window) ...");
            }
        }

        next_seq += window_size;
    }

    echo::info("  Total DT frames sent: ", total_data_frames);

    // Step 2d: Simulate the peer sending EOMA to acknowledge complete transfer.
    echo::info("  Peer sends EOMA");
    Frame eoma_frame = build_eoma(DEST_ADDR, SOURCE_ADDR, 200,
                                   static_cast<u8>(cm_total_packets), TEST_PGN);
    auto eoma_responses = tp_cm_proto.process_frame(eoma_frame);
    echo::info("  EOMA processed -- session complete");
    echo::info("");

    // -----------------------------------------------------------------------
    // 3. Receiving a Connection-Mode Message (RX side)
    // -----------------------------------------------------------------------
    echo::info("--- Part 3: Receiving a Connection-Mode Message ---");

    TransportProtocol tp_rx;
    dp::Vector<u8> received_data;

    tp_rx.on_complete.subscribe([&received_data, &complete_count](TransportSession &session) {
        echo::info("[EVENT] RX complete: pgn=0x", session.pgn,
                   " bytes=", session.total_bytes);
        received_data = session.data;
        ++complete_count;
    });

    // Simulate a remote peer sending us an RTS for 50 bytes.
    constexpr u32 RX_DATA_SIZE = 50;
    u32 rx_total_packets = (RX_DATA_SIZE + TransportProtocol::BYTES_PER_FRAME - 1)
                           / TransportProtocol::BYTES_PER_FRAME; // = 8 packets
    echo::info("Simulating incoming RTS for ", RX_DATA_SIZE, " bytes (",
               rx_total_packets, " packets)");

    // Build an RTS from a remote sender.
    Frame rts_incoming;
    rts_incoming.id = Identifier::encode(Priority::Lowest, PGN_TP_CM,
                                          DEST_ADDR, SOURCE_ADDR);
    rts_incoming.data[0] = tp_cm::RTS;
    rts_incoming.data[1] = static_cast<u8>(RX_DATA_SIZE & 0xFF);
    rts_incoming.data[2] = static_cast<u8>((RX_DATA_SIZE >> 8) & 0xFF);
    rts_incoming.data[3] = static_cast<u8>(rx_total_packets);
    rts_incoming.data[4] = static_cast<u8>(TP_MAX_PACKETS_PER_CTS);
    rts_incoming.data[5] = static_cast<u8>(TEST_PGN & 0xFF);
    rts_incoming.data[6] = static_cast<u8>((TEST_PGN >> 8) & 0xFF);
    rts_incoming.data[7] = static_cast<u8>((TEST_PGN >> 16) & 0xFF);
    rts_incoming.length = 8;

    auto rts_responses = tp_rx.process_frame(rts_incoming);
    echo::info("  RTS processed -> ", rts_responses.size(), " response(s) [CTS]");
    if (!rts_responses.empty()) {
        print_frame(rts_responses[0], "  CTS reply");
    }

    // Build and feed DT frames with sequential data.
    auto rx_source_data = make_sequential_data(RX_DATA_SIZE);
    echo::info("  Feeding ", rx_total_packets, " DT frames...");

    for (u8 seq = 1; seq <= static_cast<u8>(rx_total_packets); ++seq) {
        Frame dt;
        dt.id = Identifier::encode(Priority::Lowest, PGN_TP_DT,
                                    DEST_ADDR, SOURCE_ADDR);
        dt.data[0] = seq;
        for (u8 j = 0; j < 7; ++j) {
            u32 idx = (static_cast<u32>(seq) - 1) * 7 + j;
            dt.data[j + 1] = (idx < RX_DATA_SIZE) ? rx_source_data[idx] : 0xFF;
        }
        dt.length = 8;

        auto dt_responses = tp_rx.process_frame(dt);
        // Last DT will produce an EOMA response.
        if (!dt_responses.empty()) {
            for (auto &resp : dt_responses) {
                print_frame(resp, "  Response");
            }
        }
    }

    // Verify received data matches what was sent.
    bool data_ok = (received_data.size() == RX_DATA_SIZE);
    if (data_ok) {
        for (u32 i = 0; i < RX_DATA_SIZE; ++i) {
            if (received_data[i] != rx_source_data[i]) {
                data_ok = false;
                break;
            }
        }
    }
    echo::info("  Data integrity check: ", data_ok ? "PASS" : "FAIL");
    echo::info("");

    // -----------------------------------------------------------------------
    // 4. Timeout Handling
    // -----------------------------------------------------------------------
    echo::info("--- Part 4: Timeout Handling ---");

    TransportProtocol tp_timeout;
    tp_timeout.on_abort.subscribe([&abort_count](TransportSession &session,
                                                  TransportAbortReason reason) {
        echo::warn("[EVENT] Timeout abort: pgn=0x", session.pgn,
                   " reason=", static_cast<int>(reason));
        ++abort_count;
    });

    // Start a connection-mode send, but never respond with CTS.
    auto timeout_data = make_sequential_data(50);
    auto timeout_result = tp_timeout.send(TEST_PGN, timeout_data, SOURCE_ADDR,
                                           DEST_ADDR);
    if (timeout_result) {
        echo::info("  CM send initiated, waiting for CTS timeout...");
        // Simulate time passing beyond T3 timeout (1250ms).
        auto timeout_frames = tp_timeout.update(TP_TIMEOUT_T3_MS + 1);
        echo::info("  update() after ", TP_TIMEOUT_T3_MS + 1,
                   "ms returned ", timeout_frames.size(), " frame(s)");
        if (!timeout_frames.empty()) {
            print_frame(timeout_frames[0], "  ABORT");
        }
    }
    echo::info("");

    // -----------------------------------------------------------------------
    // Summary
    // -----------------------------------------------------------------------
    echo::info("=== Summary ===");
    echo::info("  Completions: ", complete_count);
    echo::info("  Aborts:      ", abort_count);
    echo::info("  Expected:    3 completions, 1 abort");

    return 0;
}
