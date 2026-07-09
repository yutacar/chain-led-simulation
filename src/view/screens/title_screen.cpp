/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */

#include "title_screen.h"

#include "asset_manager.h"
#include "bindings.h"
#include "theme.h"

#include "fonts.h"
#include "strings.h"

#include <string>

namespace screen {
namespace {

/// Owns the Load/Delete buttons so delete_animation_cb can hide both in
/// place after removing the save file, without a full screen rebuild (the
/// generic ScreenManager::reload_current_screen() path is driven by the
/// language-change subject and re-navigating to the already-current Title
/// page is a no-op -- lv_subject only notifies on an actual value change).
struct TitleUi {
    viewmodel::BaseViewModel* view_model = nullptr;
    lv_obj_t* load_button = nullptr;
    lv_obj_t* delete_button = nullptr;
};

void title_ui_delete_cb(lv_event_t* event) {
    delete static_cast<TitleUi*>(lv_event_get_user_data(event));
}

void new_animation_cb(lv_event_t* event) {
    auto* view_model = static_cast<viewmodel::BaseViewModel*>(lv_event_get_user_data(event));
    if (view_model) {
        view_model->new_animation();
    }
}

void load_animation_cb(lv_event_t* event) {
    auto* ui = static_cast<TitleUi*>(lv_event_get_user_data(event));
    if (ui && ui->view_model) {
        ui->view_model->load_animation();
    }
}

void delete_animation_cb(lv_event_t* event) {
    auto* ui = static_cast<TitleUi*>(lv_event_get_user_data(event));
    if (!ui || !ui->view_model || !ui->view_model->delete_saved_animation()) {
        return;
    }
    // Hide both buttons in place -- has_saved_animation() is now false, and
    // an already-focused/focusable hidden object would otherwise trap
    // keyboard navigation.
    if (ui->load_button) {
        lv_group_remove_obj(ui->load_button);
        lv_obj_add_flag(ui->load_button, LV_OBJ_FLAG_HIDDEN);
    }
    if (ui->delete_button) {
        lv_group_remove_obj(ui->delete_button);
        lv_obj_add_flag(ui->delete_button, LV_OBJ_FLAG_HIDDEN);
    }
    lv_group_focus_next(BaseScreen::shared_group());
}

void send_receive_cb(lv_event_t* event) {
    auto* view_model = static_cast<viewmodel::BaseViewModel*>(lv_event_get_user_data(event));
    if (view_model) {
        view_model->show_page(model::AppPage::SendReceive);
    }
}

void settings_cb(lv_event_t* event) {
    auto* view_model = static_cast<viewmodel::BaseViewModel*>(lv_event_get_user_data(event));
    if (view_model) {
        view_model->show_page(model::AppPage::Settings);
    }
}

lv_obj_t* make_menu_button(lv_obj_t* parent, const char* text) {
    auto* button = lv_button_create(parent);
    lv_obj_set_size(button, 170, 16);
    auto* label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, i18n::font_for_size(12), 0);
    lv_obj_center(label);
    return button;
}

} // namespace

TitleScreen::TitleScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

void TitleScreen::build_content(lv_obj_t* content) {
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    auto* ui = new TitleUi();
    ui->view_model = &view_model();
    lv_obj_add_event_cb(content, title_ui_delete_cb, LV_EVENT_DELETE, ui);

    // Logo + subtitle + button stack are nested inside one flex-column
    // "group" so the whole block centers as a unit (both axes) regardless
    // of how many buttons are showing -- a fixed screen-relative offset
    // for each piece was fragile (see NOTES.md: it overlapped twice before
    // this nested-flex rewrite). Only the bottom guide stays outside the
    // group, pinned to the screen edge.
    auto* group = lv_obj_create(content);
    lv_obj_remove_style_all(group);
    lv_obj_set_size(group, 200, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(group, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(group, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(group, 4, 0);
    lv_obj_clear_flag(group, LV_OBJ_FLAG_SCROLLABLE);
    // Nudged up further when Load/Delete are both showing (5 buttons: New/
    // Load/Delete/Send-Receive/Settings) -- true screen-center would clip
    // into the bottom guide at that worst-case height (see NOTES.md this
    // was found via screenshot). Kept at a gentler nudge otherwise so the
    // common 3-button case still reads as centered rather than shifted up.
    const int group_offset = view_model().has_saved_animation() ? -18 : -6;
    lv_obj_align(group, LV_ALIGN_CENTER, 0, group_offset);

    // "CHAINLED" is the product's brand wordmark, kept in English in every
    // UI language (same treatment as Battleship's "BATTLESHIP" title logo --
    // see tools/generate_title_logo.py). Rendered as a pre-generated 3D
    // ASCII-art PNG rather than an lv_label: LVGL's built-in fonts have no
    // bevel/shading support, so the isometric-block look is produced
    // offline with Pillow.
    auto* title = lv_image_create(group);
    auto logo_path = assets().resolve("images/title_logo.png");
    if (!logo_path.empty()) {
        // lv_image_set_src() lv_strdup()s file-path sources internally, so
        // this local string does not need to outlive the call.
        std::string logo_path_str = "A:" + logo_path.string();
        lv_image_set_src(title, logo_path_str.c_str());
    } else {
        // Fallback in case the PNG asset failed to resolve -- keep the
        // title readable instead of showing a blank/broken image.
        lv_obj_delete(title);
        title = lv_label_create(group);
        lv_label_set_text(title, "CHAINLED");
        lv_obj_set_style_text_font(title, i18n::font_for_size(18), 0);
        reactive::bind_theme(title, view_model().dark_mode_subject(), reactive::ThemeRole::Text);
    }

    // "Simulation" completes the "CHAINLED Simulation" brand lockup below
    // the logo -- fixed English literal, same non-translated treatment as
    // the logo itself (not part of the i18n catalog).
    auto* subtitle = lv_label_create(group);
    lv_label_set_text(subtitle, "Simulation");
    lv_obj_set_style_text_font(subtitle, i18n::font_for_size(12), 0);
    lv_obj_set_style_text_opa(subtitle, LV_OPA_70, 0);
    reactive::bind_theme(subtitle, view_model().dark_mode_subject(), reactive::ThemeRole::Text);

    auto* menu = lv_obj_create(group);
    lv_obj_remove_style_all(menu);
    lv_obj_set_size(menu, 170, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(menu, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(menu, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(menu, 2, 0);
    lv_obj_set_style_pad_top(menu, 3, 0);
    lv_obj_clear_flag(menu, LV_OBJ_FLAG_SCROLLABLE);

    auto* new_button = make_menu_button(menu, i18n::tr(i18n::StrId::TitleNewAnimation));
    reactive::bind_theme(new_button, view_model().dark_mode_subject(), reactive::ThemeRole::Button);
    lv_obj_add_event_cb(new_button, new_animation_cb, LV_EVENT_CLICKED, &view_model());
    BaseScreen::enable_arrow_focus_navigation(new_button);
    lv_group_add_obj(BaseScreen::shared_group(), new_button);

    lv_obj_t* default_focus = new_button;

    if (view_model().has_saved_animation()) {
        auto* load_button = make_menu_button(menu, i18n::tr(i18n::StrId::TitleLoadAnimation));
        reactive::bind_theme(load_button, view_model().dark_mode_subject(), reactive::ThemeRole::Button);
        lv_obj_add_event_cb(load_button, load_animation_cb, LV_EVENT_CLICKED, ui);
        BaseScreen::enable_arrow_focus_navigation(load_button);
        lv_group_add_obj(BaseScreen::shared_group(), load_button);
        ui->load_button = load_button;

        auto* delete_button = make_menu_button(menu, i18n::tr(i18n::StrId::TitleDeleteAnimation));
        reactive::bind_theme(delete_button, view_model().dark_mode_subject(), reactive::ThemeRole::Button);
        lv_obj_add_event_cb(delete_button, delete_animation_cb, LV_EVENT_CLICKED, ui);
        BaseScreen::enable_arrow_focus_navigation(delete_button);
        lv_group_add_obj(BaseScreen::shared_group(), delete_button);
        ui->delete_button = delete_button;
    }

    auto* send_receive_button = make_menu_button(menu, i18n::tr(i18n::StrId::TitleSendReceive));
    reactive::bind_theme(send_receive_button, view_model().dark_mode_subject(), reactive::ThemeRole::Button);
    lv_obj_add_event_cb(send_receive_button, send_receive_cb, LV_EVENT_CLICKED, &view_model());
    BaseScreen::enable_arrow_focus_navigation(send_receive_button);
    lv_group_add_obj(BaseScreen::shared_group(), send_receive_button);

    auto* settings_button = make_menu_button(menu, i18n::tr(i18n::StrId::TitleSettings));
    reactive::bind_theme(settings_button, view_model().dark_mode_subject(), reactive::ThemeRole::Button);
    lv_obj_add_event_cb(settings_button, settings_cb, LV_EVENT_CLICKED, &view_model());
    BaseScreen::enable_arrow_focus_navigation(settings_button);
    lv_group_add_obj(BaseScreen::shared_group(), settings_button);

    // No Quit button -- Esc (hold) / Home already quits (see guide below),
    // and dropping it keeps this screen's button stack short (user request).
    auto* guide = lv_label_create(content);
    lv_label_set_text(guide, i18n::tr(i18n::StrId::TitleGuide));
    lv_obj_set_style_text_font(guide, i18n::font_for_size(12), 0);
    lv_obj_set_style_text_opa(guide, LV_OPA_70, 0);
    lv_obj_align(guide, LV_ALIGN_BOTTOM_MID, 0, -4);
    reactive::bind_theme(guide, view_model().dark_mode_subject(), reactive::ThemeRole::Text);

    lv_group_focus_obj(default_focus);
}

} // namespace screen
