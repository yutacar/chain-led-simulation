/**
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */
#include "core/animation_serializer.h"

#include <gtest/gtest.h>

#include <filesystem>

namespace chainled::core {
namespace {

TEST(AnimationSerializer, RoundTripsThroughText) {
    Animation animation;
    animation.frame(0).set_pixel(0, 0, 0xF800);
    animation.frame(0).set_pixel(7, 7, 0x07E0);
    animation.set_frame_duration_ms(0, 250);
    animation.add_blank_frame();
    animation.frame(1).set_pixel(3, 3, 0x001F);

    const std::string text = animation_to_text(animation);
    const auto restored = animation_from_text(text);

    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(*restored, animation);
}

TEST(AnimationSerializer, RoundTripsMaxFrames) {
    Animation animation;
    while (animation.frame_count() < kMaxFrames) {
        animation.add_blank_frame();
    }
    for (int i = 0; i < animation.frame_count(); ++i) {
        animation.frame(i).set_pixel(i % kFrameSize, i % kFrameSize, static_cast<Rgb565>(0x100 * i));
    }

    const auto restored = animation_from_text(animation_to_text(animation));
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(*restored, animation);
}

TEST(AnimationSerializer, RejectsEmptyInput) { EXPECT_FALSE(animation_from_text("").has_value()); }

TEST(AnimationSerializer, RejectsWrongMagic) {
    EXPECT_FALSE(animation_from_text("NotChainLED/1\nframes 1\n").has_value());
}

TEST(AnimationSerializer, RejectsFrameCountAboveMax) {
    std::string text = "ChainLEDAnimation/1\nframes 11\n";
    EXPECT_FALSE(animation_from_text(text).has_value());
}

TEST(AnimationSerializer, RejectsFrameCountBelowMin) {
    std::string text = "ChainLEDAnimation/1\nframes 0\n";
    EXPECT_FALSE(animation_from_text(text).has_value());
}

TEST(AnimationSerializer, RejectsTruncatedFrameData) {
    Animation animation;
    std::string text = animation_to_text(animation);
    // Chop off part of the last frame's hex payload.
    text.resize(text.size() - 10);
    EXPECT_FALSE(animation_from_text(text).has_value());
}

TEST(AnimationSerializer, RejectsNonHexCharactersInFrameData) {
    std::string text = "ChainLEDAnimation/1\nframes 1\nframe 0 200 z" + std::string(255, '0') + "\n";
    EXPECT_FALSE(animation_from_text(text).has_value());
}

TEST(AnimationSerializer, RejectsOutOfOrderFrameIndices) {
    std::string text =
        "ChainLEDAnimation/1\n"
        "frames 2\n"
        "frame 1 200 " +
        std::string(256, '0') +
        "\n"
        "frame 0 200 " +
        std::string(256, '0') + "\n";
    EXPECT_FALSE(animation_from_text(text).has_value());
}

TEST(AnimationSerializer, RejectsMissingFrameLines) {
    std::string text = "ChainLEDAnimation/1\nframes 2\nframe 0 200 " + std::string(256, '0') + "\n";
    EXPECT_FALSE(animation_from_text(text).has_value());
}

TEST(AnimationSerializer, SavesAndLoadsFromFile) {
    Animation animation;
    animation.frame(0).set_pixel(1, 1, 0x1234);

    const std::string path =
        (std::filesystem::temp_directory_path() / "chainled_animation_serializer_test.txt").string();
    ASSERT_TRUE(save_animation_to_file(animation, path));

    const auto loaded = load_animation_from_file(path);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(*loaded, animation);

    std::filesystem::remove(path);
}

TEST(AnimationSerializer, LoadFromMissingFileReturnsNullopt) {
    EXPECT_FALSE(load_animation_from_file("/nonexistent/path/animation.txt").has_value());
}

}  // namespace
}  // namespace chainled::core
