/**
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */
#include "chain/frame_codec.h"

#include <gtest/gtest.h>

namespace chainled::chain {
namespace {

using chainled::core::Frame;
using chainled::core::kFrameSize;
using chainled::core::rgb565;

TEST(FrameCodec, MonoEncodesBlankFrameAsAllZero) {
    Frame frame;
    const auto buffer = encode_mono_display_buffer(frame);
    for (auto byte : buffer) {
        EXPECT_EQ(byte, 0);
    }
}

TEST(FrameCodec, MonoEncodesEachRowIndependently) {
    Frame frame;
    frame.set_pixel(0, 0, rgb565(255, 255, 255));  // row 0, column 0
    frame.set_pixel(7, 3, rgb565(255, 255, 255));  // row 3, column 7

    const auto buffer = encode_mono_display_buffer(frame);
    ASSERT_EQ(buffer.size(), static_cast<std::size_t>(kFrameSize));
    EXPECT_EQ(buffer[0], 0x01);  // bit 0 set
    EXPECT_EQ(buffer[3], 0x80);  // bit 7 set
    for (int y = 0; y < kFrameSize; ++y) {
        if (y != 0 && y != 3) {
            EXPECT_EQ(buffer[static_cast<std::size_t>(y)], 0);
        }
    }
}

TEST(FrameCodec, MonoTreatsAnyNonzeroColorAsLit) {
    Frame frame;
    // rgb565(1, 0, 0) would quantize to exactly 0 (5-bit red channel drops
    // values below 8) -- use a raw Rgb565 value that isn't a real color but
    // is guaranteed nonzero, to test the "any nonzero value" contract
    // itself rather than a specific color's quantization.
    frame.set_pixel(2, 2, static_cast<core::Rgb565>(1));
    const auto buffer = encode_mono_display_buffer(frame);
    EXPECT_EQ(buffer[2], 1U << 2);
}

TEST(FrameCodec, RgbEncodesBlankFrameAsAllZero) {
    Frame frame;
    const auto buffer = encode_rgb_display_buffer(frame);
    for (auto byte : buffer) {
        EXPECT_EQ(byte, 0);
    }
}

TEST(FrameCodec, RgbEncodesEachPixelLittleEndian) {
    Frame frame;
    frame.set_pixel(0, 0, rgb565(255, 0, 0));  // 0xF800
    frame.set_pixel(1, 0, rgb565(0, 255, 0));  // 0x07E0

    const auto buffer = encode_rgb_display_buffer(frame);
    ASSERT_EQ(buffer.size(), static_cast<std::size_t>(kFrameSize * kFrameSize * 2));
    EXPECT_EQ(buffer[0], 0x00);  // low byte of 0xF800
    EXPECT_EQ(buffer[1], 0xF8);  // high byte
    EXPECT_EQ(buffer[2], 0xE0);  // low byte of 0x07E0
    EXPECT_EQ(buffer[3], 0x07);  // high byte
}

TEST(FrameCodec, RgbPixelOffsetIsRowMajor) {
    Frame frame;
    frame.set_pixel(3, 2, rgb565(0, 0, 255));  // row 2, column 3 -> pixel index 2*8+3=19

    const auto buffer = encode_rgb_display_buffer(frame);
    const std::size_t offset = 19 * 2;
    EXPECT_EQ(buffer[offset], 0x1F);
    EXPECT_EQ(buffer[offset + 1], 0x00);
}

}  // namespace
}  // namespace chainled::chain
