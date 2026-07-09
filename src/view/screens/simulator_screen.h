/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "base_screen.h"

namespace screen {

/// On-screen playback preview of the current animation -- PLAN.md §2.2's
/// SimulatorChainOutput. Loops through the frames using each frame's own
/// duration; 'M' toggles between showing the actual RGB565 colors and a
/// mono (on/off) preview of how it would look on a Chain Mono unit.
class SimulatorScreen : public BaseScreen {
public:
    SimulatorScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);

private:
    void build_content(lv_obj_t* content) override;
};

} // namespace screen
