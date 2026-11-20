/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */

#include "base_viewmodel.h"

#include "draw_palette.h"

#include "app/paths.h"
#include "core/animation_serializer.h"
#include "generator/procedural_completer.h"
#include "settings.h"

#include <filesystem>

namespace viewmodel {
namespace {

int page_to_int(model::AppPage page) {
    return static_cast<int>(page);
}

int clampi(int value, int lo, int hi) {
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

} // namespace

BaseViewModel::BaseViewModel()
    : dark_mode_subject_(model_.dark_mode()),
      current_page_subject_(page_to_int(model_.current_page())),
      quit_requested_subject_(false),
      animation_dirty_subject_(0),
      language_version_subject_(0) {}

BaseViewModel::~BaseViewModel() = default;

lv_subject_t* BaseViewModel::dark_mode_subject() {
    return dark_mode_subject_.native();
}

lv_subject_t* BaseViewModel::current_page_subject() {
    return current_page_subject_.native();
}

lv_subject_t* BaseViewModel::quit_requested_subject() {
    return quit_requested_subject_.native();
}

lv_subject_t* BaseViewModel::animation_dirty_subject() {
    return animation_dirty_subject_.native();
}

lv_subject_t* BaseViewModel::language_version_subject() {
    return language_version_subject_.native();
}

bool BaseViewModel::is_dark_mode() const {
    return model_.dark_mode();
}

model::AppPage BaseViewModel::current_page() const {
    return model_.current_page();
}

void BaseViewModel::show_page(model::AppPage page) {
    model_.set_current_page(page);
    current_page_subject_.set(page_to_int(page));
}

lv_subject_t* BaseViewModel::title_quit_armed_subject() {
    return title_quit_armed_subject_.native();
}

void BaseViewModel::set_title_quit_armed(bool armed) {
    if (title_quit_armed_subject_.value() != armed) {
        title_quit_armed_subject_.set(armed);
    }
}

bool BaseViewModel::title_quit_armed() const {
    return title_quit_armed_subject_.value();
}

void BaseViewModel::request_quit() {
    quit_requested_subject_.set(true);
}

void BaseViewModel::set_language(i18n::Lang lang) {
    i18n::save_language(lang);  // applies immediately + persists to disk
    // Bump after saving, so ScreenManager's reload sees the already-applied
    // language when it rebuilds the current screen.
    language_version_subject_.set(language_version_subject_.value() + 1);
}

chainled::core::Rgb565 BaseViewModel::current_draw_color() const {
    return kDrawPalette[static_cast<std::size_t>(draw_color_index_)];
}

bool BaseViewModel::has_saved_animation() const {
    return std::filesystem::exists(app::default_animation_save_path());
}

void BaseViewModel::new_animation() {
    animation_ = chainled::core::Animation();
    current_frame_index_ = 0;
    cursor_x_ = 0;
    cursor_y_ = 0;
    draw_color_index_ = 0;
    bump_dirty();
    show_page(model::AppPage::Editor);
}

bool BaseViewModel::load_animation() {
    auto loaded = chainled::core::load_animation_from_file(app::default_animation_save_path());
    if (!loaded.has_value()) {
        return false;
    }
    animation_ = *loaded;
    current_frame_index_ = 0;
    cursor_x_ = 0;
    cursor_y_ = 0;
    bump_dirty();
    show_page(model::AppPage::Editor);
    return true;
}

bool BaseViewModel::save_animation() const {
    std::error_code ec;
    std::filesystem::create_directories(app::config_dir(), ec);
    return chainled::core::save_animation_to_file(animation_, app::default_animation_save_path());
}

bool BaseViewModel::delete_saved_animation() const {
    std::error_code ec;
    return std::filesystem::remove(app::default_animation_save_path(), ec);
}

void BaseViewModel::move_cursor(int dx, int dy) {
    cursor_x_ = clampi(cursor_x_ + dx, 0, chainled::core::kFrameSize - 1);
    cursor_y_ = clampi(cursor_y_ + dy, 0, chainled::core::kFrameSize - 1);
    bump_dirty();
}

void BaseViewModel::toggle_pixel_at_cursor() {
    auto& frame = animation_.frame(current_frame_index_);
    const auto draw_color = current_draw_color();
    const auto existing = frame.pixel(cursor_x_, cursor_y_);
    frame.set_pixel(cursor_x_, cursor_y_, existing == draw_color ? chainled::core::kOffColor : draw_color);
    bump_dirty();
}

void BaseViewModel::cycle_draw_color() {
    draw_color_index_ = static_cast<int>((static_cast<std::size_t>(draw_color_index_) + 1) % kDrawPalette.size());
    bump_dirty();
}

void BaseViewModel::flip_current_frame_horizontal() {
    animation_.frame(current_frame_index_).flip_horizontal();
    bump_dirty();
}

void BaseViewModel::flip_current_frame_vertical() {
    animation_.frame(current_frame_index_).flip_vertical();
    bump_dirty();
}

void BaseViewModel::adjust_current_frame_duration(int delta_ms) {
    animation_.set_frame_duration_ms(current_frame_index_,
                                      animation_.frame_duration_ms(current_frame_index_) + delta_ms);
    bump_dirty();
}

void BaseViewModel::select_frame(int index) {
    if (animation_.valid_index(index)) {
        current_frame_index_ = index;
    }
    bump_dirty();
}

void BaseViewModel::next_frame() {
    select_frame(current_frame_index_ + 1);
}

void BaseViewModel::prev_frame() {
    select_frame(current_frame_index_ - 1);
}

void BaseViewModel::add_frame() {
    if (animation_.add_blank_frame()) {
        current_frame_index_ = animation_.frame_count() - 1;
    }
    bump_dirty();
}

void BaseViewModel::duplicate_current_frame() {
    if (animation_.duplicate_frame(current_frame_index_)) {
        current_frame_index_ += 1;
    }
    bump_dirty();
}

void BaseViewModel::remove_current_frame() {
    animation_.remove_frame(current_frame_index_);
    clamp_current_frame_index();
    bump_dirty();
}

void BaseViewModel::generate_remaining_frames() {
    chainled::generator::ProceduralCompleter completer;
    completer.generate(animation_, chainled::core::kMaxFrames, "");
    clamp_current_frame_index();
    bump_dirty();
}

void BaseViewModel::receive_animation(const chainled::core::Animation& animation) {
    animation_ = animation;
    current_frame_index_ = 0;
    cursor_x_ = 0;
    cursor_y_ = 0;
    bump_dirty();
}

void BaseViewModel::bump_dirty() {
    animation_dirty_subject_.set(animation_dirty_subject_.value() + 1);
}

void BaseViewModel::clamp_current_frame_index() {
    current_frame_index_ = clampi(current_frame_index_, 0, animation_.frame_count() - 1);
}

} // namespace viewmodel
