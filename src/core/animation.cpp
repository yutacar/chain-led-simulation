/**
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */
#include "core/animation.h"

namespace chainled::core {

Animation::Animation() { frames_.emplace_back(); }

bool Animation::set_frame_duration_ms(int index, int duration_ms) {
    if (!valid_index(index)) {
        return false;
    }
    frames_[static_cast<std::size_t>(index)].duration_ms = clamp_frame_duration_ms(duration_ms);
    return true;
}

bool Animation::add_blank_frame() {
    if (frame_count() >= kMaxFrames) {
        return false;
    }
    frames_.emplace_back();
    return true;
}

bool Animation::duplicate_frame(int index) {
    if (!valid_index(index) || frame_count() >= kMaxFrames) {
        return false;
    }
    frames_.insert(frames_.begin() + index + 1, frames_[static_cast<std::size_t>(index)]);
    return true;
}

bool Animation::remove_frame(int index) {
    if (!valid_index(index) || frame_count() <= kMinFrames) {
        return false;
    }
    frames_.erase(frames_.begin() + index);
    return true;
}

bool Animation::move_frame(int from, int to) {
    if (!valid_index(from) || !valid_index(to)) {
        return false;
    }
    if (from == to) {
        return true;
    }
    AnimationFrame moved = frames_[static_cast<std::size_t>(from)];
    frames_.erase(frames_.begin() + from);
    frames_.insert(frames_.begin() + to, moved);
    return true;
}

int Animation::total_duration_ms() const {
    int total = 0;
    for (const auto& f : frames_) {
        total += f.duration_ms;
    }
    return total;
}

}  // namespace chainled::core
