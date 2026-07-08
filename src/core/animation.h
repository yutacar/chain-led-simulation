/**
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 *
 * Animation: an ordered sequence of Frame + per-frame display duration.
 * Pure C++, zero LVGL/platform dependency (PLAN.md §2.1).
 */
#pragma once

#include <vector>

#include "core/frame.h"

namespace chainled::core {

/// Hardware/UX cap: matches the app brief ("dot animations up to 10 frames").
constexpr int kMaxFrames = 10;
/// An animation always has at least one frame (the one the user is drawing).
constexpr int kMinFrames = 1;

constexpr int kDefaultFrameDurationMs = 200;
constexpr int kMinFrameDurationMs = 50;
constexpr int kMaxFrameDurationMs = 5000;

struct AnimationFrame {
    Frame frame;
    int duration_ms = kDefaultFrameDurationMs;

    bool operator==(const AnimationFrame& other) const {
        return frame == other.frame && duration_ms == other.duration_ms;
    }
};

/// Clamps a requested duration into [kMinFrameDurationMs, kMaxFrameDurationMs].
constexpr int clamp_frame_duration_ms(int duration_ms) {
    if (duration_ms < kMinFrameDurationMs) return kMinFrameDurationMs;
    if (duration_ms > kMaxFrameDurationMs) return kMaxFrameDurationMs;
    return duration_ms;
}

class Animation {
public:
    /// Starts with exactly one blank frame -- the frame the user draws first.
    Animation();

    [[nodiscard]] int frame_count() const { return static_cast<int>(frames_.size()); }

    [[nodiscard]] bool valid_index(int index) const { return index >= 0 && index < frame_count(); }

    [[nodiscard]] const Frame& frame(int index) const { return frames_[static_cast<std::size_t>(index)].frame; }
    Frame& frame(int index) { return frames_[static_cast<std::size_t>(index)].frame; }

    [[nodiscard]] int frame_duration_ms(int index) const {
        return frames_[static_cast<std::size_t>(index)].duration_ms;
    }
    /// Returns false for an out-of-range index; otherwise clamps and applies.
    bool set_frame_duration_ms(int index, int duration_ms);

    /// Appends a blank frame at the end. Returns false if already at kMaxFrames.
    bool add_blank_frame();

    /// Inserts a copy of `index` immediately after it. Returns false if `index`
    /// is invalid or the animation is already at kMaxFrames.
    bool duplicate_frame(int index);

    /// Removes the frame at `index`. Returns false if `index` is invalid or
    /// removing it would drop the animation below kMinFrames.
    bool remove_frame(int index);

    /// Moves the frame at `from` to position `to`, shifting the others.
    /// Returns false if either index is invalid.
    bool move_frame(int from, int to);

    [[nodiscard]] int total_duration_ms() const;

    bool operator==(const Animation& other) const { return frames_ == other.frames_; }
    bool operator!=(const Animation& other) const { return !(*this == other); }

    [[nodiscard]] const std::vector<AnimationFrame>& frames() const { return frames_; }

private:
    std::vector<AnimationFrame> frames_;
};

}  // namespace chainled::core
