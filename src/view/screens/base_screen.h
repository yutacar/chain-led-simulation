/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 *
 * Full-screen base for every ChainLED screen. Unlike the imported
 * template's original BaseScreen (which reserves a 30px TitleBar + 30px
 * NavBar), this app needs the whole 320x170 for the editor grid/timeline
 * and drives navigation entirely via keyboard, so there is no on-screen
 * nav chrome to reserve space for (same choice Battleship made for the
 * same template -- see PLAN.md "Battleship と同一方針").
 */

#pragma once

#include "base_viewmodel.h"
#include "lvgl.h"

namespace app {
class AssetManager;
}

namespace screen {

class BaseScreen {
public:
    BaseScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);
    virtual ~BaseScreen();

    BaseScreen(const BaseScreen&) = delete;
    BaseScreen& operator=(const BaseScreen&) = delete;

    void init();
    lv_obj_t* root() const;

    /// Application-wide input group, shared by every screen instance and
    /// owned by ScreenManager. Screens must add their focusable widgets to
    /// this group from build_content(); ScreenManager clears its contents
    /// on every screen switch so screens never see stale focusable objects
    /// from the previous screen.
    static void set_shared_group(lv_group_t* group);
    static lv_group_t* shared_group();

    /// Makes arrow keys move keyboard focus while `obj` is focused: Right/
    /// Down focus the next object in the shared group, Left/Up the previous
    /// one. For multi-button screens (e.g. Title) whose widgets are plain
    /// buttons that otherwise ignore arrow keys. Do NOT use on the pixel
    /// grid, whose own LV_EVENT_KEY handler consumes arrows for cursor
    /// movement.
    static void enable_arrow_focus_navigation(lv_obj_t* obj);

protected:
    virtual void build_content(lv_obj_t* content) = 0;

    viewmodel::BaseViewModel& view_model();
    app::AssetManager& assets();

private:
    viewmodel::BaseViewModel& view_model_;
    app::AssetManager& assets_;
    lv_obj_t* root_{nullptr};
};

} // namespace screen
