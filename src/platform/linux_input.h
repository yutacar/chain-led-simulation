/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <functional>

#include "lvgl.h"

namespace platform {

/// Called once for every evdev keypad indev init_key_input() creates, right
/// after it is registered with `display`. app.cpp uses this to attach the
/// Esc/Home key convention (ScreenManager::attach_esc_handling) to each
/// physical keypad it discovers, without this platform-layer header needing
/// to know about ScreenManager or anything else app-specific.
using KeypadIndevCallback = std::function<void(lv_indev_t*)>;

/// Linux (CM0 device) key input: auto-discovers evdev keypad devices under
/// /dev/input (or APP_KEY_INPUT_DEVICE if set) and creates one LVGL keypad
/// indev per usable device, attached to `display`. Every created indev is
/// bound to the LVGL default group (lv_group_get_default()) via
/// lv_indev_set_group -- callers (app.cpp) must call lv_group_set_default()
/// with the app's shared focus group *before* calling this. No-op on
/// desktop builds (USE_DESKTOP), which use lv_sdl_keyboard_create() directly
/// instead (see app.cpp). If `on_indev_created` is set, it is invoked once
/// per created indev.
void init_key_input(lv_display_t* display, const KeypadIndevCallback& on_indev_created = {});

} // namespace platform
