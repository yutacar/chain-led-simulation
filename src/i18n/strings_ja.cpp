/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */
#include "strings_table.h"

namespace i18n::detail {

const Table kJapaneseTable = [] {
    Table t{};
    t[static_cast<std::size_t>(StrId::TitleNewAnimation)] = "新規アニメーション";
    t[static_cast<std::size_t>(StrId::TitleLoadAnimation)] = "アニメーションを開く";
    t[static_cast<std::size_t>(StrId::TitleDeleteAnimation)] = "アニメーションを削除";
    t[static_cast<std::size_t>(StrId::TitleSendReceive)] = "送受信";
    t[static_cast<std::size_t>(StrId::TitleSendAnimation)] = "アニメーションを送信 (LAN)";
    t[static_cast<std::size_t>(StrId::TitleReceiveAnimation)] = "アニメーションを受信 (LAN)";
    t[static_cast<std::size_t>(StrId::TitleSendAnimationBle)] = "アニメーションを送信 (BLE)";
    t[static_cast<std::size_t>(StrId::TitleReceiveAnimationBle)] = "アニメーションを受信 (BLE)";
    t[static_cast<std::size_t>(StrId::TitleSettings)] = "設定";
    t[static_cast<std::size_t>(StrId::TitleGuide)] = "Enter:選択   Esc長押し/Home:終了";

    t[static_cast<std::size_t>(StrId::SendReceiveTitle)] = "送受信";
    t[static_cast<std::size_t>(StrId::SendReceiveBack)] = "戻る";
    t[static_cast<std::size_t>(StrId::SendReceiveGuide)] = "Enter:選択   Esc:戻る";

    t[static_cast<std::size_t>(StrId::EditorTitle)] = "エディタ   フレーム %d/%d";
    t[static_cast<std::size_t>(StrId::EditorColorLabel)] = "色:";
    t[static_cast<std::size_t>(StrId::EditorGuideMove)] = "矢印:移動  Enter:切替  C:色";
    t[static_cast<std::size_t>(StrId::EditorGuideFrameOps)] = "F/V:反転  N:追加  D:複製  Bksp:削除";
    t[static_cast<std::size_t>(StrId::EditorGuideDurationSave)] = "+/-:表示時間  S:保存  P:再生";
    t[static_cast<std::size_t>(StrId::EditorGuideGenerate)] = "G:このコマから10コマ自動生成";
    t[static_cast<std::size_t>(StrId::EditorDurationHint)] = "%dms (+/-)";
    t[static_cast<std::size_t>(StrId::EditorSaved)] = "保存しました";

    t[static_cast<std::size_t>(StrId::SimulatorTitle)] = "シミュレータ   フレーム %d/%d  [%s]";
    t[static_cast<std::size_t>(StrId::SimulatorModeRgb)] = "RGB";
    t[static_cast<std::size_t>(StrId::SimulatorModeMono)] = "Mono";
    t[static_cast<std::size_t>(StrId::SimulatorGuide)] = "M:Mono/RGB  R:実機出力[%s]  Esc:戻る";
    t[static_cast<std::size_t>(StrId::SimulatorRealOff)] = "OFF";
    t[static_cast<std::size_t>(StrId::SimulatorRealOn)] = "ON";
    t[static_cast<std::size_t>(StrId::SimulatorRealNoDevice)] = "未接続";

    t[static_cast<std::size_t>(StrId::SendTitle)] = "アニメーション送信";
    t[static_cast<std::size_t>(StrId::SendFailedToOpenPort)] = "ポート %u を開けませんでした";
    t[static_cast<std::size_t>(StrId::SendWaitingForConnection)] = "ポート %u で接続を待機中...";
    t[static_cast<std::size_t>(StrId::SendBleAdvertising)] = "%s としてアドバタイズ中、接続を待機しています...";
    t[static_cast<std::size_t>(StrId::SendBleUnavailable)] = "Bluetoothが利用できません";
    t[static_cast<std::size_t>(StrId::SendSendingChunk)] = "送信中... チャンク %d/%d";
    t[static_cast<std::size_t>(StrId::SendDone)] = "完了！ %d/%d チャンク送信";
    t[static_cast<std::size_t>(StrId::SendGuide)] = "Esc:キャンセル / 戻る";

    t[static_cast<std::size_t>(StrId::ReceiveTitle)] = "アニメーション受信";
    t[static_cast<std::size_t>(StrId::ReceiveConnecting)] = "127.0.0.1:%u に接続中...";
    t[static_cast<std::size_t>(StrId::ReceiveBleScanning)] = "ChainLEDホストをスキャン中...";
    t[static_cast<std::size_t>(StrId::ReceiveBleUnavailable)] = "Bluetoothが利用できません";
    t[static_cast<std::size_t>(StrId::ReceiveReceivingChunk)] = "受信中... チャンク %d/%d";
    t[static_cast<std::size_t>(StrId::ReceiveWaitingForSender)] = "接続済み、送信側を待機中...";
    t[static_cast<std::size_t>(StrId::ReceiveDone)] = "受信完了！エディタに読み込みました";
    t[static_cast<std::size_t>(StrId::ReceiveGuide)] = "Enter:エディタで見る（完了後）   Esc:キャンセル / 戻る";

    t[static_cast<std::size_t>(StrId::TransferFailed)] = "転送に失敗しました";

    t[static_cast<std::size_t>(StrId::SettingsTitle)] = "設定";
    t[static_cast<std::size_t>(StrId::SettingsLanguageLabel)] = "言語:";
    t[static_cast<std::size_t>(StrId::SettingsLanguageEnglish)] = "English";
    t[static_cast<std::size_t>(StrId::SettingsLanguageJapanese)] = "日本語";
    t[static_cast<std::size_t>(StrId::SettingsLanguageChinese)] = "简体中文";
    t[static_cast<std::size_t>(StrId::SettingsBack)] = "戻る";
    t[static_cast<std::size_t>(StrId::SettingsGuide)] = "Enter:選択   Esc:戻る";

    return t;
}();

}  // namespace i18n::detail
