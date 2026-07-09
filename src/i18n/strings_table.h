/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <array>

#include "strings.h"

namespace i18n::detail {

using Table = std::array<const char*, kStrIdCount>;

extern const Table kEnglishTable;
extern const Table kJapaneseTable;
extern const Table kChineseTable;

}  // namespace i18n::detail
