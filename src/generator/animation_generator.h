/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 *
 * AnimationGenerator: "draw frame 1, generate the rest" (PLAN.md §2.4).
 * Pure C++, zero LVGL/platform dependency -- only ever sees an Animation.
 *
 * ProceduralCompleter is the only implementation shipped so far. A
 * llama.cpp-backed LlmCompleter was evaluated (see NOTES.md "Phase 3:
 * Go/No-Go" for the desktop benchmark and reasoning) and deferred: the
 * candidate model's weights alone (~258MB) would consume over half of the
 * CardputerZero's 512MB total RAM before the OS/LVGL/app get anything,
 * which is a hard resource-budget problem no amount of generation-speed
 * tuning fixes. This interface is the extension point for revisiting that
 * once real hardware (or a much smaller model) is available -- callers
 * only depend on AnimationGenerator, not on which concrete completer is
 * active.
 */
#pragma once

#include <string>

#include "core/animation.h"

namespace chainled::generator {

class AnimationGenerator {
public:
    virtual ~AnimationGenerator() = default;

    /// Rebuilds `animation` to have exactly `target_frame_count` frames,
    /// using its current frame 0 as the seed (any existing frames beyond
    /// index 0 are discarded and regenerated). `hint` is an optional
    /// free-text description of the desired motion; implementations that
    /// can't use it (e.g. ProceduralCompleter) ignore it.
    ///
    /// Returns false (animation left untouched) if target_frame_count is
    /// outside [core::kMinFrames, core::kMaxFrames].
    virtual bool generate(core::Animation& animation, int target_frame_count, const std::string& hint) = 0;
};

}  // namespace chainled::generator
