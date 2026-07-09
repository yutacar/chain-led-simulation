#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2026 yutacar
SPDX-License-Identifier: MIT

Generates assets/images/title_logo.png: a 3D isometric-block ASCII-art
rendering of "CHAINLED" for the Title screen (src/view/screens/
title_screen.cpp), adapted from Battleship's tools/generate_title_logo.py
(same rendering algorithm and font style, see
[[battleship-cardputerzero-project]]).

Why "CHAINLED" and not the full "Chain LED Simulation" display name: the
brand wordmark, kept in English in every UI language -- same treatment
Battleship gives "BATTLESHIP" (the product's own name is not translated,
even though everything else is; see PLAN.md/NOTES.md Phase 6 i18n notes).
It is also the project's own name everywhere else in the codebase (CMake
project(chainled), the BLE advertised name prefix "CHAINLED-", the repo
name) so it reads as the consistent brand token instead of introducing a
second name just for the logo.

Design
------
Each letter is authored as a 5x7 dot-matrix glyph (FONT_5X7 below, classic
LED-sign style bitmap font). Battleship's table already covers A/E/H/I/L;
this file adds the 3 additional letters CHAINLED needs (C/N/D) in the same
classic-5x7 style. Every "on" dot is drawn as a small cube with 3 faces
(front, top, right-side) using isometric-style parallelogram bevels;
adjacent "on" dots fuse into solid blocks because the top/side bevels are
only drawn on the *silhouette edge* of the glyph, not on every individual
cell -- see render() for the exact algorithm (unchanged from Battleship's).

Why pre-rendered PNG instead of an LVGL label: LVGL's built-in fonts
(Montserrat) are proportional and have no built-in 3D/bevel rendering, so
drawing this directly with lv_label would require a custom bitmap font at
minimum, and still couldn't do the per-glyph beveled shading. Pre-rendering
with Pillow and displaying via lv_image (LV_USE_LIBPNG=1, LV_USE_FS_STDIO
drive letter 'A' -- both already enabled in lv_conf_desktop.h /
lv_conf_cm0.h) is the simplest correct approach.

Color choice: the face color (99, 226, 183) = 0x63e2b7 matches
view::color::dark::kPrimary (src/view/ui_const.h) exactly -- ChainLED
inherited the same template default accent color Battleship used, so no
new palette was invented; the logo reads as an intentional accent on the
dark theme, with a heavy near-black outline keeping it legible on the
light theme's 0xffffff body too.

Requirements (installed on demand into a local venv, not vendored):
    pip install pillow

Usage:
    python3 tools/generate_title_logo.py
"""

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw

REPO_ROOT = Path(__file__).resolve().parent.parent
OUTPUT_PATH = REPO_ROOT / "assets" / "images" / "title_logo.png"

TEXT = "CHAINLED"
GLYPH_COLS, GLYPH_ROWS = 5, 7

# Hand-authored 5x7 dot-matrix bitmap font (rows top->bottom, '1' = lit
# cell), classic LED-sign style. A/E/H/I/L reused verbatim from
# Battleship's table (same style); C/N/D added here for CHAINLED.
FONT_5X7 = {
    "A": ["01110", "10001", "10001", "11111", "10001", "10001", "10001"],
    "C": ["01111", "10000", "10000", "10000", "10000", "10000", "01111"],
    "D": ["11110", "10001", "10001", "10001", "10001", "10001", "11110"],
    "E": ["11111", "10000", "10000", "11110", "10000", "10000", "11111"],
    "H": ["10001", "10001", "10001", "11111", "10001", "10001", "10001"],
    "I": ["11111", "00100", "00100", "00100", "00100", "00100", "11111"],
    "L": ["10000", "10000", "10000", "10000", "10000", "10000", "11111"],
    "N": ["10001", "11001", "10101", "10101", "10011", "10001", "10001"],
}

# Max PNG dimensions the Title screen layout budgets for the logo (320x170
# screen; see title_screen.cpp). Title's button stack is short (at most 4:
# New/Load/Send-Receive/Settings -- Quit was dropped in favor of Esc-hold/
# Home), so there's more headroom than Battleship's own 48px budget assumed
# for its own button stack; this only needs to leave room for the
# "Simulation" subtitle plus that button stack.
MAX_W, MAX_H = 300, 30

# Matches view::color::dark::kPrimary (0x63e2b7) -- see module docstring.
PALETTE = dict(
    face=(99, 226, 183, 255),
    top=(200, 250, 235, 255),
    side=(20, 80, 65, 255),
    outline=(6, 20, 16, 255),
)


def _on(grid: list[str], r: int, c: int) -> bool:
    if 0 <= r < GLYPH_ROWS and 0 <= c < GLYPH_COLS:
        return grid[r][c] == "1"
    return False


def render(
    face: tuple[int, int, int, int],
    top: tuple[int, int, int, int],
    side: tuple[int, int, int, int],
    outline: tuple[int, int, int, int],
    cell: int = 6,
    depth: int = 3,
    letter_gap: int = 1,
    supersample: int = 4,
) -> Image.Image:
    ss = supersample
    cell_px = cell * ss
    depth_px = depth * ss

    letter_col_offsets = []
    total_cols = 0
    for _ in TEXT:
        letter_col_offsets.append(total_cols)
        total_cols += GLYPH_COLS + letter_gap

    width = total_cols * cell_px + depth_px + 4 * ss
    height = GLYPH_ROWS * cell_px + depth_px + 4 * ss
    img = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    origin_x, origin_y = depth_px + 2 * ss, depth_px + 2 * ss

    def cell_origin(base_col: int, r: int, c: int) -> tuple[int, int]:
        return origin_x + (base_col + c) * cell_px, origin_y + r * cell_px

    # Pass 1: heavy outline -- redraw every face/bevel offset in a ring of
    # directions so a crisp dark border surrounds the whole glyph silhouette.
    for li, ch in enumerate(TEXT):
        grid = FONT_5X7[ch]
        base_col = letter_col_offsets[li]
        for r in range(GLYPH_ROWS):
            for c in range(GLYPH_COLS):
                if grid[r][c] != "1":
                    continue
                x0, y0 = cell_origin(base_col, r, c)
                for dx in (-ss, 0, ss):
                    for dy in (-ss, 0, ss):
                        draw.rectangle(
                            [x0 + dx, y0 + dy, x0 + cell_px - 1 + dx, y0 + cell_px - 1 + dy],
                            fill=outline,
                        )
                if not _on(grid, r - 1, c):
                    for dx in (-ss, 0, ss):
                        for dy in (-ss, 0, ss):
                            draw.polygon(
                                [
                                    (x0 + dx, y0 + dy),
                                    (x0 + depth_px + dx, y0 - depth_px + dy),
                                    (x0 + cell_px - 1 + depth_px + dx, y0 - depth_px + dy),
                                    (x0 + cell_px - 1 + dx, y0 + dy),
                                ],
                                fill=outline,
                            )
                if not _on(grid, r, c + 1):
                    for dx in (-ss, 0, ss):
                        for dy in (-ss, 0, ss):
                            draw.polygon(
                                [
                                    (x0 + cell_px - 1 + dx, y0 + dy),
                                    (x0 + cell_px - 1 + depth_px + dx, y0 - depth_px + dy),
                                    (x0 + cell_px - 1 + depth_px + dx, y0 + cell_px - 1 - depth_px + dy),
                                    (x0 + cell_px - 1 + dx, y0 + cell_px - 1 + dy),
                                ],
                                fill=outline,
                            )

    # Pass 2: front faces (solid fill).
    for li, ch in enumerate(TEXT):
        grid = FONT_5X7[ch]
        base_col = letter_col_offsets[li]
        for r in range(GLYPH_ROWS):
            for c in range(GLYPH_COLS):
                if grid[r][c] == "1":
                    x0, y0 = cell_origin(base_col, r, c)
                    draw.rectangle([x0, y0, x0 + cell_px - 1, y0 + cell_px - 1], fill=face)

    # Pass 3: top bevels, only on the silhouette's top edge (no 'on'
    # neighbor directly above) so adjoining cells fuse into solid blocks.
    for li, ch in enumerate(TEXT):
        grid = FONT_5X7[ch]
        base_col = letter_col_offsets[li]
        for r in range(GLYPH_ROWS):
            for c in range(GLYPH_COLS):
                if grid[r][c] == "1" and not _on(grid, r - 1, c):
                    x0, y0 = cell_origin(base_col, r, c)
                    draw.polygon(
                        [
                            (x0, y0),
                            (x0 + depth_px, y0 - depth_px),
                            (x0 + cell_px - 1 + depth_px, y0 - depth_px),
                            (x0 + cell_px - 1, y0),
                        ],
                        fill=top,
                    )

    # Pass 4: right-side bevels, only on the silhouette's right edge.
    for li, ch in enumerate(TEXT):
        grid = FONT_5X7[ch]
        base_col = letter_col_offsets[li]
        for r in range(GLYPH_ROWS):
            for c in range(GLYPH_COLS):
                if grid[r][c] == "1" and not _on(grid, r, c + 1):
                    x0, y0 = cell_origin(base_col, r, c)
                    draw.polygon(
                        [
                            (x0 + cell_px - 1, y0),
                            (x0 + cell_px - 1 + depth_px, y0 - depth_px),
                            (x0 + cell_px - 1 + depth_px, y0 + cell_px - 1 - depth_px),
                            (x0 + cell_px - 1, y0 + cell_px - 1),
                        ],
                        fill=side,
                    )

    # Downsample (supersampling anti-alias) then fit within the MAX_W x
    # MAX_H budget the Title screen layout allows.
    img = img.resize((width // ss, height // ss), Image.LANCZOS)
    scale = min(MAX_W / img.width, MAX_H / img.height, 1.0)
    if scale < 1.0:
        img = img.resize((max(1, int(img.width * scale)), max(1, int(img.height * scale))), Image.LANCZOS)
    return img


def main() -> None:
    img = render(**PALETTE)
    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    img.save(OUTPUT_PATH)
    print(f"{OUTPUT_PATH} ({img.width}x{img.height})")


if __name__ == "__main__":
    main()
