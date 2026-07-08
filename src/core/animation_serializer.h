/**
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 *
 * Text (de)serialization for Animation. This is the local save-file format;
 * it also doubles as the payload the BLE distribution transport (PLAN.md
 * §2.3) chunks and sends, so it deliberately has no external dependency
 * (no JSON library) and is safe to parse from untrusted/corrupted input --
 * any malformed input yields std::nullopt rather than throwing.
 */
#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "core/animation.h"

namespace chainled::core {

[[nodiscard]] std::string animation_to_text(const Animation& animation);
[[nodiscard]] std::optional<Animation> animation_from_text(std::string_view text);

bool save_animation_to_file(const Animation& animation, const std::string& path);
[[nodiscard]] std::optional<Animation> load_animation_from_file(const std::string& path);

}  // namespace chainled::core
