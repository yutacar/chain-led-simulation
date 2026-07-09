#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2026 yutacar
SPDX-License-Identifier: MIT

Generates assets/images/chainled.png: the 256x256 square app icon used by
app-builder.json's store.icon and installed to APPLaunch's launcher
(cmake/cm0-package.cmake) -- distinct from the wide title_logo.png wordmark
banner shown on the in-app Title screen.

Design: an 8x8 heart pattern rendered as flat colored squares with a small
gap between cells and a dark background -- literally the same visual
language as the in-app pixel grid (src/view/widgets/pixel_grid.cpp: filled
squares, kUnlitColor-style dark "off" cells, ~kCellGap-style spacing), so
the icon reads honestly as "a screenshot of what this app lets you make"
rather than an unrelated logo. Red is the obvious, most recognizable heart
color at small sizes (48x48 launcher icon); no text, per the App
Submission Guide's screenshot/icon guidance.

Requirements (installed on demand into a local venv, not vendored):
    pip install pillow

Usage:
    python3 tools/generate_app_icon.py
"""
from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw

REPO_ROOT = Path(__file__).resolve().parent.parent
OUTPUT_PATH = REPO_ROOT / "assets" / "images" / "chainled.png"

# Classic 8x8 pixel-art heart ('1' = lit cell).
HEART = [
    "01100110",
    "11111111",
    "11111111",
    "11111111",
    "01111110",
    "00111100",
    "00011000",
    "00000000",
]

GRID_SIZE = 8
CANVAS = 256
MARGIN = 8
CELL_GAP = 3

BACKGROUND = (0x10, 0x10, 0x14, 255)  # matches view::color::dark::kBody
LIT_COLOR = (255, 76, 76, 255)  # same red as the title logo's "L" diode
UNLIT_COLOR = (0x28, 0x28, 0x2c, 255)  # slightly lighter than background, matches kUnlitColor's intent


def render() -> Image.Image:
    img = Image.new("RGBA", (CANVAS, CANVAS), BACKGROUND)
    draw = ImageDraw.Draw(img)

    usable = CANVAS - 2 * MARGIN
    cell = (usable - (GRID_SIZE - 1) * CELL_GAP) // GRID_SIZE
    grid_px = cell * GRID_SIZE + (GRID_SIZE - 1) * CELL_GAP
    origin = (CANVAS - grid_px) // 2

    for row in range(GRID_SIZE):
        for col in range(GRID_SIZE):
            lit = HEART[row][col] == "1"
            x0 = origin + col * (cell + CELL_GAP)
            y0 = origin + row * (cell + CELL_GAP)
            draw.rectangle(
                [x0, y0, x0 + cell - 1, y0 + cell - 1],
                fill=LIT_COLOR if lit else UNLIT_COLOR,
            )

    return img


def main() -> None:
    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    img = render()
    img.save(OUTPUT_PATH)
    print(f"{OUTPUT_PATH} ({img.width}x{img.height})")


if __name__ == "__main__":
    main()
