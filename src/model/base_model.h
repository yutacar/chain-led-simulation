/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

namespace model {

enum class AppPage {
    Title     = 0,
    Editor    = 1,
    Simulator = 2,
};

class BaseModel {
public:
    bool dark_mode() const;
    void set_dark_mode(bool enabled);

    AppPage current_page() const;
    void set_current_page(AppPage page);

private:
    bool dark_mode_ = false;
    AppPage current_page_ = AppPage::Title;
};

} // namespace model
