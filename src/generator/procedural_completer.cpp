/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */
#include "procedural_completer.h"

#include <algorithm>
#include <optional>

namespace chainled::generator {
namespace {

struct BoundingBox {
    int min_x;
    int max_x;
    int min_y;
    int max_y;
};

std::optional<BoundingBox> bounding_box(const core::Frame& frame) {
    std::optional<BoundingBox> box;
    for (int y = 0; y < core::kFrameSize; ++y) {
        for (int x = 0; x < core::kFrameSize; ++x) {
            if (frame.pixel(x, y) == core::kOffColor) {
                continue;
            }
            if (!box.has_value()) {
                box = BoundingBox{x, x, y, y};
            } else {
                box->min_x = std::min(box->min_x, x);
                box->max_x = std::max(box->max_x, x);
                box->min_y = std::min(box->min_y, y);
                box->max_y = std::max(box->max_y, y);
            }
        }
    }
    return box;
}

}  // namespace

bool ProceduralCompleter::generate(core::Animation& animation, int target_frame_count, const std::string& hint) {
    (void)hint;  // ProceduralCompleter has no way to act on free-text intent.

    if (target_frame_count < core::kMinFrames || target_frame_count > core::kMaxFrames) {
        return false;
    }

    const core::Frame seed = animation.frame(0);
    while (animation.frame_count() > 1) {
        animation.remove_frame(animation.frame_count() - 1);
    }
    animation.frame(0) = seed;

    auto box = bounding_box(seed);
    core::Frame current = seed;
    int dx = 1;
    int dy = 1;

    for (int i = 1; i < target_frame_count; ++i) {
        if (box.has_value()) {
            if (box->min_x + dx < 0 || box->max_x + dx >= core::kFrameSize) {
                dx = -dx;
            }
            if (box->min_y + dy < 0 || box->max_y + dy >= core::kFrameSize) {
                dy = -dy;
            }
            current.shift(dx, dy);
            box->min_x += dx;
            box->max_x += dx;
            box->min_y += dy;
            box->max_y += dy;
        }
        animation.add_blank_frame();
        animation.frame(animation.frame_count() - 1) = current;
    }

    return true;
}

}  // namespace chainled::generator
