#pragma once

#include <agrobus/net/types.hpp>
#include <datapod/datapod.hpp>

namespace agrobus::net {

    // ─── Bus load estimation ─────────────────────────────────────────────────────
    class BusLoad {
        static constexpr usize WINDOW_SIZE = 100;
        static constexpr u32 SAMPLE_PERIOD_MS = 100;
        static constexpr u32 CAN_BITRATE = 250000; // 250 kbit/s standard ISOBUS

        dp::Array<u32, WINDOW_SIZE> bit_counts_ = {};
        usize write_idx_ = 0;
        u32 current_bits_ = 0;
        u32 timer_ms_ = 0;
        bool filled_ = false;

      public:
        void add_frame(u8 dlc = 8) noexcept {
            // Standard CAN frame overhead: SOF(1) + ID(29) + SRR(1) + IDE(1) + RTR(1) +
            // r0(1) + DLC(4) + DATA(dlc*8) + CRC(15) + CRC_del(1) + ACK(2) + EOF(7) + IFS(3)
            // Plus ~20% stuff bits on average
            u32 bits = 67 + static_cast<u32>(dlc) * 8;
            bits = bits * 120 / 100; // approximate stuff bits
            current_bits_ += bits;
        }

        void update(u32 elapsed_ms) noexcept {
            timer_ms_ += elapsed_ms;
            if (timer_ms_ >= SAMPLE_PERIOD_MS) {
                timer_ms_ -= SAMPLE_PERIOD_MS;
                bit_counts_[write_idx_] = current_bits_;
                write_idx_ = (write_idx_ + 1) % WINDOW_SIZE;
                if (write_idx_ == 0)
                    filled_ = true;
                current_bits_ = 0;
            }
        }

        f32 load_percent() const noexcept {
            usize count = filled_ ? WINDOW_SIZE : write_idx_;
            if (count == 0)
                return 0.0f;

            u32 total_bits = 0;
            for (usize i = 0; i < count; ++i) {
                total_bits += bit_counts_[i];
            }

            f32 window_seconds = static_cast<f32>(count) * static_cast<f32>(SAMPLE_PERIOD_MS) / 1000.0f;
            f32 bits_per_second = static_cast<f32>(total_bits) / window_seconds;
            return (bits_per_second / static_cast<f32>(CAN_BITRATE)) * 100.0f;
        }

        void reset() noexcept {
            bit_counts_ = {};
            write_idx_ = 0;
            current_bits_ = 0;
            timer_ms_ = 0;
            filled_ = false;
        }
    };
} // namespace agrobus::net
