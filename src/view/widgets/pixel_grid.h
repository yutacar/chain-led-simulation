/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 *
 * PixelGrid: an 8x8 grid of lv_obj_t cells rendering a chainled::core::Frame.
 * Shared between the Editor screen (with an optional cursor highlight, live
 * pixel editing) and the Simulator screen (playback preview, no cursor) --
 * PLAN.md §2.2's SimulatorChainOutput is this widget in "no cursor" mode.
 */
#pragma once

#include "theme.h"

#include "core/frame.h"

#include "lvgl.h"

namespace view::widgets {

struct PixelGrid {
    lv_obj_t* container = nullptr;
    lv_obj_t* cells[chainled::core::kFrameSize][chainled::core::kFrameSize] = {};
};

/// Creates the cell grid under `parent`. Each cell is `cell_size - cell_gap`
/// pixels square, laid out on a `cell_size` pitch (so a 1px gap is visible
/// between cells), matching the board-rendering approach used elsewhere in
/// this codebase family (see Battleship's PlacementScreen).
PixelGrid build_pixel_grid(lv_obj_t* parent, int cell_size, int cell_gap);

/// Redraws every cell from `frame`. If `cursor_x`/`cursor_y` are both >= 0,
/// that cell gets a highlighted border (Editor mode); pass -1, -1 to render
/// without a cursor (Simulator mode). `mono` renders every non-off pixel as
/// a fixed "lit" color instead of its actual RGB565 value, simulating how
/// the animation would look on a Chain Mono unit. `draw_cell_borders`
/// disables the per-cell border (leave it on the default for the main
/// editor/simulator grid; turn it off for small thumbnails, where a 1px
/// border on a 2px cell visually overwhelms the fill color).
void refresh_pixel_grid(const PixelGrid& grid, const chainled::core::Frame& frame, const view::ThemePalette& palette,
                         int cursor_x = -1, int cursor_y = -1, bool mono = false, bool draw_cell_borders = true);

/// RGB565 -> lv_color_t, shared by the grid cell renderer and any UI element
/// (e.g. the Editor's current-draw-color swatch) that needs to show a raw
/// Rgb565 value as an LVGL color.
lv_color_t rgb565_to_lv_color(chainled::core::Rgb565 color);

}  // namespace view::widgets
