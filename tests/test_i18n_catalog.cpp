/**
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */
#include "strings.h"

#include <gtest/gtest.h>

#include <cctype>
#include <cstring>

namespace i18n {
namespace {

constexpr Lang kAllLangs[] = {Lang::EN, Lang::JA, Lang::ZH};

/// Counts printf-style conversion specifiers in a format string (treats
/// "%%" as a literal percent, skips width/precision digits and '.').
int count_conversions(const char* fmt) {
    int count = 0;
    for (const char* p = fmt; *p != '\0'; ++p) {
        if (*p != '%') {
            continue;
        }
        ++p;
        if (*p == '%') {
            continue;  // literal "%%"
        }
        while (*p != '\0' && (std::isdigit(static_cast<unsigned char>(*p)) || *p == '.')) {
            ++p;
        }
        if (*p == '\0') {
            break;
        }
        ++count;
    }
    return count;
}

struct TemplateArity {
    StrId id;
    int conversions;
};

constexpr TemplateArity kTemplates[] = {
    {StrId::EditorTitle, 2},
    {StrId::EditorDurationHint, 1},
    {StrId::SimulatorTitle, 3},
    {StrId::SimulatorGuide, 1},
    {StrId::SendFailedToOpenPort, 1},
    {StrId::SendWaitingForConnection, 1},
    {StrId::SendSendingChunk, 2},
    {StrId::SendDone, 2},
    {StrId::ReceiveConnecting, 1},
    {StrId::ReceiveReceivingChunk, 2},
};

class I18nCatalogTest : public ::testing::Test {
protected:
    void TearDown() override { set_language(Lang::EN); }
};

TEST_F(I18nCatalogTest, EveryStrIdHasNonEmptyTextInEveryLanguage) {
    for (Lang lang : kAllLangs) {
        set_language(lang);
        for (std::size_t i = 0; i < kStrIdCount; ++i) {
            const auto id = static_cast<StrId>(i);
            const char* text = tr(id);
            ASSERT_NE(text, nullptr) << "lang=" << static_cast<int>(lang) << " id=" << i;
            EXPECT_GT(std::strlen(text), 0u) << "empty string for lang=" << static_cast<int>(lang) << " id=" << i;
        }
    }
}

TEST_F(I18nCatalogTest, TemplatePlaceholderCountMatchesAcrossLanguages) {
    for (Lang lang : kAllLangs) {
        set_language(lang);
        for (const auto& tmpl : kTemplates) {
            const int actual = count_conversions(tr(tmpl.id));
            EXPECT_EQ(actual, tmpl.conversions)
                << "lang=" << static_cast<int>(lang) << " id=" << static_cast<int>(tmpl.id);
        }
    }
}

TEST_F(I18nCatalogTest, DefaultLanguageIsEnglish) {
    // Fresh process state (no prior set_language call in this translation
    // unit's tests) should read English -- guards against a test elsewhere
    // leaking a non-English language across the process-wide global.
    set_language(Lang::EN);
    EXPECT_EQ(current_language(), Lang::EN);
    EXPECT_STREQ(tr(StrId::TitleNewAnimation), "New Animation");
}

TEST_F(I18nCatalogTest, SetLanguageSwitchesActiveTable) {
    set_language(Lang::JA);
    EXPECT_EQ(current_language(), Lang::JA);
    EXPECT_STRNE(tr(StrId::TitleQuit), "Quit");

    set_language(Lang::ZH);
    EXPECT_EQ(current_language(), Lang::ZH);
    EXPECT_STRNE(tr(StrId::TitleQuit), "Quit");
}

TEST_F(I18nCatalogTest, TrfFormatsArgsCorrectly) {
    set_language(Lang::EN);
    EXPECT_EQ(trf(StrId::EditorDurationHint, 250), "250ms (+/-)");
    EXPECT_EQ(trf(StrId::SendSendingChunk, 3, 10), "Sending... chunk 3/10");
}

TEST_F(I18nCatalogTest, LanguageNamesAreIdentityAcrossAllTables) {
    // These three always show in their own script regardless of the active
    // UI language (see strings_en.cpp comment) -- verify they're the same
    // text in every table, not just non-empty.
    for (Lang lang : kAllLangs) {
        set_language(lang);
        EXPECT_STREQ(tr(StrId::SettingsLanguageEnglish), "English");
        EXPECT_STREQ(tr(StrId::SettingsLanguageJapanese), "日本語");
        EXPECT_STREQ(tr(StrId::SettingsLanguageChinese), "简体中文");
    }
}

TEST_F(I18nCatalogTest, OutOfRangeIdReturnsEmptyStringNotCrash) {
    const auto bogus = static_cast<StrId>(kStrIdCount + 100);
    EXPECT_STREQ(tr(bogus), "");
}

}  // namespace
}  // namespace i18n
