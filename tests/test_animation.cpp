/**
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */
#include "core/animation.h"

#include <gtest/gtest.h>

namespace chainled::core {
namespace {

TEST(Animation, StartsWithOneBlankFrame) {
    Animation animation;
    EXPECT_EQ(animation.frame_count(), 1);
    EXPECT_EQ(animation.frame_duration_ms(0), kDefaultFrameDurationMs);
}

TEST(Animation, AddBlankFrameAppends) {
    Animation animation;
    EXPECT_TRUE(animation.add_blank_frame());
    EXPECT_EQ(animation.frame_count(), 2);
}

TEST(Animation, AddBlankFrameStopsAtMax) {
    Animation animation;
    while (animation.frame_count() < kMaxFrames) {
        EXPECT_TRUE(animation.add_blank_frame());
    }
    EXPECT_EQ(animation.frame_count(), kMaxFrames);
    EXPECT_FALSE(animation.add_blank_frame());
    EXPECT_EQ(animation.frame_count(), kMaxFrames);
}

TEST(Animation, DuplicateFrameInsertsCopyAfterIndex) {
    Animation animation;
    animation.frame(0).set_pixel(0, 0, 0x42);
    EXPECT_TRUE(animation.duplicate_frame(0));
    EXPECT_EQ(animation.frame_count(), 2);
    EXPECT_EQ(animation.frame(1).pixel(0, 0), 0x42);
}

TEST(Animation, DuplicateFrameRejectsInvalidIndex) {
    Animation animation;
    EXPECT_FALSE(animation.duplicate_frame(-1));
    EXPECT_FALSE(animation.duplicate_frame(1));
    EXPECT_EQ(animation.frame_count(), 1);
}

TEST(Animation, DuplicateFrameStopsAtMax) {
    Animation animation;
    while (animation.frame_count() < kMaxFrames) {
        EXPECT_TRUE(animation.duplicate_frame(0));
    }
    EXPECT_FALSE(animation.duplicate_frame(0));
    EXPECT_EQ(animation.frame_count(), kMaxFrames);
}

TEST(Animation, RemoveFrameDeletesAtIndex) {
    Animation animation;
    animation.add_blank_frame();
    animation.frame(1).set_pixel(0, 0, 0x9);
    EXPECT_TRUE(animation.remove_frame(0));
    EXPECT_EQ(animation.frame_count(), 1);
    EXPECT_EQ(animation.frame(0).pixel(0, 0), 0x9);
}

TEST(Animation, RemoveFrameCannotDropBelowMinimum) {
    Animation animation;
    EXPECT_EQ(animation.frame_count(), kMinFrames);
    EXPECT_FALSE(animation.remove_frame(0));
    EXPECT_EQ(animation.frame_count(), kMinFrames);
}

TEST(Animation, RemoveFrameRejectsInvalidIndex) {
    Animation animation;
    animation.add_blank_frame();
    EXPECT_FALSE(animation.remove_frame(-1));
    EXPECT_FALSE(animation.remove_frame(2));
    EXPECT_EQ(animation.frame_count(), 2);
}

TEST(Animation, MoveFrameReorders) {
    Animation animation;
    animation.frame(0).set_pixel(0, 0, 0x1);
    animation.add_blank_frame();
    animation.frame(1).set_pixel(0, 0, 0x2);
    animation.add_blank_frame();
    animation.frame(2).set_pixel(0, 0, 0x3);

    EXPECT_TRUE(animation.move_frame(0, 2));
    EXPECT_EQ(animation.frame(0).pixel(0, 0), 0x2);
    EXPECT_EQ(animation.frame(1).pixel(0, 0), 0x3);
    EXPECT_EQ(animation.frame(2).pixel(0, 0), 0x1);
}

TEST(Animation, MoveFrameRejectsInvalidIndex) {
    Animation animation;
    animation.add_blank_frame();
    EXPECT_FALSE(animation.move_frame(-1, 0));
    EXPECT_FALSE(animation.move_frame(0, 5));
}

TEST(Animation, SetFrameDurationClampsToValidRange) {
    Animation animation;
    EXPECT_TRUE(animation.set_frame_duration_ms(0, 1));
    EXPECT_EQ(animation.frame_duration_ms(0), kMinFrameDurationMs);

    EXPECT_TRUE(animation.set_frame_duration_ms(0, 999999));
    EXPECT_EQ(animation.frame_duration_ms(0), kMaxFrameDurationMs);

    EXPECT_TRUE(animation.set_frame_duration_ms(0, 300));
    EXPECT_EQ(animation.frame_duration_ms(0), 300);
}

TEST(Animation, SetFrameDurationRejectsInvalidIndex) {
    Animation animation;
    EXPECT_FALSE(animation.set_frame_duration_ms(-1, 300));
    EXPECT_FALSE(animation.set_frame_duration_ms(1, 300));
}

TEST(Animation, TotalDurationSumsAllFrames) {
    Animation animation;
    animation.set_frame_duration_ms(0, 100);
    animation.add_blank_frame();
    animation.set_frame_duration_ms(1, 150);
    EXPECT_EQ(animation.total_duration_ms(), 250);
}

TEST(Animation, EqualityComparesFramesAndDurations) {
    Animation a;
    Animation b;
    EXPECT_EQ(a, b);
    a.set_frame_duration_ms(0, 300);
    EXPECT_NE(a, b);
}

}  // namespace
}  // namespace chainled::core
