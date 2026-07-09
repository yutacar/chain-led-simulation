/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 *
 * Frame -> Chain SET_DISPLAY_BUFFER payload encoding (PLAN.md §1.2/§2.2).
 */
#pragma once

#include <array>

#include "core/frame.h"

namespace chainled::chain {

/// Chain Mono buffer: 1 byte per row, bit x set = column x lit (nonzero
/// pixel). Bit order isn't specified in M5Stack's public docs; this is a
/// documented choice to be confirmed against real hardware in Phase 9 --
/// if the on-device image comes out horizontally mirrored, flip it here.
std::array<std::uint8_t, chainled::core::kFrameSize> encode_mono_display_buffer(const chainled::core::Frame& frame);

/// Chain RGB buffer: 64 pixels, row-major, each RGB565 value written
/// little-endian (low byte first).
std::array<std::uint8_t, chainled::core::kFrameSize * chainled::core::kFrameSize * 2> encode_rgb_display_buffer(
    const chainled::core::Frame& frame);

}  // namespace chainled::chain
