/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */

#include "send_receive_screen.h"

#include "asset_manager.h"
#include "bindings.h"
#include "theme.h"

#include "fonts.h"
#include "strings.h"

namespace screen {
namespace {

void send_cb(lv_event_t* event) {
    auto* view_model = static_cast<viewmodel::BaseViewModel*>(lv_event_get_user_data(event));
    if (view_model) {
        view_model->show_page(model::AppPage::Send);
    }
}

void receive_cb(lv_event_t* event) {
    auto* view_model = static_cast<viewmodel::BaseViewModel*>(lv_event_get_user_data(event));
    if (view_model) {
        view_model->show_page(model::AppPage::Receive);
    }
}

void back_cb(lv_event_t* event) {
    auto* view_model = static_cast<viewmodel::BaseViewModel*>(lv_event_get_user_data(event));
    if (view_model) {
        view_model->show_page(model::AppPage::Title);
    }
}

lv_obj_t* make_menu_button(lv_obj_t* parent, const char* text) {
    auto* button = lv_button_create(parent);
    lv_obj_set_size(button, 160, 20);
    auto* label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, i18n::font_for_size(12), 0);
    lv_obj_center(label);
    return button;
}

} // namespace

SendReceiveScreen::SendReceiveScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

void SendReceiveScreen::build_content(lv_obj_t* content) {
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    auto* title = lv_label_create(content);
    lv_label_set_text(title, i18n::tr(i18n::StrId::SendReceiveTitle));
    lv_obj_set_style_text_font(title, i18n::font_for_size(18), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    reactive::bind_theme(title, view_model().dark_mode_subject(), reactive::ThemeRole::Text);

    auto* menu = lv_obj_create(content);
    lv_obj_remove_style_all(menu);
    lv_obj_set_size(menu, 160, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(menu, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(menu, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(menu, 4, 0);
    lv_obj_clear_flag(menu, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(menu, LV_ALIGN_CENTER, 0, 10);

#if CHAINLED_BLE
    auto* send_button = make_menu_button(menu, i18n::tr(i18n::StrId::TitleSendAnimationBle));
#else
    auto* send_button = make_menu_button(menu, i18n::tr(i18n::StrId::TitleSendAnimation));
#endif
    reactive::bind_theme(send_button, view_model().dark_mode_subject(), reactive::ThemeRole::Button);
    lv_obj_add_event_cb(send_button, send_cb, LV_EVENT_CLICKED, &view_model());
    BaseScreen::enable_arrow_focus_navigation(send_button);
    lv_group_add_obj(BaseScreen::shared_group(), send_button);

#if CHAINLED_BLE
    auto* receive_button = make_menu_button(menu, i18n::tr(i18n::StrId::TitleReceiveAnimationBle));
#else
    auto* receive_button = make_menu_button(menu, i18n::tr(i18n::StrId::TitleReceiveAnimation));
#endif
    reactive::bind_theme(receive_button, view_model().dark_mode_subject(), reactive::ThemeRole::Button);
    lv_obj_add_event_cb(receive_button, receive_cb, LV_EVENT_CLICKED, &view_model());
    BaseScreen::enable_arrow_focus_navigation(receive_button);
    lv_group_add_obj(BaseScreen::shared_group(), receive_button);

    auto* back_button = make_menu_button(menu, i18n::tr(i18n::StrId::SendReceiveBack));
    reactive::bind_theme(back_button, view_model().dark_mode_subject(), reactive::ThemeRole::Button);
    lv_obj_add_event_cb(back_button, back_cb, LV_EVENT_CLICKED, &view_model());
    BaseScreen::enable_arrow_focus_navigation(back_button);
    lv_group_add_obj(BaseScreen::shared_group(), back_button);

    auto* guide = lv_label_create(content);
    lv_label_set_text(guide, i18n::tr(i18n::StrId::SendReceiveGuide));
    lv_obj_set_style_text_font(guide, i18n::font_for_size(12), 0);
    lv_obj_set_style_text_opa(guide, LV_OPA_70, 0);
    lv_obj_align(guide, LV_ALIGN_BOTTOM_MID, 0, -4);
    reactive::bind_theme(guide, view_model().dark_mode_subject(), reactive::ThemeRole::Text);

    lv_group_focus_obj(send_button);
}

} // namespace screen
