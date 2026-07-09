/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */

#include "receive_screen.h"

#include "asset_manager.h"
#include "bindings.h"
#include "theme.h"

#include "fonts.h"
#include "net/animation_transfer.h"
#include "net/transport.h"
#include "strings.h"

#if CHAINLED_BLE
#include "net/ble/ble_transport.h"
#else
#include "net/tcp_transport.h"
#endif

#include <cstdio>
#include <memory>
#include <string>

namespace screen {
namespace {

struct ReceiveUi {
    viewmodel::BaseViewModel* view_model = nullptr;
#if CHAINLED_BLE
    // Non-null until a host is found and joined -- see timer_cb(). No
    // device-picker UI in v1 (matches the existing "always dial one fixed
    // target" simplification the TCP dev path already uses for Receive):
    // the first discovered ChainLED host is joined automatically.
    std::unique_ptr<chainled::net::BleScanner> scanner;
    bool scan_failed = false;
#endif
    std::unique_ptr<chainled::net::ITransport> transport;
    std::unique_ptr<chainled::net::AnimationReceiver> receiver;
    lv_obj_t* title_label = nullptr;
    lv_obj_t* status_label = nullptr;
    lv_timer_t* timer = nullptr;
    bool applied = false;
};

/// Status text once `transport`/`receiver` exist and a link (TCP or BLE) is
/// at least Connecting -- transport-agnostic, shared by both build
/// configurations.
std::string transfer_status_text(ReceiveUi* ui) {
    switch (ui->receiver->status()) {
        case chainled::net::TransferStatus::InProgress:
            if (ui->receiver->total_chunks() > 0) {
                return i18n::trf(i18n::StrId::ReceiveReceivingChunk, ui->receiver->chunks_received(),
                                 ui->receiver->total_chunks());
            }
            return i18n::tr(i18n::StrId::ReceiveWaitingForSender);
        case chainled::net::TransferStatus::Done:
            return i18n::tr(i18n::StrId::ReceiveDone);
        case chainled::net::TransferStatus::Failed:
            return i18n::tr(i18n::StrId::TransferFailed);
    }
    return {};
}

void refresh(ReceiveUi* ui) {
    if (!ui) {
        return;
    }

    std::string status;
#if CHAINLED_BLE
    if (ui->scan_failed) {
        status = i18n::tr(i18n::StrId::ReceiveBleUnavailable);
    } else if (!ui->transport || ui->transport->state() != chainled::net::TransportState::Connected) {
        // Covers both "still scanning" and "joining, not yet subscribed" --
        // no separate device-path/BD_ADDR display in v1.
        status = i18n::tr(i18n::StrId::ReceiveBleScanning);
    } else {
        status = transfer_status_text(ui);
    }
#else
    if (ui->transport->state() != chainled::net::TransportState::Connected) {
        status = i18n::trf(i18n::StrId::ReceiveConnecting, chainled::net::kDevTransferPort);
    } else {
        status = transfer_status_text(ui);
    }
#endif
    lv_label_set_text(ui->status_label, status.c_str());
}

void timer_cb(lv_timer_t* timer) {
    auto* ui = static_cast<ReceiveUi*>(lv_timer_get_user_data(timer));
    if (!ui) {
        return;
    }
#if CHAINLED_BLE
    if (!ui->transport) {
        if (ui->scanner) {
            ui->scanner->poll(lv_tick_get());
            if (!ui->scanner->results().empty()) {
                auto ble_transport = std::make_unique<chainled::net::BleTransport>();
                if (ble_transport->start_join(ui->scanner->results().front().device_path)) {
                    ui->transport = std::move(ble_transport);
                    ui->receiver = std::make_unique<chainled::net::AnimationReceiver>(*ui->transport);
                    ui->scanner->stop();
                    ui->scanner.reset();
                }
                // start_join() failure: keep scanning: the result list may
                // refresh with a joinable host on a later poll.
            }
        }
        refresh(ui);
        return;
    }
#endif
    ui->receiver->poll(lv_tick_get());
    if (ui->receiver->status() == chainled::net::TransferStatus::Done && !ui->applied) {
        ui->applied = true;
        if (ui->receiver->animation().has_value()) {
            ui->view_model->receive_animation(*ui->receiver->animation());
        }
    }
    refresh(ui);
}

void delete_cb(lv_event_t* event) {
    auto* ui = static_cast<ReceiveUi*>(lv_event_get_user_data(event));
    if (ui) {
        if (ui->timer) {
            lv_timer_delete(ui->timer);
        }
        delete ui;
    }
}

void key_cb(lv_event_t* event) {
    auto* ui = static_cast<ReceiveUi*>(lv_event_get_user_data(event));
    if (!ui || !ui->receiver) {
        return;
    }
    if (lv_event_get_key(event) == LV_KEY_ENTER && ui->receiver->status() == chainled::net::TransferStatus::Done) {
        ui->view_model->show_page(model::AppPage::Editor);
    }
}

} // namespace

ReceiveScreen::ReceiveScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

void ReceiveScreen::build_content(lv_obj_t* content) {
    auto* ui = new ReceiveUi();
    ui->view_model = &view_model();
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(content, delete_cb, LV_EVENT_DELETE, ui);

#if CHAINLED_BLE
    ui->scanner = std::make_unique<chainled::net::BleScanner>();
    ui->scan_failed = !ui->scanner->start();
#else
    auto tcp_transport = std::make_unique<chainled::net::TcpTransport>();
    tcp_transport->connect("127.0.0.1", chainled::net::kDevTransferPort);
    ui->transport = std::move(tcp_transport);
    ui->receiver = std::make_unique<chainled::net::AnimationReceiver>(*ui->transport);
#endif

    lv_obj_add_event_cb(content, key_cb, LV_EVENT_KEY, ui);
    lv_group_add_obj(BaseScreen::shared_group(), content);

    auto* small_font = i18n::font_for_size(12);

    auto* title = lv_label_create(content);
    lv_label_set_text(title, i18n::tr(i18n::StrId::ReceiveTitle));
    lv_obj_set_style_text_font(title, small_font, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 4, 0);
    reactive::bind_theme(title, view_model().dark_mode_subject(), reactive::ThemeRole::Text);
    ui->title_label = title;

    auto* status = lv_label_create(content);
    lv_obj_set_style_text_font(status, small_font, 0);
    lv_obj_center(status);
    reactive::bind_theme(status, view_model().dark_mode_subject(), reactive::ThemeRole::Text);
    ui->status_label = status;

    auto* guide = lv_label_create(content);
    lv_label_set_text(guide, i18n::tr(i18n::StrId::ReceiveGuide));
    lv_obj_set_style_text_font(guide, small_font, 0);
    lv_obj_set_style_text_opa(guide, LV_OPA_70, 0);
    lv_obj_align(guide, LV_ALIGN_BOTTOM_MID, 0, -4);
    reactive::bind_theme(guide, view_model().dark_mode_subject(), reactive::ThemeRole::Text);

    refresh(ui);
    ui->timer = lv_timer_create(timer_cb, 30, ui);
    lv_group_focus_obj(content);
}

} // namespace screen
