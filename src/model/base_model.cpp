/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */

#include "base_model.h"

namespace model {

bool BaseModel::dark_mode() const {
    return dark_mode_;
}

void BaseModel::set_dark_mode(bool enabled) {
    dark_mode_ = enabled;
}

AppPage BaseModel::current_page() const {
    return current_page_;
}

void BaseModel::set_current_page(AppPage page) {
    current_page_ = page;
}

} // namespace model
