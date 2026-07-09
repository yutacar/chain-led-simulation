/**
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */
#include "paths.h"

#include <cstdlib>
#include <filesystem>

namespace app {

std::string config_dir() {
    const char* home = std::getenv("HOME");
    std::filesystem::path base = (home && *home) ? std::filesystem::path(home) : std::filesystem::path(".");
    return (base / ".config" / "chainled").string();
}

std::string default_animation_save_path() {
    return (std::filesystem::path(config_dir()) / "animation.txt").string();
}

}  // namespace app
