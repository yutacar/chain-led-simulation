/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */
#include "settings.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "app/paths.h"

namespace i18n {
namespace {

const char* lang_to_key(Lang lang) {
    switch (lang) {
        case Lang::JA:
            return "ja";
        case Lang::ZH:
            return "zh";
        case Lang::EN:
        default:
            return "en";
    }
}

bool key_to_lang(const std::string& key, Lang& out) {
    if (key == "en") {
        out = Lang::EN;
        return true;
    }
    if (key == "ja") {
        out = Lang::JA;
        return true;
    }
    if (key == "zh") {
        out = Lang::ZH;
        return true;
    }
    return false;
}

/// Rewrites settings.conf with every known key's current value. Both
/// save_*() entry points (just save_language() so far) funnel through here
/// so persisting one setting never drops another -- the file is written
/// truncate-style, not appended/patched.
void write_settings_file() {
    const auto path = settings_file_path();
    if (path.empty()) {
        return;
    }
    std::error_code ec;
    std::filesystem::create_directories(app::config_dir(), ec);

    std::ofstream file(path, std::ios::trunc);
    if (!file) {
        return;
    }
    file << "language=" << lang_to_key(current_language()) << "\n";
}

}  // namespace

std::string settings_file_path() {
    const auto dir = app::config_dir();
    if (dir.empty()) {
        return {};
    }
    return (std::filesystem::path(dir) / "settings.conf").string();
}

void load_settings() {
    const auto path = settings_file_path();
    if (path.empty()) {
        return;
    }
    std::ifstream file(path);
    if (!file) {
        return;  // no settings file yet -- stay on the caller's default (English)
    }

    std::string line;
    while (std::getline(file, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
            line.pop_back();
        }
        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        const std::string key = line.substr(0, eq);
        const std::string value = line.substr(eq + 1);

        if (key == "language") {
            Lang lang{};
            if (key_to_lang(value, lang)) {
                set_language(lang);
            }
        }
        // Unknown keys are ignored, not an error -- keeps the file
        // forward/backward compatible as more settings are added.
    }
}

void save_language(Lang lang) {
    set_language(lang);
    write_settings_file();
}

}  // namespace i18n
