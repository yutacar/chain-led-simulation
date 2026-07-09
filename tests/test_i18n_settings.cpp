/**
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */
#include "settings.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace i18n {
namespace {

class I18nSettingsTest : public ::testing::Test {
protected:
    void SetUp() override {
        temp_dir_ = std::filesystem::temp_directory_path() / "chainled_i18n_settings_test";
        std::filesystem::remove_all(temp_dir_);
        setenv("XDG_CONFIG_HOME", temp_dir_.c_str(), 1);
        set_language(Lang::EN);
    }

    void TearDown() override {
        unsetenv("XDG_CONFIG_HOME");
        std::filesystem::remove_all(temp_dir_);
        set_language(Lang::EN);
    }

    std::filesystem::path temp_dir_;
};

TEST_F(I18nSettingsTest, LoadWithNoFileKeepsDefaultLanguage) {
    ASSERT_FALSE(std::filesystem::exists(settings_file_path()));
    load_settings();
    EXPECT_EQ(current_language(), Lang::EN);
}

TEST_F(I18nSettingsTest, SaveThenLoadRoundTripsLanguage) {
    save_language(Lang::JA);
    ASSERT_TRUE(std::filesystem::exists(settings_file_path()));

    set_language(Lang::EN);  // simulate a fresh process before load
    load_settings();
    EXPECT_EQ(current_language(), Lang::JA);
}

TEST_F(I18nSettingsTest, SaveLanguageAppliesImmediately) {
    save_language(Lang::ZH);
    EXPECT_EQ(current_language(), Lang::ZH);
}

TEST_F(I18nSettingsTest, FileFormatIsKeyEqualsValue) {
    save_language(Lang::JA);
    std::ifstream file(settings_file_path());
    ASSERT_TRUE(file.is_open());
    std::string line;
    std::getline(file, line);
    EXPECT_EQ(line, "language=ja");
}

TEST_F(I18nSettingsTest, UnknownKeysAreIgnoredNotFatal) {
    std::filesystem::create_directories(std::filesystem::path(settings_file_path()).parent_path());
    std::ofstream file(settings_file_path(), std::ios::trunc);
    file << "language=zh\nsome_future_key=whatever\n";
    file.close();

    load_settings();
    EXPECT_EQ(current_language(), Lang::ZH);
}

TEST_F(I18nSettingsTest, MalformedLineWithoutEqualsIsSkipped) {
    std::filesystem::create_directories(std::filesystem::path(settings_file_path()).parent_path());
    std::ofstream file(settings_file_path(), std::ios::trunc);
    file << "not a valid line\nlanguage=ja\n";
    file.close();

    load_settings();
    EXPECT_EQ(current_language(), Lang::JA);
}

TEST_F(I18nSettingsTest, UnrecognizedLanguageValueIsIgnored) {
    std::filesystem::create_directories(std::filesystem::path(settings_file_path()).parent_path());
    std::ofstream file(settings_file_path(), std::ios::trunc);
    file << "language=fr\n";
    file.close();

    set_language(Lang::EN);
    load_settings();
    EXPECT_EQ(current_language(), Lang::EN);  // unrecognized value left untouched
}

}  // namespace
}  // namespace i18n
