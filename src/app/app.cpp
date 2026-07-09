/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */

#include "app.h"

#include "asset_manager.h"
#include "fonts.h"
#include "logger.h"
#include "linux_input.h"
#include "screen_manager.h"
#include "screenshot.h"
#include "settings.h"
#include "base_viewmodel.h"
#include "theme.h"

#if !USE_DESKTOP
#include <cstdlib>
#if APP_USE_DRM
#include "src/drivers/display/drm/lv_linux_drm.h"
#else
#include "src/drivers/display/fb/lv_linux_fbdev.h"
#endif
#elif LV_USE_SDL
#include <SDL.h>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#endif

#ifndef APP_FRAMEBUFFER_DEVICE
#define APP_FRAMEBUFFER_DEVICE "/dev/fb0"
#endif

#ifndef APP_DRM_DEVICE
#define APP_DRM_DEVICE "/dev/dri/card0"
#endif

#ifndef APP_DRM_CONNECTOR_ID
#define APP_DRM_CONNECTOR_ID -1
#endif

namespace app {
namespace {

#if USE_DESKTOP && LV_USE_SDL
// F12 debug screenshot capture (see screenshot.h). SDL_AddEventWatch fires
// synchronously as events are pushed onto the queue -- unlike SDL_PollEvent,
// it does not consume them, so it is safe to install alongside LVGL's own
// lv_sdl_window.c event pump without stealing its input. We only set a flag
// here; the actual PNG encode happens on the main loop thread after
// lv_timer_handler() has rendered the frame.
std::atomic<bool> g_screenshot_requested{false};

int SDLCALL screenshot_key_watch(void* user_data, SDL_Event* event) {
    (void)user_data;
    if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_F12 && event->key.repeat == 0) {
        g_screenshot_requested.store(true, std::memory_order_relaxed);
    }
    return 1;
}

std::string next_screenshot_name() {
    static int counter = 0;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "capture_%03d", counter++);
    return buf;
}

// Maps a short mnemonic (used by APP_SCRIPT, see below) to an SDL keycode
// LVGL's lv_sdl_keyboard_handler() understands, or a literal ASCII character
// for plain text keys (Editor's C/F/V/N/D/P/S shortcuts etc.).
SDL_Keycode script_key_to_sdl(const std::string& name) {
    if (name == "up") return SDLK_UP;
    if (name == "down") return SDLK_DOWN;
    if (name == "left") return SDLK_LEFT;
    if (name == "right") return SDLK_RIGHT;
    if (name == "enter") return SDLK_RETURN;
    if (name == "esc") return SDLK_ESCAPE;
    if (name == "home") return SDLK_HOME;
    if (name == "backspace") return SDLK_BACKSPACE;
    if (name.size() == 1) return static_cast<SDL_Keycode>(name[0]);
    return SDLK_UNKNOWN;
}

// LVGL's own SDL keyboard translation entry point (lv_sdl_keyboard.c,
// declared in the private lv_sdl_private.h; forward-declared here to avoid
// dragging that private header onto our include path). lv_sdl_window.c's
// SDL_PollEvent pump calls this for every real SDL_KEYDOWN / SDL_TEXTINPUT
// event.
extern "C" void lv_sdl_keyboard_handler(SDL_Event* event);

// Synthesizes a key press by handing a crafted SDL_Event directly to
// lv_sdl_keyboard_handler() -- the same function every real keystroke goes
// through -- rather than SDL_PushEvent(). Homebrew's "sdl2" is sdl2-compat
// (an SDL2 ABI shim over SDL3); pushing an application-synthesized SDL2
// SDL_TEXTINPUT event through that shim SIGSEGVs in its SDL2->SDL3 event
// conversion. Real keyboard input never takes that path (SDL3 generates its
// text events natively), so this only affects synthesized keys -- the same
// issue already recorded for the Battleship CardputerZero project.
//
// LVGL only maps *control* keys (arrows/Enter/Esc/Backspace/...) from
// SDL_KEYDOWN; plain characters are consumed exclusively from
// SDL_TEXTINPUT's UTF-8 text, hence the two shapes below. windowID stays 0,
// which lv_sdl_keyboard_handler() treats as "no matching display" and falls
// back to the first SDL keyboard indev -- correct here, since the desktop
// build creates exactly one.
void push_synthetic_key(const std::string& name) {
    const SDL_Keycode keycode = script_key_to_sdl(name);
    if (keycode == SDLK_UNKNOWN) {
        return;
    }

    SDL_Event event{};
    if (name.size() == 1) {
        event.type = SDL_TEXTINPUT;
        event.text.text[0] = name[0];
        event.text.text[1] = '\0';
    } else {
        event.type = SDL_KEYDOWN;
        event.key.keysym.sym = keycode;
        event.key.repeat = 0;
    }
    lv_sdl_keyboard_handler(&event);
}

// CHAINLED_WINDOW_ZOOM: desktop-only convenience for test-play. Integer 1-4,
// default 1 (no scaling). Only scales the SDL *window* pixel size -- the
// LVGL logical resolution stays 320x170, so capture_screenshot() and
// APP_SCRIPT input coordinates are unaffected by the zoom level.
int read_window_zoom_from_env() {
    const char* zoom_env = std::getenv("CHAINLED_WINDOW_ZOOM");
    if (!zoom_env || !*zoom_env) {
        return 1;
    }

    char* end = nullptr;
    const long parsed = std::strtol(zoom_env, &end, 10);
    if (end == zoom_env || *end != '\0' || parsed < 1 || parsed > 4) {
        LOG_WARN("CHAINLED_WINDOW_ZOOM='{}' is not an integer in [1,4]; using 1", zoom_env);
        return 1;
    }
    return static_cast<int>(parsed);
}
#endif

void quit_requested_observer(lv_observer_t* observer, lv_subject_t* subject) {
    auto* running = static_cast<bool*>(lv_observer_get_user_data(observer));
    if (running && lv_subject_get_int(subject)) {
        *running = false;
    }
}

lv_display_t* init_display(lv_indev_t** out_keypad_indev) {
#if USE_DESKTOP
    auto* display = lv_sdl_window_create(view::kScreenWidth, view::kScreenHeight);
    if (!display) {
        return nullptr;
    }

    lv_sdl_window_set_title(display, "Chain LED Simulation");
    lv_sdl_window_set_resizeable(display, false);
#if LV_USE_SDL
    const int zoom = read_window_zoom_from_env();
    if (zoom != 1) {
        lv_sdl_window_set_zoom(display, static_cast<float>(zoom));
        LOG_INFO("CHAINLED_WINDOW_ZOOM={}: SDL window scaled to {}x{}", zoom, view::kScreenWidth * zoom,
                 view::kScreenHeight * zoom);
    }
#endif
    lv_sdl_mouse_create();
    lv_sdl_mousewheel_create();
    if (out_keypad_indev) {
        *out_keypad_indev = lv_sdl_keyboard_create();
    }
    return display;
#elif APP_USE_DRM
    auto* display = lv_linux_drm_create();
    if (!display) {
        return nullptr;
    }

    if (lv_linux_drm_set_file(display, APP_DRM_DEVICE, APP_DRM_CONNECTOR_ID) != LV_RESULT_OK) {
        lv_display_delete(display);
        return nullptr;
    }

    LV_UNUSED(out_keypad_indev);
    return display;
#else
    auto* display = lv_linux_fbdev_create();
    if (!display) {
        return nullptr;
    }

    // App Submission Guide: don't hard-code /dev/fb0 -- respect
    // LV_LINUX_FBDEV_DEVICE, which names the real framebuffer node on an
    // APPLaunch-managed device. APP_FRAMEBUFFER_DEVICE is only the
    // build-time fallback for CI/dev builds that never set the env var.
    const char* fbdev_path = APP_FRAMEBUFFER_DEVICE;
    if (const char* env_path = std::getenv("LV_LINUX_FBDEV_DEVICE"); env_path && *env_path) {
        fbdev_path = env_path;
    }

    if (lv_linux_fbdev_set_file(display, fbdev_path) != LV_RESULT_OK) {
        lv_display_delete(display);
        return nullptr;
    }

    LV_UNUSED(out_keypad_indev);
    return display;
#endif
}

} // namespace

int Application::run() {
    std::setvbuf(stdout, nullptr, _IOLBF, 0);

    logger::Logger::init();
    logger::Logger::set_tag("chainled");

    lv_init();

    // Must happen before any screen is built, so every tr()/trf() call
    // during the very first screen construction already sees the saved
    // language.
    i18n::load_settings();

    lv_indev_t* keypad_indev = nullptr;
    auto* display = init_display(&keypad_indev);
    if (!display) {
        LOG_ERROR("failed to initialize display");
        return 1;
    }

    AssetManager assets;
    for (const auto& root : assets.roots()) {
        LOG_INFO("asset root: {}", root.string());
    }
    i18n::init_fonts(assets);

    viewmodel::BaseViewModel view_model;
    view::apply_lvgl_theme(display, view_model.is_dark_mode());

    ScreenManager screen_manager(view_model, assets);
    // Every screen's focusable widgets are registered into this one shared
    // group (see BaseScreen::set_shared_group); make it the LVGL default so
    // both the desktop SDL keyboard indev (bound explicitly below) and any
    // CM0 evdev keypad indevs (bound inside platform::init_key_input, which
    // reads lv_group_get_default()) drive the same focus chain.
    lv_group_set_default(screen_manager.input_group());
    if (keypad_indev) {
        lv_indev_set_group(keypad_indev, screen_manager.input_group());
        screen_manager.attach_esc_handling(keypad_indev);
    }
#if !USE_DESKTOP
    platform::init_key_input(display, [&screen_manager](lv_indev_t* indev) {
        screen_manager.attach_esc_handling(indev);
    });
#endif
    screen_manager.start();

    bool running = true;
    auto* quit_observer = lv_subject_add_observer(view_model.quit_requested_subject(),
                                                  quit_requested_observer,
                                                  &running);

#if USE_DESKTOP && LV_USE_SDL
    SDL_AddEventWatch(screenshot_key_watch, nullptr);
    // APP_SCREENSHOT_SEQUENCE: automated UI smoke tests. Colon-separated
    // "delay_ms:file_stem" entries (e.g. "500:title:1500:editor_blank");
    // each entry captures the active screen to docs/screenshots/<stem>.png
    // `delay_ms` milliseconds after the *previous* entry fired (first entry
    // is relative to startup).
    //
    // APP_SCRIPT: drives a full automated walkthrough. Colon-separated
    // "delay_ms:action" entries, same timing model. `action` is one of:
    //   key:<name>   synthesize a keypress (up/down/left/right/enter/esc/
    //                home/backspace, or a single ASCII character like
    //                c/f/v/n/d/p/s) via lv_sdl_keyboard_handler().
    //   shot:<stem>  capture_screenshot(stem)
    // Example: "300:key:enter:300:key:right:300:shot:editor_focus"
    struct ScheduledStep {
        Uint32 fire_at_ms;
        bool is_key;   // true: action_name is a push_synthetic_key() name.
                       // false: action_name is a capture_screenshot() stem.
        std::string action_name;
    };
    std::vector<ScheduledStep> scheduled_steps;

    // Trims leading/trailing whitespace from a token. Multi-line shell
    // strings can leak a trailing "\n" into a screenshot file name;
    // capture_screenshot() additionally sanitizes its file stem as a second
    // line of defense (see screenshot.cpp, same fix Battleship needed).
    auto trim = [](const std::string& s) -> std::string {
        const auto first = s.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return {};
        const auto last = s.find_last_not_of(" \t\r\n");
        return s.substr(first, last - first + 1);
    };

    auto parse_delay_action_list = [&trim](const char* spec_env, bool default_is_screenshot,
                                           std::vector<ScheduledStep>& out) {
        std::string spec = spec_env;
        Uint32 running_total = SDL_GetTicks();
        size_t pos = 0;
        while (pos < spec.size()) {
            auto colon1 = spec.find(':', pos);
            if (colon1 == std::string::npos) break;
            const std::string delay_str = trim(spec.substr(pos, colon1 - pos));
            if (delay_str.empty()) {
                pos = colon1 + 1;
                continue;
            }
            try {
                size_t chars_consumed = 0;
                const unsigned long delay_ms = std::stoul(delay_str, &chars_consumed);
                if (chars_consumed != delay_str.size()) {
                    LV_LOG_WARN("app script: ignoring malformed delay token '%s'", delay_str.c_str());
                    pos = colon1 + 1;
                    continue;
                }
                running_total += static_cast<Uint32>(delay_ms);
            } catch (const std::exception&) {
                LV_LOG_WARN("app script: ignoring unparsable delay token '%s'", delay_str.c_str());
                pos = colon1 + 1;
                continue;
            }

            size_t action_start = colon1 + 1;
            while (action_start < spec.size() &&
                   std::isspace(static_cast<unsigned char>(spec[action_start]))) {
                ++action_start;  // tolerate "500:\nkey:enter" style line breaks
            }
            bool is_key = !default_is_screenshot;
            if (spec.compare(action_start, 4, "key:") == 0) {
                is_key = true;
                action_start += 4;
            } else if (spec.compare(action_start, 5, "shot:") == 0) {
                is_key = false;
                action_start += 5;
            }

            auto next_colon = spec.find(':', action_start);
            const std::string name = trim(spec.substr(
                action_start, next_colon == std::string::npos ? std::string::npos : next_colon - action_start));
            if (!name.empty()) {
                out.push_back({running_total, is_key, name});
            }

            if (next_colon == std::string::npos) break;
            pos = next_colon + 1;
        }
    };

    if (const char* sequence_env = std::getenv("APP_SCREENSHOT_SEQUENCE")) {
        parse_delay_action_list(sequence_env, /*default_is_screenshot=*/true, scheduled_steps);
    }
    if (const char* script_env = std::getenv("APP_SCRIPT")) {
        parse_delay_action_list(script_env, /*default_is_screenshot=*/false, scheduled_steps);
    }
    std::sort(scheduled_steps.begin(), scheduled_steps.end(),
             [](const auto& a, const auto& b) { return a.fire_at_ms < b.fire_at_ms; });
#endif

    LOG_INFO("LVGL app started at {}x{}", lv_display_get_horizontal_resolution(display),
             lv_display_get_vertical_resolution(display));
    while (running) {
        lv_timer_handler();

#if USE_DESKTOP && LV_USE_SDL
        if (g_screenshot_requested.exchange(false, std::memory_order_relaxed)) {
            capture_screenshot(next_screenshot_name());
        }
        if (!scheduled_steps.empty()) {
            const auto now = SDL_GetTicks();
            while (!scheduled_steps.empty() && now >= scheduled_steps.front().fire_at_ms) {
                const auto& step = scheduled_steps.front();
                if (step.is_key) {
                    push_synthetic_key(step.action_name);
                } else {
                    capture_screenshot(step.action_name);
                }
                scheduled_steps.erase(scheduled_steps.begin());
            }
        }
#endif

        lv_delay_ms(5);
    }

    if (quit_observer) {
        lv_observer_remove(quit_observer);
    }

    return 0;
}

} // namespace app
