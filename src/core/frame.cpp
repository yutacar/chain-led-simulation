/**
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */
#include "core/frame.h"

#include <utility>

namespace chainled::core {

Frame::Frame() { pixels_.fill(kOffColor); }

Rgb565 Frame::pixel(int x, int y) const {
    if (!in_bounds(x, y)) {
        return kOffColor;
    }
    return pixels_[static_cast<std::size_t>(y * kFrameSize + x)];
}

bool Frame::set_pixel(int x, int y, Rgb565 color) {
    if (!in_bounds(x, y)) {
        return false;
    }
    pixels_[static_cast<std::size_t>(y * kFrameSize + x)] = color;
    return true;
}

void Frame::clear() { pixels_.fill(kOffColor); }

void Frame::fill(Rgb565 color) { pixels_.fill(color); }

void Frame::flip_horizontal() {
    for (int y = 0; y < kFrameSize; ++y) {
        for (int x = 0; x < kFrameSize / 2; ++x) {
            std::swap(pixels_[static_cast<std::size_t>(y * kFrameSize + x)],
                      pixels_[static_cast<std::size_t>(y * kFrameSize + (kFrameSize - 1 - x))]);
        }
    }
}

void Frame::flip_vertical() {
    for (int y = 0; y < kFrameSize / 2; ++y) {
        for (int x = 0; x < kFrameSize; ++x) {
            std::swap(pixels_[static_cast<std::size_t>(y * kFrameSize + x)],
                      pixels_[static_cast<std::size_t>((kFrameSize - 1 - y) * kFrameSize + x)]);
        }
    }
}

void Frame::shift(int dx, int dy) {
    std::array<Rgb565, static_cast<std::size_t>(kFrameSize * kFrameSize)> shifted{};
    shifted.fill(kOffColor);
    for (int y = 0; y < kFrameSize; ++y) {
        for (int x = 0; x < kFrameSize; ++x) {
            const int src_x = x - dx;
            const int src_y = y - dy;
            if (in_bounds(src_x, src_y)) {
                shifted[static_cast<std::size_t>(y * kFrameSize + x)] =
                    pixels_[static_cast<std::size_t>(src_y * kFrameSize + src_x)];
            }
        }
    }
    pixels_ = shifted;
}

}  // namespace chainled::core
