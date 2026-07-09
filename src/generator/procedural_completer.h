/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 *
 * ProceduralCompleter: the default, always-available AnimationGenerator.
 * Deterministic and dependency-free -- see PLAN.md §2.4/§4. Bounces the
 * seed frame's pattern off the 8x8 grid edges (translate, reversing
 * direction on either axis whenever the pattern's bounding box would leave
 * the grid), producing a simple but immediately recognizable "the shape
 * moves" animation from a single drawn frame.
 */
#pragma once

#include "animation_generator.h"

namespace chainled::generator {

class ProceduralCompleter : public AnimationGenerator {
public:
    bool generate(core::Animation& animation, int target_frame_count, const std::string& hint) override;
};

}  // namespace chainled::generator
