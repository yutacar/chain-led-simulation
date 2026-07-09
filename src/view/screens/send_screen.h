/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "base_screen.h"

namespace screen {

/// Listens on the TCP dev-transfer port and streams the current animation
/// to whichever Receive screen connects (PLAN.md §2.3/§2.5). Desktop/LAN
/// dev path; BLE is the real-device default (Phase 7).
class SendScreen : public BaseScreen {
public:
    SendScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);

private:
    void build_content(lv_obj_t* content) override;
};

} // namespace screen
