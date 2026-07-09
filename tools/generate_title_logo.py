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

Color choice: only the "LED" half of the wordmark is colored -- L=red,
E=green, D=blue, one diode color per letter -- while "CHAIN" stays the
single original accent color (99, 226, 183) = 0x63e2b7, matching
view::color::dark::kPrimary (src/view/ui_const.h). This reads as "CHAIN"
+ a literal three-color "LED" rather than the whole wordmark being an
arbitrary rainbow -- the colored half is exactly the part of the name
that means LED. A single unified dark outline (not per-color) keeps the
multi-hue glyph reading as one cohesive graphic instead of looking like
scattered confetti.

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

# "CHAIN" stays this single original accent color (see module docstring).
CHAIN_COLOR: tuple[int, int, int] = (99, 226, 183)

# One diode color per letter of "LED" (L, E, D in order) -- see module
# docstring for why per-letter rather than a whole-wordmark cycle.
LED_COLORS: list[tuple[int, int, int]] = [
    (255, 76, 76),   # L: red
    (86, 214, 110),  # E: green
    (92, 156, 255),  # D: blue
]
OUTLINE = (6, 20, 16, 255)

# Index into TEXT where "LED" starts (TEXT = "CHAINLED" -> "LED" is the
# last 3 letters).
LED_START_INDEX = len("CHAIN")


def _lighten(rgb: tuple[int, int, int], factor: float) -> tuple[int, int, int, int]:
    r, g, b = (min(255, int(c + (255 - c) * factor)) for c in rgb)
    return (r, g, b, 255)


def _darken(rgb: tuple[int, int, int], factor: float) -> tuple[int, int, int, int]:
    r, g, b = (max(0, int(c * (1 - factor))) for c in rgb)
    return (r, g, b, 255)


def _cell_colors(letter_index: int) -> dict[str, tuple[int, int, int, int]]:
    """face/top/side shades for the cell belonging to this letter of TEXT
    (by letter, not by column -- every cell in "C" shares one color, etc.)."""
    if letter_index < LED_START_INDEX:
        base = CHAIN_COLOR
    else:
        base = LED_COLORS[(letter_index - LED_START_INDEX) % len(LED_COLORS)]
    return {
        "face": (*base, 255),
        "top": _lighten(base, 0.55),
        "side": _darken(base, 0.6),
    }


def _on(grid: list[str], r: int, c: int) -> bool:
    if 0 <= r < GLYPH_ROWS and 0 <= c < GLYPH_COLS:
        return grid[r][c] == "1"
    return False


def render(
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
                            fill=OUTLINE,
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
                                fill=OUTLINE,
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
                                fill=OUTLINE,
                            )

    # Pass 2: front faces (solid fill, per-cell R/G/B).
    for li, ch in enumerate(TEXT):
        grid = FONT_5X7[ch]
        base_col = letter_col_offsets[li]
        for r in range(GLYPH_ROWS):
            for c in range(GLYPH_COLS):
                if grid[r][c] == "1":
                    x0, y0 = cell_origin(base_col, r, c)
                    face = _cell_colors(li)["face"]
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
                    top = _cell_colors(li)["top"]
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
                    side = _cell_colors(li)["side"]
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
    img = render()
    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    img.save(OUTPUT_PATH)
    print(f"{OUTPUT_PATH} ({img.width}x{img.height})")


if __name__ == "__main__":
    main()
