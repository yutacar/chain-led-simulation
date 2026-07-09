/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */

#include "send_screen.h"

#include "asset_manager.h"
#include "bindings.h"
#include "theme.h"

#include "core/animation_serializer.h"
#include "net/animation_transfer.h"
#include "net/tcp_transport.h"

#include <cstdio>
#include <memory>

namespace screen {
namespace {

struct SendUi {
    viewmodel::BaseViewModel* view_model = nullptr;
    std::unique_ptr<chainled::net::TcpTransport> transport;
    std::unique_ptr<chainled::net::AnimationSender> sender;
    lv_obj_t* title_label = nullptr;
    lv_obj_t* status_label = nullptr;
    lv_obj_t* guide_label = nullptr;
    lv_timer_t* timer = nullptr;
    bool listen_failed = false;
};

void refresh(SendUi* ui) {
    if (!ui) {
        return;
    }

    char status_buf[80];
    if (ui->listen_failed) {
        std::snprintf(status_buf, sizeof(status_buf), "Failed to open port %u", chainled::net::kDevTransferPort);
    } else if (!ui->sender) {
        std::snprintf(status_buf, sizeof(status_buf), "Waiting for connection on port %u...",
                      chainled::net::kDevTransferPort);
    } else if (ui->sender->status() == chainled::net::TransferStatus::InProgress &&
               ui->transport->state() != chainled::net::TransportState::Connected) {
        // Only show "waiting" while nothing has happened yet -- once a
        // transfer reaches Done/Failed, keep showing that result even if
        // the receiver disconnects right after (expected: it's done too).
        std::snprintf(status_buf, sizeof(status_buf), "Waiting for connection on port %u...",
                      chainled::net::kDevTransferPort);
    } else {
        switch (ui->sender->status()) {
            case chainled::net::TransferStatus::InProgress:
                std::snprintf(status_buf, sizeof(status_buf), "Sending... chunk %d/%d", ui->sender->chunks_acked(),
                              ui->sender->total_chunks());
                break;
            case chainled::net::TransferStatus::Done:
                std::snprintf(status_buf, sizeof(status_buf), "Done! Sent %d/%d chunks", ui->sender->chunks_acked(),
                              ui->sender->total_chunks());
                break;
            case chainled::net::TransferStatus::Failed:
                std::snprintf(status_buf, sizeof(status_buf), "Transfer failed");
                break;
        }
    }
    lv_label_set_text(ui->status_label, status_buf);
}

void timer_cb(lv_timer_t* timer) {
    auto* ui = static_cast<SendUi*>(lv_timer_get_user_data(timer));
    if (!ui || ui->listen_failed) {
        return;
    }
    if (ui->sender) {
        ui->sender->poll(lv_tick_get());
    } else {
        ui->transport->poll();
    }
    refresh(ui);
}

void delete_cb(lv_event_t* event) {
    auto* ui = static_cast<SendUi*>(lv_event_get_user_data(event));
    if (ui) {
        if (ui->timer) {
            lv_timer_delete(ui->timer);
        }
        delete ui;
    }
}

} // namespace

SendScreen::SendScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

void SendScreen::build_content(lv_obj_t* content) {
    auto* ui = new SendUi();
    ui->view_model = &view_model();
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(content, delete_cb, LV_EVENT_DELETE, ui);

    ui->transport = std::make_unique<chainled::net::TcpTransport>();
    if (ui->transport->listen(chainled::net::kDevTransferPort)) {
        ui->sender = std::make_unique<chainled::net::AnimationSender>(
            *ui->transport, chainled::core::animation_to_text(view_model().animation()));
    } else {
        ui->listen_failed = true;
    }

    auto* small_font = assets().load_font("inter-regular.ttf", 12);

    auto* title = lv_label_create(content);
    lv_label_set_text(title, "Send Animation");
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
    lv_label_set_text(guide, "Esc: Cancel / Back");
    lv_obj_set_style_text_font(guide, small_font ? small_font : &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_opa(guide, LV_OPA_70, 0);
    lv_obj_align(guide, LV_ALIGN_BOTTOM_MID, 0, -4);
    reactive::bind_theme(guide, view_model().dark_mode_subject(), reactive::ThemeRole::Text);
    ui->guide_label = guide;

    refresh(ui);
    ui->timer = lv_timer_create(timer_cb, 30, ui);
}

} // namespace screen
