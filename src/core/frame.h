/**
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 *
 * Frame: a single 8x8 dot-matrix image. Pure C++, zero LVGL/platform
 * dependency so it can be used from the editor, tests, the LED chain
 * output, and (later) the animation generator alike.
 */
#pragma once

#include <array>
#include <cstdint>

namespace chainled::core {

/// Frame is always 8x8, matching the Chain Mono / Chain RGB hardware grid.
constexpr int kFrameSize = 8;

/// Pixel color in RGB565. Mono output derives on/off by thresholding this
/// value (nonzero = on) rather than the core model having a separate
/// monochrome representation -- see PLAN.md §2.1/§2.2.
using Rgb565 = std::uint16_t;

/// The "off"/background pixel value.
constexpr Rgb565 kOffColor = 0;

/// Packs 8-bit RGB888 components into RGB565.
constexpr Rgb565 rgb565(std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    return static_cast<Rgb565>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

class Frame {
public:
    Frame();

    static constexpr bool in_bounds(int x, int y) {
        return x >= 0 && x < kFrameSize && y >= 0 && y < kFrameSize;
    }

    /// Returns kOffColor for out-of-bounds coordinates rather than throwing.
    [[nodiscard]] Rgb565 pixel(int x, int y) const;

    /// Returns false (no-op) for out-of-bounds coordinates.
    bool set_pixel(int x, int y, Rgb565 color);

    void clear();
    void fill(Rgb565 color);

    /// Mirrors the image left-right / top-bottom in place.
    void flip_horizontal();
    void flip_vertical();

    /// Translates the image by (dx, dy); cells vacated at the edges become
    /// kOffColor. Positive dx moves right, positive dy moves down.
    void shift(int dx, int dy);

    bool operator==(const Frame& other) const { return pixels_ == other.pixels_; }
    bool operator!=(const Frame& other) const { return !(*this == other); }

private:
    std::array<Rgb565, static_cast<std::size_t>(kFrameSize * kFrameSize)> pixels_{};
};

}  // namespace chainled::core
