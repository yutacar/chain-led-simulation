/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */

#include "base_screen.h"

#include "asset_manager.h"
#include "bindings.h"
#include "theme.h"

namespace screen {
namespace {

lv_group_t* g_shared_group = nullptr;

void arrow_focus_navigation_cb(lv_event_t* event) {
    if (!g_shared_group) {
        return;
    }
    switch (lv_event_get_key(event)) {
        case LV_KEY_RIGHT:
        case LV_KEY_DOWN:
            lv_group_focus_next(g_shared_group);
            break;
        case LV_KEY_LEFT:
        case LV_KEY_UP:
            lv_group_focus_prev(g_shared_group);
            break;
        default:
            break;
    }
}

} // namespace

void BaseScreen::set_shared_group(lv_group_t* group) {
    g_shared_group = group;
}

lv_group_t* BaseScreen::shared_group() {
    return g_shared_group;
}

void BaseScreen::enable_arrow_focus_navigation(lv_obj_t* obj) {
    lv_obj_add_event_cb(obj, arrow_focus_navigation_cb, LV_EVENT_KEY, nullptr);
}

BaseScreen::BaseScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : view_model_(view_model), assets_(assets) {}

BaseScreen::~BaseScreen() {
    if (root_ && lv_obj_is_valid(root_)) {
        lv_obj_delete(root_);
    }
}

void BaseScreen::init() {
    if (root_) {
        return;
    }

    if (g_shared_group) {
        lv_group_remove_all_objs(g_shared_group);
    }

    root_ = lv_obj_create(nullptr);
    lv_obj_remove_style_all(root_);
    lv_obj_set_size(root_, view::kScreenWidth, view::kScreenHeight);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
    reactive::bind_theme(root_, view_model_.dark_mode_subject(), reactive::ThemeRole::Screen);

    build_content(root_);
}

lv_obj_t* BaseScreen::root() const {
    return root_;
}

viewmodel::BaseViewModel& BaseScreen::view_model() {
    return view_model_;
}

app::AssetManager& BaseScreen::assets() {
    return assets_;
}

} // namespace screen
