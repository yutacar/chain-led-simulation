/**
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */
#include "screenshot.h"

#include "logger.h"

#include "lvgl.h"

#if LV_USE_SNAPSHOT

#include <png.h>

#include <cctype>
#include <cstdio>
#include <filesystem>
#include <vector>

namespace app {
namespace {

// Writes an ARGB8888 (LVGL byte order: B,G,R,A per pixel, native endian
// little-endian host) draw buffer out as an 8-bit RGB PNG using libpng
// directly. LVGL's lv_libpng.cpp only *decodes* PNGs for use as image
// sources, so there's no ready-made encode path -- this is a small, self
// contained encoder good enough for debug screenshots.
bool write_argb8888_as_png(const std::string& path, const uint8_t* data, uint32_t width, uint32_t height,
                            uint32_t stride) {
    FILE* fp = std::fopen(path.c_str(), "wb");
    if (!fp) {
        LOG_ERROR("screenshot: failed to open {} for writing", path);
        return false;
    }

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png) {
        std::fclose(fp);
        return false;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_write_struct(&png, nullptr);
        std::fclose(fp);
        return false;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        std::fclose(fp);
        return false;
    }

    png_init_io(png, fp);
    png_set_IHDR(png, info, width, height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);

    std::vector<uint8_t> row(static_cast<size_t>(width) * 3);
    for (uint32_t y = 0; y < height; ++y) {
        const uint8_t* src_row = data + static_cast<size_t>(y) * stride;
        for (uint32_t x = 0; x < width; ++x) {
            // LVGL ARGB8888 pixel layout in memory (little-endian host): B, G, R, A.
            const uint8_t* px = src_row + static_cast<size_t>(x) * 4;
            row[static_cast<size_t>(x) * 3 + 0] = px[2];  // R
            row[static_cast<size_t>(x) * 3 + 1] = px[1];  // G
            row[static_cast<size_t>(x) * 3 + 2] = px[0];  // B
        }
        png_write_row(png, row.data());
    }

    png_write_end(png, nullptr);
    png_destroy_write_struct(&png, &info);
    std::fclose(fp);
    return true;
}

}  // namespace

// File-name safety: callers hand us arbitrary strings -- notably stems
// parsed out of the APP_SCRIPT / APP_SCREENSHOT_SEQUENCE environment
// variables, which users naturally write as multi-line shell strings.
// std::stoul silently skips leading whitespace when parsing the delay
// tokens, so a trailing "\n" from a multi-line spec could otherwise survive
// all the way into the PNG file name. Keep only [A-Za-z0-9._-]: whitespace
// is dropped, any other character becomes '_'.
std::string sanitize_name(const std::string& name) {
    std::string out;
    out.reserve(name.size());
    for (const char c : name) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' ||
                        c == '_' || c == '-';
        if (ok) {
            out += c;
        } else if (!std::isspace(static_cast<unsigned char>(c))) {
            out += '_';
        }
    }
    return out;
}

bool capture_screenshot(const std::string& name) {
    const std::string safe_name = sanitize_name(name);
    if (safe_name.empty()) {
        LOG_ERROR("screenshot: file stem is empty after sanitizing");
        return false;
    }

    lv_obj_t* screen = lv_screen_active();
    if (!screen) {
        LOG_ERROR("screenshot: no active screen");
        return false;
    }

    lv_draw_buf_t* draw_buf = lv_snapshot_take(screen, LV_COLOR_FORMAT_ARGB8888);
    if (!draw_buf) {
        LOG_ERROR("screenshot: lv_snapshot_take failed");
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(kScreenshotDir, ec);

    const auto path = (std::filesystem::path(kScreenshotDir) / (safe_name + ".png")).string();
    const bool ok = write_argb8888_as_png(path, draw_buf->data, draw_buf->header.w, draw_buf->header.h,
                                           draw_buf->header.stride);

    lv_draw_buf_destroy(draw_buf);

    if (ok) {
        LOG_INFO("screenshot saved: {}", path);
    }
    return ok;
}

}  // namespace app

#else  // !LV_USE_SNAPSHOT

namespace app {

bool capture_screenshot(const std::string& name) {
    LV_UNUSED(name);
    LOG_WARN("screenshot: LV_USE_SNAPSHOT is disabled, cannot capture");
    return false;
}

}  // namespace app

#endif
