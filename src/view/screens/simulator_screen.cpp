/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */

#include "simulator_screen.h"

#include "asset_manager.h"
#include "bindings.h"
#include "pixel_grid.h"
#include "theme.h"

#include <cstdio>

namespace screen {
namespace {

constexpr int kCellSize = 14;

struct SimulatorUi {
    viewmodel::BaseViewModel* view_model = nullptr;
    view::widgets::PixelGrid grid;
    lv_obj_t* title_label = nullptr;
    lv_timer_t* timer = nullptr;
    int preview_index = 0;
    bool mono = false;
};

void render(SimulatorUi* ui) {
    if (!ui || !ui->view_model) {
        return;
    }
    auto& vm = *ui->view_model;
    const auto& animation = vm.animation();
    if (ui->preview_index >= animation.frame_count()) {
        ui->preview_index = 0;
    }
    const auto palette = view::palette(vm.is_dark_mode());
    view::widgets::refresh_pixel_grid(ui->grid, animation.frame(ui->preview_index), palette, -1, -1, ui->mono);

    char buf[48];
    std::snprintf(buf, sizeof(buf), "Simulator   Frame %d/%d  [%s]", ui->preview_index + 1, animation.frame_count(),
                  ui->mono ? "Mono" : "RGB");
    lv_label_set_text(ui->title_label, buf);
}

void timer_cb(lv_timer_t* timer) {
    auto* ui = static_cast<SimulatorUi*>(lv_timer_get_user_data(timer));
    if (!ui || !ui->view_model) {
        return;
    }
    const auto& animation = ui->view_model->animation();
    ui->preview_index = (ui->preview_index + 1) % animation.frame_count();
    render(ui);
    lv_timer_set_period(timer, static_cast<uint32_t>(animation.frame_duration_ms(ui->preview_index)));
}

void key_cb(lv_event_t* event) {
    auto* ui = static_cast<SimulatorUi*>(lv_event_get_user_data(event));
    if (!ui) {
        return;
    }
    const uint32_t key = lv_event_get_key(event);
    if (key == 'm' || key == 'M') {
        ui->mono = !ui->mono;
        render(ui);
    }
}

void delete_cb(lv_event_t* event) {
    auto* ui = static_cast<SimulatorUi*>(lv_event_get_user_data(event));
    if (ui) {
        if (ui->timer) {
            lv_timer_delete(ui->timer);
        }
        delete ui;
    }
}

} // namespace

SimulatorScreen::SimulatorScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

void SimulatorScreen::build_content(lv_obj_t* content) {
    auto* ui = new SimulatorUi();
    ui->view_model = &view_model();
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    auto* small_font = assets().load_font("inter-regular.ttf", 12);

    auto* title = lv_label_create(content);
    lv_obj_set_style_text_font(title, small_font ? small_font : &lv_font_montserrat_12, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 4, 0);
    reactive::bind_theme(title, view_model().dark_mode_subject(), reactive::ThemeRole::Text);
    ui->title_label = title;

    ui->grid = view::widgets::build_pixel_grid(content, kCellSize, 1);
    lv_obj_align(ui->grid.container, LV_ALIGN_CENTER, 0, 2);
    lv_obj_add_event_cb(ui->grid.container, key_cb, LV_EVENT_KEY, ui);
    lv_obj_add_event_cb(ui->grid.container, delete_cb, LV_EVENT_DELETE, ui);
    lv_group_add_obj(BaseScreen::shared_group(), ui->grid.container);

    auto* guide = lv_label_create(content);
    lv_label_set_text(guide, "M: Mono/RGB view    Esc: Back");
    lv_obj_set_style_text_font(guide, small_font ? small_font : &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_opa(guide, LV_OPA_70, 0);
    lv_obj_align(guide, LV_ALIGN_BOTTOM_MID, 0, -4);
    reactive::bind_theme(guide, view_model().dark_mode_subject(), reactive::ThemeRole::Text);

    render(ui);
    const auto& animation = view_model().animation();
    ui->timer = lv_timer_create(timer_cb, static_cast<uint32_t>(animation.frame_duration_ms(0)), ui);

    lv_group_focus_obj(ui->grid.container);
}

} // namespace screen
