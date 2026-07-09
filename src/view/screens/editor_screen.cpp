/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */

#include "editor_screen.h"

#include "asset_manager.h"
#include "bindings.h"
#include "pixel_grid.h"
#include "theme.h"

#include "core/animation.h"
#include "fonts.h"
#include "strings.h"

#include <cstdio>
#include <string>

namespace screen {
namespace {

constexpr int kCellSize = 12;
constexpr int kCellGap = 1;
constexpr int kGridPixels = kCellSize * chainled::core::kFrameSize;

constexpr int kThumbCellSize = 2;
constexpr int kThumbPixels = kThumbCellSize * chainled::core::kFrameSize;
constexpr int kThumbGap = 3;

struct EditorUi {
    viewmodel::BaseViewModel* view_model = nullptr;
    lv_obj_t* title_label = nullptr;
    view::widgets::PixelGrid grid;
    lv_obj_t* color_swatch = nullptr;
    lv_obj_t* status_label = nullptr;
    view::widgets::PixelGrid thumbnails[chainled::core::kMaxFrames];
    lv_observer_t* dirty_observer = nullptr;
    bool show_saved = false;
};

EditorUi* ui_for(lv_event_t* event) {
    return static_cast<EditorUi*>(lv_event_get_user_data(event));
}

void refresh_ui(EditorUi* ui) {
    if (!ui || !ui->view_model) {
        return;
    }
    auto& vm = *ui->view_model;
    const auto palette = view::palette(vm.is_dark_mode());
    const auto& animation = vm.animation();
    const int current = vm.current_frame_index();

    const std::string title = i18n::trf(i18n::StrId::EditorTitle, current + 1, animation.frame_count());
    lv_label_set_text(ui->title_label, title.c_str());

    view::widgets::refresh_pixel_grid(ui->grid, animation.frame(current), palette, vm.cursor_x(), vm.cursor_y());

    lv_obj_set_style_bg_color(ui->color_swatch, view::widgets::rgb565_to_lv_color(vm.current_draw_color()), 0);

    std::string status = i18n::trf(i18n::StrId::EditorDurationHint, animation.frame_duration_ms(current));
    if (ui->show_saved) {
        status += "   ";
        status += i18n::tr(i18n::StrId::EditorSaved);
    }
    lv_label_set_text(ui->status_label, status.c_str());

    for (int i = 0; i < chainled::core::kMaxFrames; ++i) {
        auto& thumb = ui->thumbnails[i];
        if (!thumb.container) {
            continue;
        }
        if (i < animation.frame_count()) {
            lv_obj_remove_flag(thumb.container, LV_OBJ_FLAG_HIDDEN);
            view::widgets::refresh_pixel_grid(thumb, animation.frame(i), palette, -1, -1, false,
                                               /*draw_cell_borders=*/false);
            const bool is_current = (i == current);
            lv_obj_set_style_border_width(thumb.container, is_current ? 2 : 1, 0);
            lv_obj_set_style_border_color(thumb.container, is_current ? palette.text : palette.border, 0);
            lv_obj_set_style_border_opa(thumb.container, LV_OPA_COVER, 0);
        } else {
            lv_obj_add_flag(thumb.container, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void dirty_cb(lv_observer_t* observer, lv_subject_t*) {
    auto* ui = static_cast<EditorUi*>(lv_observer_get_user_data(observer));
    refresh_ui(ui);
}

void grid_key_cb(lv_event_t* event) {
    auto* ui = ui_for(event);
    if (!ui || !ui->view_model) {
        return;
    }
    auto& vm = *ui->view_model;
    ui->show_saved = false;
    const uint32_t key = lv_event_get_key(event);

    switch (key) {
        case LV_KEY_LEFT:
            vm.move_cursor(-1, 0);
            break;
        case LV_KEY_RIGHT:
            vm.move_cursor(1, 0);
            break;
        case LV_KEY_UP:
            vm.move_cursor(0, -1);
            break;
        case LV_KEY_DOWN:
            vm.move_cursor(0, 1);
            break;
        case LV_KEY_ENTER:
            vm.toggle_pixel_at_cursor();
            break;
        case LV_KEY_BACKSPACE:
            vm.remove_current_frame();
            break;
        case 'c':
        case 'C':
            vm.cycle_draw_color();
            break;
        case 'f':
        case 'F':
            vm.flip_current_frame_horizontal();
            break;
        case 'v':
        case 'V':
            vm.flip_current_frame_vertical();
            break;
        case ',':
            vm.prev_frame();
            break;
        case '.':
            vm.next_frame();
            break;
        case 'n':
        case 'N':
            vm.add_frame();
            break;
        case 'd':
        case 'D':
            vm.duplicate_current_frame();
            break;
        case 'g':
        case 'G':
            vm.generate_remaining_frames();
            break;
        case '+':
        case '=':
            vm.adjust_current_frame_duration(50);
            break;
        case '-':
        case '_':
            vm.adjust_current_frame_duration(-50);
            break;
        case 'p':
        case 'P':
            vm.show_page(model::AppPage::Simulator);
            break;
        case 's':
        case 'S':
            vm.save_animation();
            ui->show_saved = true;
            refresh_ui(ui);
            break;
        default:
            break;
    }
}

void grid_delete_cb(lv_event_t* event) {
    auto* ui = ui_for(event);
    if (ui && ui->dirty_observer) {
        lv_observer_remove(ui->dirty_observer);
    }
    delete ui;
}

} // namespace

EditorScreen::EditorScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

void EditorScreen::build_content(lv_obj_t* content) {
    auto* ui = new EditorUi();
    ui->view_model = &view_model();
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    auto* small_font = i18n::font_for_size(12);

    // ---- Title / frame indicator ----
    auto* title = lv_label_create(content);
    lv_obj_set_style_text_font(title, small_font, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 4, 0);
    reactive::bind_theme(title, view_model().dark_mode_subject(), reactive::ThemeRole::Text);
    ui->title_label = title;

    // ---- Pixel grid (focusable, consumes arrows/Enter/shortcuts) ----
    ui->grid = view::widgets::build_pixel_grid(content, kCellSize, kCellGap);
    lv_obj_align(ui->grid.container, LV_ALIGN_TOP_LEFT, 4, 18);
    lv_obj_add_event_cb(ui->grid.container, grid_key_cb, LV_EVENT_KEY, ui);
    lv_obj_add_event_cb(ui->grid.container, grid_delete_cb, LV_EVENT_DELETE, ui);
    lv_group_add_obj(BaseScreen::shared_group(), ui->grid.container);

    // ---- Side panel: draw color swatch + key guide ----
    auto* panel = lv_obj_create(content);
    lv_obj_remove_style_all(panel);
    const int panel_width = view::kScreenWidth - (4 + kGridPixels + 8) - 4;
    lv_obj_set_size(panel, panel_width, kGridPixels);
    lv_obj_align(panel, LV_ALIGN_TOP_RIGHT, -4, 18);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(panel, 3, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    auto* swatch_row = lv_obj_create(panel);
    lv_obj_remove_style_all(swatch_row);
    lv_obj_set_size(swatch_row, LV_PCT(100), 16);
    lv_obj_set_flex_flow(swatch_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(swatch_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(swatch_row, 4, 0);
    lv_obj_clear_flag(swatch_row, LV_OBJ_FLAG_SCROLLABLE);

    auto* swatch_label = lv_label_create(swatch_row);
    lv_label_set_text(swatch_label, i18n::tr(i18n::StrId::EditorColorLabel));
    lv_obj_set_style_text_font(swatch_label, small_font, 0);
    reactive::bind_theme(swatch_label, view_model().dark_mode_subject(), reactive::ThemeRole::Text);

    auto* swatch = lv_obj_create(swatch_row);
    lv_obj_remove_style_all(swatch);
    lv_obj_set_size(swatch, 14, 14);
    lv_obj_clear_flag(swatch, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(swatch, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(swatch, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(swatch, 1, 0);
    lv_obj_set_style_border_color(swatch, view::palette(view_model().is_dark_mode()).border, 0);
    ui->color_swatch = swatch;

    const i18n::StrId guide_lines[] = {
        i18n::StrId::EditorGuideMove,
        i18n::StrId::EditorGuideFrameOps,
        i18n::StrId::EditorGuideDurationSave,
        i18n::StrId::EditorGuideGenerate,
    };
    for (const auto line_id : guide_lines) {
        auto* guide = lv_label_create(panel);
        lv_label_set_text(guide, i18n::tr(line_id));
        lv_obj_set_style_text_font(guide, small_font, 0);
        lv_obj_set_style_text_opa(guide, LV_OPA_70, 0);
        reactive::bind_theme(guide, view_model().dark_mode_subject(), reactive::ThemeRole::Text);
    }

    // ---- Timeline: up to kMaxFrames mini previews, current one highlighted ----
    for (int i = 0; i < chainled::core::kMaxFrames; ++i) {
        ui->thumbnails[i] = view::widgets::build_pixel_grid(content, kThumbCellSize, 0);
        lv_obj_set_pos(ui->thumbnails[i].container, 4 + i * (kThumbPixels + kThumbGap), 118);
        lv_obj_clear_flag(ui->thumbnails[i].container, LV_OBJ_FLAG_CLICKABLE);
    }

    // ---- Bottom status: current frame duration + save confirmation ----
    auto* status = lv_label_create(content);
    lv_obj_set_style_text_font(status, small_font, 0);
    lv_obj_align(status, LV_ALIGN_BOTTOM_LEFT, 4, -4);
    reactive::bind_theme(status, view_model().dark_mode_subject(), reactive::ThemeRole::Text);
    ui->status_label = status;

    ui->dirty_observer = reactive::observe(view_model().animation_dirty_subject(), dirty_cb, ui);

    refresh_ui(ui);
    lv_group_focus_obj(ui->grid.container);
}

} // namespace screen
