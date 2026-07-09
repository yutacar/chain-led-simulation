/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */
#include "fonts.h"

#include "asset_manager.h"

namespace i18n {
namespace {

struct CjkFontSet {
    lv_font_t* size_12 = nullptr;
    lv_font_t* size_18 = nullptr;
};

bool g_initialized = false;
CjkFontSet g_japanese;
CjkFontSet g_chinese;

const lv_font_t* montserrat_for_size(std::int32_t size) {
    switch (size) {
        case 18:
            return &lv_font_montserrat_18;
        case 12:
        default:
            return &lv_font_montserrat_12;
    }
}

/// Embedded ASCII (digits, key names like "Esc"/"Enter", LV_SYMBOL_* arrow
/// glyphs) renders via the bitmap Montserrat font instead of being
/// FreeType-rasterized, by chaining it as the CJK font's fallback.
void wire_fallback(lv_font_t* cjk_font, const lv_font_t* latin_font) {
    if (cjk_font) {
        cjk_font->fallback = latin_font;
    }
}

CjkFontSet load_cjk_font_set(app::AssetManager& assets, const char* file_name) {
    CjkFontSet set;
    set.size_12 = assets.load_font(file_name, 12);
    set.size_18 = assets.load_font(file_name, 18);
    wire_fallback(set.size_12, &lv_font_montserrat_12);
    wire_fallback(set.size_18, &lv_font_montserrat_18);
    return set;
}

const lv_font_t* pick_from_set(const CjkFontSet& set, std::int32_t size) {
    switch (size) {
        case 18:
            return set.size_18 ? set.size_18 : &lv_font_montserrat_18;
        case 12:
        default:
            return set.size_12 ? set.size_12 : &lv_font_montserrat_12;
    }
}

}  // namespace

void init_fonts(app::AssetManager& assets) {
    if (g_initialized) {
        return;
    }
    g_japanese = load_cjk_font_set(assets, "NotoSansJP-Subset.otf");
    g_chinese = load_cjk_font_set(assets, "NotoSansSC-Subset.otf");
    g_initialized = true;
}

const lv_font_t* font_for_size(std::int32_t size) { return font_for_language_and_size(current_language(), size); }

const lv_font_t* font_for_language_and_size(Lang lang, std::int32_t size) {
    switch (lang) {
        case Lang::JA:
            return pick_from_set(g_japanese, size);
        case Lang::ZH:
            return pick_from_set(g_chinese, size);
        case Lang::EN:
        default:
            return montserrat_for_size(size);
    }
}

}  // namespace i18n
