/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */

#include "settings_screen.h"

#include "asset_manager.h"
#include "bindings.h"
#include "theme.h"

#include "fonts.h"
#include "strings.h"

namespace screen {
namespace {

void select_en_cb(lv_event_t* event) {
    auto* view_model = static_cast<viewmodel::BaseViewModel*>(lv_event_get_user_data(event));
    if (view_model) {
        view_model->set_language(i18n::Lang::EN);
    }
}

void select_ja_cb(lv_event_t* event) {
    auto* view_model = static_cast<viewmodel::BaseViewModel*>(lv_event_get_user_data(event));
    if (view_model) {
        view_model->set_language(i18n::Lang::JA);
    }
}

void select_zh_cb(lv_event_t* event) {
    auto* view_model = static_cast<viewmodel::BaseViewModel*>(lv_event_get_user_data(event));
    if (view_model) {
        view_model->set_language(i18n::Lang::ZH);
    }
}

void back_cb(lv_event_t* event) {
    auto* view_model = static_cast<viewmodel::BaseViewModel*>(lv_event_get_user_data(event));
    if (view_model) {
        view_model->show_page(model::AppPage::Title);
    }
}

lv_obj_t* make_language_button(lv_obj_t* parent, viewmodel::BaseViewModel& view_model, i18n::StrId label_id,
                               i18n::Lang lang, lv_event_cb_t click_cb, bool is_current) {
    auto* button = lv_button_create(parent);
    lv_obj_set_size(button, 160, 20);
    reactive::bind_theme(button, view_model.dark_mode_subject(), reactive::ThemeRole::Button);
    if (is_current) {
        const auto palette = view::palette(view_model.is_dark_mode());
        lv_obj_set_style_border_width(button, 2, 0);
        lv_obj_set_style_border_color(button, palette.primary, 0);
        lv_obj_set_style_border_opa(button, LV_OPA_COVER, 0);
    }
    lv_obj_add_event_cb(button, click_cb, LV_EVENT_CLICKED, &view_model);

    auto* label = lv_label_create(button);
    lv_label_set_text(label, i18n::tr(label_id));
    lv_obj_set_style_text_font(label, i18n::font_for_language_and_size(lang, 12), 0);
    lv_obj_center(label);

    return button;
}

} // namespace

SettingsScreen::SettingsScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

void SettingsScreen::build_content(lv_obj_t* content) {
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    auto* title = lv_label_create(content);
    lv_label_set_text(title, i18n::tr(i18n::StrId::SettingsTitle));
    lv_obj_set_style_text_font(title, i18n::font_for_size(18), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);
    reactive::bind_theme(title, view_model().dark_mode_subject(), reactive::ThemeRole::Text);

    auto* section_label = lv_label_create(content);
    lv_label_set_text(section_label, i18n::tr(i18n::StrId::SettingsLanguageLabel));
    lv_obj_set_style_text_font(section_label, i18n::font_for_size(12), 0);
    lv_obj_align(section_label, LV_ALIGN_TOP_MID, 0, 36);
    reactive::bind_theme(section_label, view_model().dark_mode_subject(), reactive::ThemeRole::Text);

    const auto current = i18n::current_language();

    auto* menu = lv_obj_create(content);
    lv_obj_remove_style_all(menu);
    lv_obj_set_size(menu, 160, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(menu, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(menu, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(menu, 3, 0);
    lv_obj_clear_flag(menu, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(menu, LV_ALIGN_TOP_MID, 0, 52);

    auto* en_button = make_language_button(menu, view_model(), i18n::StrId::SettingsLanguageEnglish, i18n::Lang::EN,
                                           select_en_cb, current == i18n::Lang::EN);
    BaseScreen::enable_arrow_focus_navigation(en_button);
    lv_group_add_obj(BaseScreen::shared_group(), en_button);

    auto* ja_button = make_language_button(menu, view_model(), i18n::StrId::SettingsLanguageJapanese, i18n::Lang::JA,
                                           select_ja_cb, current == i18n::Lang::JA);
    BaseScreen::enable_arrow_focus_navigation(ja_button);
    lv_group_add_obj(BaseScreen::shared_group(), ja_button);

    auto* zh_button = make_language_button(menu, view_model(), i18n::StrId::SettingsLanguageChinese, i18n::Lang::ZH,
                                           select_zh_cb, current == i18n::Lang::ZH);
    BaseScreen::enable_arrow_focus_navigation(zh_button);
    lv_group_add_obj(BaseScreen::shared_group(), zh_button);

    auto* back_button = lv_button_create(menu);
    lv_obj_set_size(back_button, 160, 20);
    reactive::bind_theme(back_button, view_model().dark_mode_subject(), reactive::ThemeRole::Button);
    lv_obj_add_event_cb(back_button, back_cb, LV_EVENT_CLICKED, &view_model());
    BaseScreen::enable_arrow_focus_navigation(back_button);
    lv_group_add_obj(BaseScreen::shared_group(), back_button);
    auto* back_label = lv_label_create(back_button);
    lv_label_set_text(back_label, i18n::tr(i18n::StrId::SettingsBack));
    lv_obj_set_style_text_font(back_label, i18n::font_for_size(12), 0);
    lv_obj_center(back_label);

    auto* guide = lv_label_create(content);
    lv_label_set_text(guide, i18n::tr(i18n::StrId::SettingsGuide));
    lv_obj_set_style_text_font(guide, i18n::font_for_size(12), 0);
    lv_obj_set_style_text_opa(guide, LV_OPA_70, 0);
    lv_obj_align(guide, LV_ALIGN_BOTTOM_MID, 0, -4);
    reactive::bind_theme(guide, view_model().dark_mode_subject(), reactive::ThemeRole::Text);

    lv_group_focus_obj(en_button);
}

} // namespace screen
