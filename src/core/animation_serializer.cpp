/**
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 *
 * Wire/file format (one animation per document):
 *
 *   ChainLEDAnimation/1
 *   frames <N>
 *   frame 0 <duration_ms> <256 hex chars, 4 per pixel, row-major RGB565>
 *   frame 1 <duration_ms> <...>
 *   ...
 *   frame <N-1> <duration_ms> <...>
 *
 * Frame indices must appear in order starting at 0. Any structural
 * deviation is treated as corrupt input (returns std::nullopt) rather than
 * a partial/best-effort parse, since this format also carries data arriving
 * over BLE from another device.
 */
#include "core/animation_serializer.h"

#include <cctype>
#include <fstream>
#include <sstream>

namespace chainled::core {

namespace {

constexpr std::string_view kMagic = "ChainLEDAnimation/1";
constexpr int kHexCharsPerFrame = kFrameSize * kFrameSize * 4;

std::string pixel_to_hex(Rgb565 color) {
    static constexpr char kDigits[] = "0123456789abcdef";
    std::string out(4, '0');
    for (int i = 3; i >= 0; --i) {
        out[static_cast<std::size_t>(i)] = kDigits[color & 0xF];
        color = static_cast<Rgb565>(color >> 4);
    }
    return out;
}

bool is_hex_digit(char c) { return std::isxdigit(static_cast<unsigned char>(c)) != 0; }

std::optional<Rgb565> parse_hex4(std::string_view s) {
    if (s.size() != 4) {
        return std::nullopt;
    }
    Rgb565 value = 0;
    for (char c : s) {
        if (!is_hex_digit(c)) {
            return std::nullopt;
        }
        value = static_cast<Rgb565>(value << 4);
        if (c >= '0' && c <= '9') {
            value = static_cast<Rgb565>(value + static_cast<Rgb565>(c - '0'));
        } else {
            value = static_cast<Rgb565>(value + static_cast<Rgb565>(std::tolower(static_cast<unsigned char>(c)) - 'a' + 10));
        }
    }
    return value;
}

std::optional<Frame> parse_frame_hex(std::string_view hex) {
    if (static_cast<int>(hex.size()) != kHexCharsPerFrame) {
        return std::nullopt;
    }
    Frame frame;
    for (int y = 0; y < kFrameSize; ++y) {
        for (int x = 0; x < kFrameSize; ++x) {
            const std::size_t offset = static_cast<std::size_t>((y * kFrameSize + x) * 4);
            const auto color = parse_hex4(hex.substr(offset, 4));
            if (!color.has_value()) {
                return std::nullopt;
            }
            frame.set_pixel(x, y, *color);
        }
    }
    return frame;
}

}  // namespace

std::string animation_to_text(const Animation& animation) {
    std::ostringstream out;
    out << kMagic << '\n';
    out << "frames " << animation.frame_count() << '\n';
    for (int i = 0; i < animation.frame_count(); ++i) {
        out << "frame " << i << ' ' << animation.frame_duration_ms(i) << ' ';
        const Frame& frame = animation.frame(i);
        for (int y = 0; y < kFrameSize; ++y) {
            for (int x = 0; x < kFrameSize; ++x) {
                out << pixel_to_hex(frame.pixel(x, y));
            }
        }
        out << '\n';
    }
    return out.str();
}

std::optional<Animation> animation_from_text(std::string_view text) {
    std::istringstream in{std::string(text)};
    std::string line;

    if (!std::getline(in, line) || line != kMagic) {
        return std::nullopt;
    }

    if (!std::getline(in, line)) {
        return std::nullopt;
    }
    std::istringstream frames_line(line);
    std::string frames_tag;
    int frame_count = 0;
    if (!(frames_line >> frames_tag >> frame_count) || frames_tag != "frames") {
        return std::nullopt;
    }
    if (frame_count < kMinFrames || frame_count > kMaxFrames) {
        return std::nullopt;
    }

    std::vector<AnimationFrame> parsed;
    parsed.reserve(static_cast<std::size_t>(frame_count));

    for (int expected_index = 0; expected_index < frame_count; ++expected_index) {
        if (!std::getline(in, line)) {
            return std::nullopt;
        }
        std::istringstream frame_line(line);
        std::string frame_tag;
        int index = -1;
        int duration_ms = 0;
        std::string hex;
        if (!(frame_line >> frame_tag >> index >> duration_ms >> hex) || frame_tag != "frame") {
            return std::nullopt;
        }
        if (index != expected_index) {
            return std::nullopt;
        }
        auto frame = parse_frame_hex(hex);
        if (!frame.has_value()) {
            return std::nullopt;
        }
        parsed.push_back(AnimationFrame{*frame, clamp_frame_duration_ms(duration_ms)});
    }

    Animation animation;
    while (animation.frame_count() < static_cast<int>(parsed.size())) {
        animation.add_blank_frame();
    }
    for (std::size_t i = 0; i < parsed.size(); ++i) {
        animation.frame(static_cast<int>(i)) = parsed[i].frame;
        animation.set_frame_duration_ms(static_cast<int>(i), parsed[i].duration_ms);
    }
    return animation;
}

bool save_animation_to_file(const Animation& animation, const std::string& path) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }
    out << animation_to_text(animation);
    return out.good();
}

std::optional<Animation> load_animation_from_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return std::nullopt;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return animation_from_text(buffer.str());
}

}  // namespace chainled::core
