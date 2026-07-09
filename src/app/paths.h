/**
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 *
 * Where the app keeps its own files on disk ($HOME/.config/chainled/...).
 * Full settings.conf key=value persistence is Phase 6 (i18n); for now this
 * only locates the single working animation save file the Editor/Title
 * screens read and write.
 */
#pragma once

#include <string>

namespace app {

std::string config_dir();
std::string default_animation_save_path();

}  // namespace app
