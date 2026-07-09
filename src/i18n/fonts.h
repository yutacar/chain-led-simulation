/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 *
 * Per-language font selection. English never touches FreeType -- it always
 * uses the builtin bitmap Montserrat fonts; JA/ZH load subsetted Noto Sans
 * CJK fonts (see tools/generate_cjk_fonts.py) only for the sizes actually
 * used in this app (12/18 -- see src/view for call sites). Adapted from
 * Battleship's src/i18n/fonts.h.
 */
#pragma once

#include "strings.h"

#include "lvgl.h"

namespace app {
class AssetManager;
}

namespace i18n {

/// Loads the CJK font sets. Call once at startup, after AssetManager
/// exists and before building any UI. Idempotent.
void init_fonts(app::AssetManager& assets);

/// Font for the current language (i18n::current_language()) at `size`
/// (12 or 18 -- any other value silently falls back to Montserrat, which
/// tofus non-Latin text; see NOTES.md "Phase 6" pitfall list before adding
/// a new size).
const lv_font_t* font_for_size(std::int32_t size);

/// Font for an explicit language regardless of the active UI language --
/// used by the Settings screen so each language option always renders in
/// its own script.
const lv_font_t* font_for_language_and_size(Lang lang, std::int32_t size);

}  // namespace i18n
