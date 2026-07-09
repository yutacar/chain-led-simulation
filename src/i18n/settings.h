/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 *
 * Settings persistence: ~/.config/chainled/settings.conf, flat key=value.
 * Adapted from Battleship's src/i18n/settings.h. Unset/missing file means
 * "use defaults" (English) rather than an error -- there is nothing to
 * migrate from on first run.
 */
#pragma once

#include <string>

#include "strings.h"

namespace i18n {

/// Reads settings.conf (if any) and applies language via set_language().
/// Call once at startup, before building any UI.
void load_settings();

/// set_language(lang) + rewrites settings.conf with every known key's
/// current value (never just the one being changed -- see NOTES.md "Phase
/// 6" for why: a partial-key save that clobbers other settings was a
/// mistake worth designing out from the start).
void save_language(Lang lang);

/// Exposed for tests (to redirect via XDG_CONFIG_HOME) and diagnostics.
std::string settings_file_path();

}  // namespace i18n
