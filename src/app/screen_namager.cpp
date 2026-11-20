/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */

#include "screen_manager.h"

#include "editor_screen.h"
#include "receive_screen.h"
#include "send_receive_screen.h"
#include "send_screen.h"
#include "settings_screen.h"
#include "simulator_screen.h"
#include "title_screen.h"

namespace app {
namespace {

void flush_requested_page_async(void* user_data) {
    auto* manager = static_cast<ScreenManager*>(user_data);
    if (manager) {
        manager->flush_requested_page();
    }
}

void current_page_observer(lv_observer_t* observer, lv_subject_t* subject) {
    auto* manager = static_cast<ScreenManager*>(lv_observer_get_target(observer));
    if (!manager) {
        return;
    }

    manager->request_page(static_cast<model::AppPage>(lv_subject_get_int(subject)));
}

void language_version_observer(lv_observer_t* observer, lv_subject_t*) {
    auto* manager = static_cast<ScreenManager*>(lv_observer_get_target(observer));
    if (manager) {
        manager->reload_current_screen();
    }
}

/// Hold duration that separates "back one level" from "quit".
constexpr uint32_t kEscLongPressMs = 1000;

/// Window for the second Esc of a double-Esc on the title. A single Esc
/// there only arms the quit, so an Esc that just returned the user to the
/// title can never drop them straight to the launcher.
constexpr uint32_t kTitleDoubleEscWindowMs = 1500;

} // namespace

ScreenManager::ScreenManager(viewmodel::BaseViewModel& view_model, AssetManager& assets)
    : view_model_(view_model), assets_(assets) {
    input_group_ = lv_group_create();
    screen::BaseScreen::set_shared_group(input_group_);
}

ScreenManager::~ScreenManager() {
    lv_async_call_cancel(flush_requested_page_async, this);
    if (title_quit_timer_) {
        lv_timer_delete(title_quit_timer_);
        title_quit_timer_ = nullptr;
    }

    if (page_observer_) {
        lv_observer_remove(page_observer_);
        page_observer_ = nullptr;
    }
    if (language_observer_) {
        lv_observer_remove(language_observer_);
        language_observer_ = nullptr;
    }

    // Screens hold raw lv_obj_t*s registered into input_group_; delete them
    // before destroying the group they're registered in.
    current_screen_.reset();

    screen::BaseScreen::set_shared_group(nullptr);
    if (input_group_) {
        lv_group_delete(input_group_);
        input_group_ = nullptr;
    }
}

void ScreenManager::start() {
    if (page_observer_) {
        return;
    }

    page_observer_ = lv_subject_add_observer_with_target(view_model_.current_page_subject(),
                                                         current_page_observer,
                                                         this,
                                                         nullptr);
    language_observer_ = lv_subject_add_observer_with_target(view_model_.language_version_subject(),
                                                              language_version_observer,
                                                              this,
                                                              nullptr);
    // Kick off the very first screen (Title) without waiting for a subject
    // change notification, since the subject already holds Title's value.
    show_screen_for_page(view_model_.current_page());
    loaded_page_ = view_model_.current_page();
    has_loaded_page_ = true;
}

void ScreenManager::reload_current_screen() {
    show_screen_for_page(has_loaded_page_ ? loaded_page_ : view_model_.current_page());
}

void ScreenManager::request_page(model::AppPage page) {
    requested_page_ = page;

    if (page_switch_scheduled_) {
        return;
    }

    page_switch_scheduled_ = lv_async_call(flush_requested_page_async, this) == LV_RESULT_OK;
}

void ScreenManager::flush_requested_page() {
    page_switch_scheduled_ = false;

    if (has_loaded_page_ && requested_page_ == loaded_page_) {
        return;
    }

    disarm_title_quit();
    show_screen_for_page(requested_page_);
    loaded_page_ = requested_page_;
    has_loaded_page_ = true;
}

lv_obj_t* ScreenManager::current_screen() const {
    return current_screen_ ? current_screen_->root() : nullptr;
}

void ScreenManager::show_screen_for_page(model::AppPage page) {
    switch (page) {
        case model::AppPage::Editor:
            load_screen(std::make_unique<screen::EditorScreen>(view_model_, assets_));
            break;
        case model::AppPage::Simulator:
            load_screen(std::make_unique<screen::SimulatorScreen>(view_model_, assets_));
            break;
        case model::AppPage::Send:
            load_screen(std::make_unique<screen::SendScreen>(view_model_, assets_));
            break;
        case model::AppPage::Receive:
            load_screen(std::make_unique<screen::ReceiveScreen>(view_model_, assets_));
            break;
        case model::AppPage::Settings:
            load_screen(std::make_unique<screen::SettingsScreen>(view_model_, assets_));
            break;
        case model::AppPage::SendReceive:
            load_screen(std::make_unique<screen::SendReceiveScreen>(view_model_, assets_));
            break;
        case model::AppPage::Title:
        default:
            load_screen(std::make_unique<screen::TitleScreen>(view_model_, assets_));
            break;
    }
}

void ScreenManager::load_screen(std::unique_ptr<screen::BaseScreen> next_screen) {
    if (!next_screen || !next_screen->root()) {
        return;
    }

    lv_screen_load(next_screen->root());
    current_screen_ = std::move(next_screen);
}

// ---- Esc-key convention ----

void ScreenManager::attach_esc_handling(lv_indev_t* indev) {
    if (indev) {
        lv_indev_add_event_cb(indev, &ScreenManager::indev_key_cb, LV_EVENT_KEY, this);
    }
}

void ScreenManager::indev_key_cb(lv_event_t* event) {
    auto* manager = static_cast<ScreenManager*>(lv_event_get_user_data(event));
    auto* indev = static_cast<lv_indev_t*>(lv_event_get_target(event));
    if (manager && indev) {
        manager->on_indev_key(indev);
    }
}

void ScreenManager::on_indev_key(lv_indev_t* indev) {
    const uint32_t key = lv_indev_get_key(indev);
    const bool pressed = lv_indev_get_state(indev) == LV_INDEV_STATE_PRESSED;
    const auto page_now = has_loaded_page_ ? loaded_page_ : view_model_.current_page();

    if (pressed) {
        if (key == LV_KEY_ESC) {
            if (!esc_down_) {
                esc_down_ = true;
                esc_long_fired_ = false;
                esc_indev_ = indev;
                esc_page_ = page_now;
                esc_pressed_at_ = lv_tick_get();
            } else if (indev == esc_indev_ && !esc_long_fired_ &&
                       lv_tick_elaps(esc_pressed_at_) >= kEscLongPressMs) {
                // The device keypad indev is in POLL mode, so LV_EVENT_KEY
                // keeps firing (~every read period) while the key stays
                // held -- this branch is what turns a held Esc into quit
                // without waiting for the release.
                esc_long_fired_ = true;
                view_model_.request_quit();
            }
        } else if (key == LV_KEY_HOME) {
            if (!home_down_) {
                home_down_ = true;
                home_indev_ = indev;
                view_model_.request_quit();
            }
        } else if (indev == esc_indev_) {
            // A different key on the same keyboard while Esc tracking was
            // active: abandon the tracking so chorded presses never
            // register as a short Esc.
            esc_down_ = false;
            esc_indev_ = nullptr;
        }
    } else {
        // Release -- but only from the indev that pressed Esc. Other keypad
        // indevs report RELEASED on every idle read (see screen_manager.h);
        // counting those fired a "short Esc" while the key was still held,
        // then re-armed and fired again on the screen it had just opened.
        // The SDL desktop driver's synthetic release read reports key==0,
        // so only the source indev is checked, not the key value.
        if (esc_down_ && indev == esc_indev_) {
            const bool was_short = !esc_long_fired_ && lv_tick_elaps(esc_pressed_at_) < kEscLongPressMs;
            const bool same_page = page_now == esc_page_;
            esc_down_ = false;
            esc_long_fired_ = false;
            esc_indev_ = nullptr;
            if (was_short && same_page) {
                handle_short_esc();
            }
        }
        if (home_down_ && indev == home_indev_) {
            home_down_ = false;
            home_indev_ = nullptr;
        }
    }
}

void ScreenManager::arm_title_quit() {
    view_model_.set_title_quit_armed(true);
    if (title_quit_timer_) {
        lv_timer_reset(title_quit_timer_);
        return;
    }
    title_quit_timer_ = lv_timer_create(&ScreenManager::title_quit_timeout_cb,
                                        kTitleDoubleEscWindowMs, this);
    lv_timer_set_repeat_count(title_quit_timer_, 1);
}

void ScreenManager::disarm_title_quit() {
    if (title_quit_timer_) {
        lv_timer_delete(title_quit_timer_);
        title_quit_timer_ = nullptr;
    }
    view_model_.set_title_quit_armed(false);
}

void ScreenManager::title_quit_timeout_cb(lv_timer_t* timer) {
    auto* manager = static_cast<ScreenManager*>(lv_timer_get_user_data(timer));
    if (!manager) {
        return;
    }
    // repeat_count 1: LVGL deletes the timer itself after this call.
    manager->title_quit_timer_ = nullptr;
    manager->view_model_.set_title_quit_armed(false);
}

void ScreenManager::handle_short_esc() {
    const auto page = has_loaded_page_ ? loaded_page_ : view_model_.current_page();
    switch (page) {
        case model::AppPage::Title:
            // Root screen: "one level up" from the title is the launcher's
            // home screen. Require a double Esc so a single Esc that just
            // returned the user to the title can't also quit; the first one
            // swaps the key-guide hint to "Press Esc again to quit".
            if (view_model_.title_quit_armed()) {
                disarm_title_quit();
                view_model_.request_quit();
            } else {
                arm_title_quit();
            }
            break;
        case model::AppPage::Editor:
            view_model_.show_page(model::AppPage::Title);
            break;
        case model::AppPage::Simulator:
            view_model_.show_page(model::AppPage::Editor);
            break;
        case model::AppPage::Send:
        case model::AppPage::Receive:
            view_model_.show_page(model::AppPage::SendReceive);
            break;
        case model::AppPage::SendReceive:
        case model::AppPage::Settings:
            view_model_.show_page(model::AppPage::Title);
            break;
    }
}

} // namespace app
