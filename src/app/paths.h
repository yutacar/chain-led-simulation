/**
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 *
 * Where the app keeps its own files on disk: $XDG_CONFIG_HOME/chainled/ if
 * set, else $HOME/.config/chainled/. Used for the single working animation
 * save file (this header) and i18n/settings.conf (src/i18n/settings.h).
 */
#pragma once

#include <string>

namespace app {

std::string config_dir();
std::string default_animation_save_path();

}  // namespace app
