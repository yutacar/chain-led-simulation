/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "subjects.h"
#include "base_model.h"

#include "core/animation.h"
#include "strings.h"

#include "lvgl.h"

namespace viewmodel {

class BaseViewModel {
public:
    BaseViewModel();
    ~BaseViewModel();

    BaseViewModel(const BaseViewModel&) = delete;
    BaseViewModel& operator=(const BaseViewModel&) = delete;

    // ---- Subjects (reactive UI bindings) ----
    lv_subject_t* dark_mode_subject();
    lv_subject_t* current_page_subject();
    lv_subject_t* quit_requested_subject();
    /// True for a short window after the first Esc on the title: a second
    /// Esc inside it quits to the launcher. The title screen observes this
    /// to swap its key-guide hint. Driven by ScreenManager.
    lv_subject_t* title_quit_armed_subject();
    void set_title_quit_armed(bool armed);
    bool title_quit_armed() const;
    /// Bumped (incremented) on every animation/editor state change. Screens
    /// register one observer on this and redraw everything on notification,
    /// the same "dirty counter" pattern used throughout this codebase rather
    /// than a subject per field.
    lv_subject_t* animation_dirty_subject();
    /// Bumped on every language change. ScreenManager observes this and
    /// rebuilds the current screen in place so every tr()/trf() call
    /// re-reads the new language immediately ("selection takes effect
    /// right away" -- see PLAN.md Phase 6).
    lv_subject_t* language_version_subject();

    // ---- App-level ----
    bool is_dark_mode() const;
    model::AppPage current_page() const;
    void show_page(model::AppPage page);
    void request_quit();
    void set_language(i18n::Lang lang);

    // ---- Animation data access ----
    const chainled::core::Animation& animation() const { return animation_; }
    int current_frame_index() const { return current_frame_index_; }
    int cursor_x() const { return cursor_x_; }
    int cursor_y() const { return cursor_y_; }
    chainled::core::Rgb565 current_draw_color() const;
    bool has_saved_animation() const;

    // ---- File I/O (see app/paths.h for the fixed save location) ----
    void new_animation();
    bool load_animation();
    bool save_animation() const;
    /// Removes the save file, if any. Returns false if there was nothing to
    /// delete (has_saved_animation() was already false) or removal failed.
    /// Does not touch the animation currently being edited in memory.
    bool delete_saved_animation() const;

    // ---- Editing ops (Editor screen) ----
    void move_cursor(int dx, int dy);
    void toggle_pixel_at_cursor();
    void cycle_draw_color();
    void flip_current_frame_horizontal();
    void flip_current_frame_vertical();
    void adjust_current_frame_duration(int delta_ms);

    // ---- Frame/timeline ops ----
    void select_frame(int index);
    void next_frame();
    void prev_frame();
    void add_frame();
    void duplicate_current_frame();
    void remove_current_frame();

    // ---- Generator (PLAN.md §2.4): rebuilds frames 1..kMaxFrames-1 from
    // frame 0 via ProceduralCompleter, discarding any existing frames
    // beyond the seed. See src/generator/ for why LlmCompleter isn't wired
    // up here yet (NOTES.md "Phase 3: Go/No-Go").
    void generate_remaining_frames();

    // ---- Distribution (PLAN.md §2.3): the Receive screen calls this once
    // AnimationReceiver reports Done, replacing whatever animation was
    // being edited. Does not touch the save file -- the user still saves
    // explicitly from the Editor if they want to keep it.
    void receive_animation(const chainled::core::Animation& animation);

private:
    void bump_dirty();
    void clamp_current_frame_index();

    model::BaseModel model_;
    chainled::core::Animation animation_;
    int cursor_x_ = 0;
    int cursor_y_ = 0;
    int current_frame_index_ = 0;
    int draw_color_index_ = 0;

    reactive::BoolSubject dark_mode_subject_;
    reactive::IntSubject current_page_subject_;
    reactive::BoolSubject quit_requested_subject_;
    reactive::BoolSubject title_quit_armed_subject_;
    reactive::IntSubject animation_dirty_subject_;
    reactive::IntSubject language_version_subject_;
};

} // namespace viewmodel
