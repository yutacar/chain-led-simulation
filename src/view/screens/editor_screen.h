/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "base_screen.h"

namespace screen {

/// Dot animation editor: 8x8 pixel grid (arrow keys + Enter to toggle),
/// a fixed preset color cycled with 'C', per-frame timeline thumbnails
/// (','/'.' to switch, N/D/Backspace to add/duplicate/remove), F/V to flip
/// the current frame, +/- to adjust its duration, S to save, P to preview
/// in the Simulator screen.
class EditorScreen : public BaseScreen {
public:
    EditorScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);

private:
    void build_content(lv_obj_t* content) override;
};

} // namespace screen
