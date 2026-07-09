/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "base_screen.h"

namespace screen {

/// Chooser between Send and Receive, reached from Title's single combined
/// "Send / Receive" entry (added to keep Title's own button stack short --
/// see NOTES.md). The two buttons reuse the exact same labels
/// (TitleSendAnimation(Ble)/TitleReceiveAnimation(Ble)) Title used to show
/// directly, unchanged wording, just moved one level deeper.
class SendReceiveScreen : public BaseScreen {
public:
    SendReceiveScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);

private:
    void build_content(lv_obj_t* content) override;
};

} // namespace screen
