/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */
#include "strings.h"

#include <cstdarg>
#include <cstdio>
#include <vector>

#include "strings_table.h"

namespace i18n {
namespace {

Lang g_current_language = Lang::EN;

const detail::Table& table_for(Lang lang) {
    switch (lang) {
        case Lang::JA:
            return detail::kJapaneseTable;
        case Lang::ZH:
            return detail::kChineseTable;
        case Lang::EN:
        default:
            return detail::kEnglishTable;
    }
}

}  // namespace

Lang current_language() { return g_current_language; }

void set_language(Lang lang) { g_current_language = lang; }

const char* tr(StrId id) {
    const auto index = static_cast<std::size_t>(id);
    if (index >= kStrIdCount) {
        return "";
    }
    const char* text = table_for(g_current_language)[index];
    return text ? text : "";
}

std::string trf(StrId id, ...) {
    const char* fmt = tr(id);

    va_list args;
    va_start(args, id);
    va_list args_copy;
    va_copy(args_copy, args);

    char stack_buf[256];
    const int needed = std::vsnprintf(stack_buf, sizeof(stack_buf), fmt, args);
    va_end(args);

    if (needed < 0) {
        va_end(args_copy);
        return "";
    }
    if (static_cast<std::size_t>(needed) < sizeof(stack_buf)) {
        va_end(args_copy);
        return std::string(stack_buf);
    }

    std::vector<char> heap_buf(static_cast<std::size_t>(needed) + 1);
    std::vsnprintf(heap_buf.data(), heap_buf.size(), fmt, args_copy);
    va_end(args_copy);
    return std::string(heap_buf.data());
}

}  // namespace i18n
