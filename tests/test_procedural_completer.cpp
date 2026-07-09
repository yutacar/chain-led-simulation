/**
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */
#include "generator/procedural_completer.h"

#include <gtest/gtest.h>

namespace chainled::generator {
namespace {

using core::Animation;
using core::kFrameSize;
using core::kMaxFrames;
using core::kMinFrames;
using core::rgb565;

Animation make_seed_animation(int x, int y, core::Rgb565 color = rgb565(255, 255, 255)) {
    Animation animation;
    animation.frame(0).set_pixel(x, y, color);
    return animation;
}

TEST(ProceduralCompleter, GeneratesRequestedFrameCount) {
    ProceduralCompleter completer;
    Animation animation = make_seed_animation(3, 3);

    EXPECT_TRUE(completer.generate(animation, 10, ""));
    EXPECT_EQ(animation.frame_count(), 10);
}

TEST(ProceduralCompleter, PreservesSeedFrame) {
    ProceduralCompleter completer;
    Animation animation = make_seed_animation(3, 3, rgb565(255, 0, 0));
    const core::Frame seed = animation.frame(0);

    ASSERT_TRUE(completer.generate(animation, 6, ""));
    EXPECT_EQ(animation.frame(0), seed);
}

TEST(ProceduralCompleter, RejectsTargetCountBelowMinimum) {
    ProceduralCompleter completer;
    Animation animation = make_seed_animation(0, 0);

    EXPECT_FALSE(completer.generate(animation, kMinFrames - 1, ""));
    EXPECT_EQ(animation.frame_count(), 1);
}

TEST(ProceduralCompleter, RejectsTargetCountAboveMaximum) {
    ProceduralCompleter completer;
    Animation animation = make_seed_animation(0, 0);

    EXPECT_FALSE(completer.generate(animation, kMaxFrames + 1, ""));
    EXPECT_EQ(animation.frame_count(), 1);
}

TEST(ProceduralCompleter, TargetCountOfOneKeepsOnlySeed) {
    ProceduralCompleter completer;
    Animation animation = make_seed_animation(2, 2);

    ASSERT_TRUE(completer.generate(animation, 1, ""));
    EXPECT_EQ(animation.frame_count(), 1);
}

TEST(ProceduralCompleter, ProducesMotionAcrossFrames) {
    ProceduralCompleter completer;
    Animation animation = make_seed_animation(3, 3);

    ASSERT_TRUE(completer.generate(animation, 4, ""));
    // A single moving pixel should not sit in the same place every frame.
    bool any_frame_differs_from_seed = false;
    for (int i = 1; i < animation.frame_count(); ++i) {
        if (animation.frame(i) != animation.frame(0)) {
            any_frame_differs_from_seed = true;
            break;
        }
    }
    EXPECT_TRUE(any_frame_differs_from_seed);
}

TEST(ProceduralCompleter, IsDeterministic) {
    ProceduralCompleter completer;
    Animation a = make_seed_animation(1, 6);
    Animation b = make_seed_animation(1, 6);

    ASSERT_TRUE(completer.generate(a, 8, ""));
    ASSERT_TRUE(completer.generate(b, 8, ""));
    EXPECT_EQ(a, b);
}

TEST(ProceduralCompleter, BlankSeedProducesAllBlankFrames) {
    ProceduralCompleter completer;
    Animation animation;  // frame 0 starts blank

    ASSERT_TRUE(completer.generate(animation, 5, ""));
    for (int i = 0; i < animation.frame_count(); ++i) {
        EXPECT_EQ(animation.frame(i), core::Frame());
    }
}

TEST(ProceduralCompleter, CornerSeedNeverCrashesAndStaysWithinBounds) {
    ProceduralCompleter completer;
    Animation animation = make_seed_animation(kFrameSize - 1, kFrameSize - 1);

    ASSERT_TRUE(completer.generate(animation, kMaxFrames, ""));
    EXPECT_EQ(animation.frame_count(), kMaxFrames);
    // No assertion beyond "didn't crash and produced the right frame count" --
    // Frame::shift() already guarantees in-bounds pixels (Phase 1 tests).
}

TEST(ProceduralCompleter, RegeneratingWithFewerFramesShrinksAnimation) {
    ProceduralCompleter completer;
    Animation animation = make_seed_animation(4, 4);

    ASSERT_TRUE(completer.generate(animation, 8, ""));
    ASSERT_TRUE(completer.generate(animation, 3, ""));
    EXPECT_EQ(animation.frame_count(), 3);
}

}  // namespace
}  // namespace chainled::generator
