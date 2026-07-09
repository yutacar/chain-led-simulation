/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */

#include "receive_screen.h"

#include "asset_manager.h"
#include "bindings.h"
#include "theme.h"

#include "net/animation_transfer.h"
#include "net/tcp_transport.h"

#include <cstdio>
#include <memory>

namespace screen {
namespace {

struct ReceiveUi {
    viewmodel::BaseViewModel* view_model = nullptr;
    std::unique_ptr<chainled::net::TcpTransport> transport;
    std::unique_ptr<chainled::net::AnimationReceiver> receiver;
    lv_obj_t* title_label = nullptr;
    lv_obj_t* status_label = nullptr;
    lv_timer_t* timer = nullptr;
    bool applied = false;
};

void refresh(ReceiveUi* ui) {
    if (!ui) {
        return;
    }

    char status_buf[80];
    if (ui->transport->state() != chainled::net::TransportState::Connected) {
        std::snprintf(status_buf, sizeof(status_buf), "Connecting to 127.0.0.1:%u...",
                      chainled::net::kDevTransferPort);
    } else {
        switch (ui->receiver->status()) {
            case chainled::net::TransferStatus::InProgress:
                if (ui->receiver->total_chunks() > 0) {
                    std::snprintf(status_buf, sizeof(status_buf), "Receiving... chunk %d/%d",
                                  ui->receiver->chunks_received(), ui->receiver->total_chunks());
                } else {
                    std::snprintf(status_buf, sizeof(status_buf), "Connected, waiting for sender...");
                }
                break;
            case chainled::net::TransferStatus::Done:
                std::snprintf(status_buf, sizeof(status_buf), "Received! Loaded into Editor.");
                break;
            case chainled::net::TransferStatus::Failed:
                std::snprintf(status_buf, sizeof(status_buf), "Transfer failed");
                break;
        }
    }
    lv_label_set_text(ui->status_label, status_buf);
}

void timer_cb(lv_timer_t* timer) {
    auto* ui = static_cast<ReceiveUi*>(lv_timer_get_user_data(timer));
    if (!ui) {
        return;
    }
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
    if (!ui) {
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

    ui->transport = std::make_unique<chainled::net::TcpTransport>();
    ui->transport->connect("127.0.0.1", chainled::net::kDevTransferPort);
    ui->receiver = std::make_unique<chainled::net::AnimationReceiver>(*ui->transport);

    lv_obj_add_event_cb(content, key_cb, LV_EVENT_KEY, ui);
    lv_group_add_obj(BaseScreen::shared_group(), content);

    auto* small_font = assets().load_font("inter-regular.ttf", 12);

    auto* title = lv_label_create(content);
    lv_label_set_text(title, "Receive Animation");
    lv_obj_set_style_text_font(title, small_font ? small_font : &lv_font_montserrat_12, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 4, 0);
    reactive::bind_theme(title, view_model().dark_mode_subject(), reactive::ThemeRole::Text);
    ui->title_label = title;

    auto* status = lv_label_create(content);
    lv_obj_set_style_text_font(status, small_font ? small_font : &lv_font_montserrat_12, 0);
    lv_obj_center(status);
    reactive::bind_theme(status, view_model().dark_mode_subject(), reactive::ThemeRole::Text);
    ui->status_label = status;

    auto* guide = lv_label_create(content);
    lv_label_set_text(guide, "Enter: View in Editor (when done)   Esc: Cancel / Back");
    lv_obj_set_style_text_font(guide, small_font ? small_font : &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_opa(guide, LV_OPA_70, 0);
    lv_obj_align(guide, LV_ALIGN_BOTTOM_MID, 0, -4);
    reactive::bind_theme(guide, view_model().dark_mode_subject(), reactive::ThemeRole::Text);

    refresh(ui);
    ui->timer = lv_timer_create(timer_cb, 30, ui);
    lv_group_focus_obj(content);
}

} // namespace screen
