/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */

#include "pixel_grid.h"

namespace view::widgets {
namespace {

/// The "unlit LED" color for an off pixel -- distinct from any color in the
/// draw palette so an off cell never reads as "drawn black".
constexpr lv_color_t kUnlitColor = LV_COLOR_MAKE(0x20, 0x20, 0x20);
/// Mono-preview "lit" color (Chain Mono has no color channel of its own).
constexpr lv_color_t kMonoLitColor = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF);

}  // namespace

lv_color_t rgb565_to_lv_color(chainled::core::Rgb565 color) {
    const uint8_t r = static_cast<uint8_t>((color >> 11) & 0x1F);
    const uint8_t g = static_cast<uint8_t>((color >> 5) & 0x3F);
    const uint8_t b = static_cast<uint8_t>(color & 0x1F);
    return LV_COLOR_MAKE(static_cast<uint8_t>((r * 255) / 31), static_cast<uint8_t>((g * 255) / 63),
                          static_cast<uint8_t>((b * 255) / 31));
}

PixelGrid build_pixel_grid(lv_obj_t* parent, int cell_size, int cell_gap) {
    PixelGrid grid;

    auto* container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    const int pixels = cell_size * chainled::core::kFrameSize;
    lv_obj_set_size(container, pixels, pixels);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
    grid.container = container;

    for (int y = 0; y < chainled::core::kFrameSize; ++y) {
        for (int x = 0; x < chainled::core::kFrameSize; ++x) {
            auto* cell = lv_obj_create(container);
            lv_obj_remove_style_all(cell);
            lv_obj_set_size(cell, cell_size - cell_gap, cell_size - cell_gap);
            lv_obj_set_pos(cell, x * cell_size, y * cell_size);
            lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_clear_flag(cell, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(cell, 1, 0);
            lv_obj_set_style_radius(cell, 0, 0);
            grid.cells[y][x] = cell;
        }
    }

    return grid;
}

void refresh_pixel_grid(const PixelGrid& grid, const chainled::core::Frame& frame, const view::ThemePalette& palette,
                         int cursor_x, int cursor_y, bool mono, bool draw_cell_borders) {
    for (int y = 0; y < chainled::core::kFrameSize; ++y) {
        for (int x = 0; x < chainled::core::kFrameSize; ++x) {
            lv_obj_t* cell = grid.cells[y][x];
            if (!cell) {
                continue;
            }

            const auto pixel = frame.pixel(x, y);
            lv_color_t color = kUnlitColor;
            if (pixel != chainled::core::kOffColor) {
                color = mono ? kMonoLitColor : rgb565_to_lv_color(pixel);
            }
            lv_obj_set_style_bg_color(cell, color, 0);

            if (!draw_cell_borders) {
                lv_obj_set_style_border_width(cell, 0, 0);
                continue;
            }

            const bool is_cursor = (x == cursor_x && y == cursor_y);
            lv_obj_set_style_border_color(cell, is_cursor ? palette.text : palette.border, 0);
            lv_obj_set_style_border_width(cell, is_cursor ? 2 : 1, 0);
        }
    }
}

}  // namespace view::widgets
