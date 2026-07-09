/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */
#include "frame_codec.h"

namespace chainled::chain {

std::array<std::uint8_t, chainled::core::kFrameSize> encode_mono_display_buffer(const chainled::core::Frame& frame) {
    std::array<std::uint8_t, chainled::core::kFrameSize> buffer{};
    for (int y = 0; y < chainled::core::kFrameSize; ++y) {
        std::uint8_t row = 0;
        for (int x = 0; x < chainled::core::kFrameSize; ++x) {
            if (frame.pixel(x, y) != chainled::core::kOffColor) {
                row = static_cast<std::uint8_t>(row | (1U << x));
            }
        }
        buffer[static_cast<std::size_t>(y)] = row;
    }
    return buffer;
}

std::array<std::uint8_t, chainled::core::kFrameSize * chainled::core::kFrameSize * 2> encode_rgb_display_buffer(
    const chainled::core::Frame& frame) {
    std::array<std::uint8_t, chainled::core::kFrameSize * chainled::core::kFrameSize * 2> buffer{};
    for (int y = 0; y < chainled::core::kFrameSize; ++y) {
        for (int x = 0; x < chainled::core::kFrameSize; ++x) {
            const auto pixel = frame.pixel(x, y);
            const auto offset = static_cast<std::size_t>((y * chainled::core::kFrameSize + x) * 2);
            buffer[offset] = static_cast<std::uint8_t>(pixel & 0xFF);
            buffer[offset + 1] = static_cast<std::uint8_t>((pixel >> 8) & 0xFF);
        }
    }
    return buffer;
}

}  // namespace chainled::chain
