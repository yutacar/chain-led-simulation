/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "subjects.h"
#include "base_model.h"

#include "core/animation.h"

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
    /// Bumped (incremented) on every animation/editor state change. Screens
    /// register one observer on this and redraw everything on notification,
    /// the same "dirty counter" pattern used throughout this codebase rather
    /// than a subject per field.
    lv_subject_t* animation_dirty_subject();

    // ---- App-level ----
    bool is_dark_mode() const;
    model::AppPage current_page() const;
    void show_page(model::AppPage page);
    void request_quit();

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
    reactive::IntSubject animation_dirty_subject_;
};

} // namespace viewmodel
