/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "base_screen.h"

namespace screen {

/// Language selection: EN/JA/ZH, one button each, rendered in their own
/// script regardless of the active UI language (see i18n::font_for_language_
/// and_size). Selecting a language takes effect immediately -- see
/// BaseViewModel::set_language, which rebuilds every screen via
/// ScreenManager::reload_current_screen().
class SettingsScreen : public BaseScreen {
public:
    SettingsScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);

private:
    void build_content(lv_obj_t* content) override;
};

} // namespace screen
