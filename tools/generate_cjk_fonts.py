#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2026 yutacar

SPDX-License-Identifier: MIT

Regenerates assets/fonts/NotoSansJP-Subset.otf and NotoSansSC-Subset.otf
(Phase 6 i18n CJK fonts) so they contain only the glyphs actually referenced
by src/i18n/strings_ja.cpp / strings_zh.cpp, plus basic ASCII (0x20-0x7E) as
a safety net for any Latin characters embedded in those files (e.g. digits,
punctuation, "ms").

Why this exists (PLAN.md Phase 6 design notes, adapted from Battleship's
tools/generate_cjk_fonts.py): CJK fonts are large (Noto Sans JP/SC ship
4-8MB even as the upstream "SubsetOTF" per-region release) and the 512MB-RAM
CardputerZero target plus .deb size budget make full-charset CJK fonts a
non-starter. Subsetting down to only the glyphs the UI actually displays
shrinks each font to well under 50KB.

Run this whenever strings_ja.cpp or strings_zh.cpp gains new CJK characters
that aren't already covered by the current subset -- otherwise those new
glyphs will silently render as tofu (LVGL's FreeType backend has no glyph to
show and falls back to .notdef) on the 320x170 screen. There is no automatic
CI check for this; treat it as a manual step alongside adding new StrId
entries with CJK text.

Requirements (not vendored -- installed on demand):
    pip install fonttools brotli

Usage:
    python3 tools/generate_cjk_fonts.py

Downloads the upstream Noto Sans JP/SC "SubsetOTF" (Google's own per-region
release, still a few MB each) into a local cache directory the first time,
then re-subsets locally on every run (no network access needed after the
first download unless the cache is cleared).
"""
import re
import subprocess
import sys
import urllib.request
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
ASSETS_FONTS_DIR = REPO_ROOT / "assets" / "fonts"
STRINGS_DIR = REPO_ROOT / "src" / "i18n"
CACHE_DIR = REPO_ROOT / ".cache" / "cjk-font-sources"

# Upstream Google Noto CJK repo's own per-region "SubsetOTF" releases -- NOT
# the multi-hundred-MB "super OTC" files; still needs further subsetting to
# the ~100 glyphs this app actually uses, but a much smaller starting point.
SOURCES = {
    "ja": {
        "url": "https://raw.githubusercontent.com/googlefonts/noto-cjk/main/Sans/SubsetOTF/JP/NotoSansJP-Regular.otf",
        "cache_name": "NotoSansJP-Regular.otf",
        "strings_file": STRINGS_DIR / "strings_ja.cpp",
        "output_name": "NotoSansJP-Subset.otf",
    },
    "zh": {
        "url": "https://raw.githubusercontent.com/googlefonts/noto-cjk/main/Sans/SubsetOTF/SC/NotoSansSC-Regular.otf",
        "cache_name": "NotoSansSC-Regular.otf",
        "strings_file": STRINGS_DIR / "strings_zh.cpp",
        "output_name": "NotoSansSC-Subset.otf",
    },
}

ASCII_SAFETY_NET = "".join(chr(c) for c in range(0x20, 0x7F))


def extract_quoted_strings(text: str) -> list[str]:
    """Extracts the contents of every "..." literal in a .cpp file. Good
    enough for strings_<lang>.cpp's simple `t[...] = "...";` assignment
    lines -- not a general C++ string-literal parser (doesn't need to be:
    these files contain no raw strings, no adjacent-literal concatenation,
    and no escaped characters beyond what re handles below)."""
    return re.findall(r'"((?:[^"\\]|\\.)*)"', text)


def collect_charset(strings_file: Path) -> set[str]:
    text = strings_file.read_text(encoding="utf-8")
    chars: set[str] = set()
    for literal in extract_quoted_strings(text):
        chars.update(literal)
    return chars


def ensure_source_font(cache_name: str, url: str) -> Path:
    CACHE_DIR.mkdir(parents=True, exist_ok=True)
    dest = CACHE_DIR / cache_name
    if dest.exists():
        print(f"  using cached {dest}")
        return dest
    print(f"  downloading {url}")
    urllib.request.urlretrieve(url, dest)
    return dest


def subset_font(source_font: Path, charset: set[str], output_path: Path) -> None:
    charset_file = CACHE_DIR / f"{output_path.stem}.charset.txt"
    charset_file.write_text("".join(sorted(charset)), encoding="utf-8")

    cmd = [
        sys.executable,
        "-m",
        "fontTools.subset",
        str(source_font),
        f"--output-file={output_path}",
        f"--text-file={charset_file}",
        "--layout-features=",
        "--glyph-names",
        "--symbol-cmap",
        "--legacy-cmap",
        "--notdef-glyph",
        "--notdef-outline",
        "--recommended-glyphs",
        "--name-IDs=*",
        "--name-legacy",
        "--name-languages=*",
    ]
    subprocess.run(cmd, check=True)


def main() -> None:
    ASSETS_FONTS_DIR.mkdir(parents=True, exist_ok=True)

    for lang, spec in SOURCES.items():
        print(f"[{lang}] collecting glyphs from {spec['strings_file'].relative_to(REPO_ROOT)}")
        charset = collect_charset(spec["strings_file"]) | set(ASCII_SAFETY_NET)
        print(f"[{lang}] {len(charset)} unique characters (incl. ASCII safety net)")

        source_font = ensure_source_font(spec["cache_name"], spec["url"])

        output_path = ASSETS_FONTS_DIR / spec["output_name"]
        print(f"[{lang}] subsetting -> {output_path.relative_to(REPO_ROOT)}")
        subset_font(source_font, charset, output_path)

        size_kb = output_path.stat().st_size / 1024
        print(f"[{lang}] done: {size_kb:.1f} KB\n")

    print("Done. Review `git diff --stat assets/fonts/` and commit the regenerated .otf files.")


if __name__ == "__main__":
    main()
