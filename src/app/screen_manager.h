/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "asset_manager.h"
#include "base_screen.h"
#include "base_viewmodel.h"
#include "lvgl.h"

#include <memory>

namespace app {

class ScreenManager {
public:
    ScreenManager(viewmodel::BaseViewModel& view_model, AssetManager& assets);
    ~ScreenManager();

    ScreenManager(const ScreenManager&) = delete;
    ScreenManager& operator=(const ScreenManager&) = delete;

    void start();
    void request_page(model::AppPage page);
    void flush_requested_page();
    lv_obj_t* current_screen() const;

    /// Shared keypad focus group used by every screen (arrow keys move focus
    /// between focusable widgets, Enter/OK activates them). Must be attached
    /// to the keypad indev by the caller (see app.cpp) so it works
    /// regardless of which platform key input backend is active.
    lv_group_t* input_group() const { return input_group_; }

    // ---- Esc-key convention ----
    /// Handles the Esc/Home key convention centrally, independent of which
    /// widget currently has keypad focus. Installed as an LV_EVENT_KEY
    /// callback on every keypad indev (see app.cpp):
    ///   - short Esc: go back one level;
    ///   - Esc held ~1s, or Home: quit.
    /// Long-press detection is done here (press timestamp + repeated
    /// LV_EVENT_KEY deliveries while the key stays pressed) because LVGL's
    /// built-in keypad long-press events only fire for LV_KEY_ENTER.
    void attach_esc_handling(lv_indev_t* indev);

    /// Back one level from the current screen. Public so the desktop
    /// APP_SCRIPT harness can drive it; normally invoked only from the
    /// indev key callback.
    void handle_short_esc();

private:
    void show_screen_for_page(model::AppPage page);
    void load_screen(std::unique_ptr<screen::BaseScreen> next_screen);
    void on_indev_key(lv_indev_t* indev);
    static void indev_key_cb(lv_event_t* event);

    viewmodel::BaseViewModel& view_model_;
    AssetManager& assets_;
    lv_group_t* input_group_ = nullptr;
    std::unique_ptr<screen::BaseScreen> current_screen_;
    lv_observer_t* page_observer_ = nullptr;
    model::AppPage requested_page_ = model::AppPage::Title;
    model::AppPage loaded_page_ = model::AppPage::Title;
    bool has_loaded_page_ = false;
    bool page_switch_scheduled_ = false;

    // ---- Esc/Home key tracking (see attach_esc_handling) ----
    bool esc_down_ = false;
    bool esc_long_fired_ = false;
    bool home_down_ = false;
    uint32_t esc_pressed_at_ = 0;
};

} // namespace app
