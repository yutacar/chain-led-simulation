/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "base_screen.h"

namespace screen {

/// Connects to 127.0.0.1 on the TCP dev-transfer port and receives an
/// animation from whichever Send screen is listening (PLAN.md §2.3/§2.5).
/// On success, replaces the animation currently open in the Editor.
class ReceiveScreen : public BaseScreen {
public:
    ReceiveScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);

private:
    void build_content(lv_obj_t* content) override;
};

} // namespace screen
