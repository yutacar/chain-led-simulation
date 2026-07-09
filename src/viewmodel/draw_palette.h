/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 *
 * Fixed preset palette the Editor cycles through for the "current draw
 * color" (see PLAN.md Phase 2 scope note: a full RGB color picker is
 * deferred, but the core animation model already stores RGB565 per pixel,
 * so a small preset palette lets Chain RGB output be exercised meaningfully
 * without that picker UI yet).
 */
#pragma once

#include <array>
#include <cstddef>

#include "core/frame.h"

namespace viewmodel {

constexpr std::array<chainled::core::Rgb565, 6> kDrawPalette = {
    chainled::core::rgb565(255, 255, 255),  // white
    chainled::core::rgb565(255, 32, 32),    // red
    chainled::core::rgb565(32, 220, 32),    // green
    chainled::core::rgb565(64, 128, 255),   // blue
    chainled::core::rgb565(255, 220, 0),    // yellow
    chainled::core::rgb565(0, 220, 220),    // cyan
};

} // namespace viewmodel
