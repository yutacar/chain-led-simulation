/**
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 *
 * Debug-only screenshot capture. Renders the currently active LVGL screen
 * (lv_screen_active()) to a PNG file using lv_snapshot_take() + libpng.
 * Intended for UI verification during development: press F12, or drive it
 * via APP_SCRIPT/APP_SCREENSHOT_SEQUENCE (see app.cpp) from an automated
 * smoke-test run.
 */
#pragma once

#include <string>

namespace app {

/// Directory (relative to the current working directory, or absolute) where
/// screenshots are written. Created if missing.
constexpr const char* kScreenshotDir = "docs/screenshots";

/// Captures the currently active screen and writes it as a PNG file named
/// `<kScreenshotDir>/<name>.png`. Returns true on success. Safe to call
/// repeatedly (overwrites any existing file of the same name).
bool capture_screenshot(const std::string& name);

}  // namespace app
