/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "base_screen.h"

namespace screen {

/// Root screen: New Animation / Load Animation (only shown if a save file
/// exists) / Quit. Arrow keys move focus between buttons, Enter activates.
class TitleScreen : public BaseScreen {
public:
    TitleScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);

private:
    void build_content(lv_obj_t* content) override;
};

} // namespace screen
