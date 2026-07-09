/**
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */
#include "core/frame.h"

#include <gtest/gtest.h>

namespace chainled::core {
namespace {

TEST(Frame, StartsBlank) {
    Frame frame;
    for (int y = 0; y < kFrameSize; ++y) {
        for (int x = 0; x < kFrameSize; ++x) {
            EXPECT_EQ(frame.pixel(x, y), kOffColor);
        }
    }
}

TEST(Frame, SetAndGetPixelRoundTrip) {
    Frame frame;
    EXPECT_TRUE(frame.set_pixel(3, 4, 0xF800));
    EXPECT_EQ(frame.pixel(3, 4), 0xF800);
    // Neighboring pixels remain untouched.
    EXPECT_EQ(frame.pixel(3, 5), kOffColor);
    EXPECT_EQ(frame.pixel(2, 4), kOffColor);
}

TEST(Frame, OutOfBoundsAccessIsSafeNoOp) {
    Frame frame;
    EXPECT_FALSE(frame.set_pixel(-1, 0, 0xFFFF));
    EXPECT_FALSE(frame.set_pixel(0, -1, 0xFFFF));
    EXPECT_FALSE(frame.set_pixel(kFrameSize, 0, 0xFFFF));
    EXPECT_FALSE(frame.set_pixel(0, kFrameSize, 0xFFFF));
    EXPECT_EQ(frame.pixel(-1, 0), kOffColor);
    EXPECT_EQ(frame.pixel(kFrameSize, 0), kOffColor);
}

TEST(Frame, ClearResetsAllPixels) {
    Frame frame;
    frame.fill(0x1234);
    frame.clear();
    EXPECT_EQ(frame.pixel(0, 0), kOffColor);
    EXPECT_EQ(frame.pixel(7, 7), kOffColor);
}

TEST(Frame, FillSetsAllPixels) {
    Frame frame;
    frame.fill(0xABCD);
    for (int y = 0; y < kFrameSize; ++y) {
        for (int x = 0; x < kFrameSize; ++x) {
            EXPECT_EQ(frame.pixel(x, y), 0xABCD);
        }
    }
}

TEST(Frame, FlipHorizontalMirrorsColumns) {
    Frame frame;
    frame.set_pixel(0, 0, 0x1);
    frame.set_pixel(7, 0, 0x2);
    frame.flip_horizontal();
    EXPECT_EQ(frame.pixel(7, 0), 0x1);
    EXPECT_EQ(frame.pixel(0, 0), 0x2);
}

TEST(Frame, FlipVerticalMirrorsRows) {
    Frame frame;
    frame.set_pixel(0, 0, 0x1);
    frame.set_pixel(0, 7, 0x2);
    frame.flip_vertical();
    EXPECT_EQ(frame.pixel(0, 7), 0x1);
    EXPECT_EQ(frame.pixel(0, 0), 0x2);
}

TEST(Frame, ShiftTranslatesAndFillsVacatedCellsWithOff) {
    Frame frame;
    frame.set_pixel(0, 0, 0x9);
    frame.shift(1, 2);
    EXPECT_EQ(frame.pixel(1, 2), 0x9);
    EXPECT_EQ(frame.pixel(0, 0), kOffColor);
}

TEST(Frame, ShiftOffTheEdgeDropsPixels) {
    Frame frame;
    frame.set_pixel(7, 7, 0x9);
    frame.shift(1, 0);
    // Pixel would land at x=8, which is out of bounds -- dropped, not wrapped.
    for (int y = 0; y < kFrameSize; ++y) {
        for (int x = 0; x < kFrameSize; ++x) {
            EXPECT_EQ(frame.pixel(x, y), kOffColor);
        }
    }
}

TEST(Frame, Rgb565PacksComponents) {
    EXPECT_EQ(rgb565(0xFF, 0xFF, 0xFF), 0xFFFF);
    EXPECT_EQ(rgb565(0x00, 0x00, 0x00), 0x0000);
    EXPECT_EQ(rgb565(0xFF, 0x00, 0x00), 0xF800);
    EXPECT_EQ(rgb565(0x00, 0xFF, 0x00), 0x07E0);
    EXPECT_EQ(rgb565(0x00, 0x00, 0xFF), 0x001F);
}

TEST(Frame, EqualityComparesPixelContent) {
    Frame a;
    Frame b;
    EXPECT_EQ(a, b);
    a.set_pixel(2, 2, 0x42);
    EXPECT_NE(a, b);
    b.set_pixel(2, 2, 0x42);
    EXPECT_EQ(a, b);
}

}  // namespace
}  // namespace chainled::core
