/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */
#include "strings_table.h"

namespace i18n::detail {

const Table kEnglishTable = [] {
    Table t{};
    t[static_cast<std::size_t>(StrId::TitleNewAnimation)] = "New Animation";
    t[static_cast<std::size_t>(StrId::TitleLoadAnimation)] = "Load Animation";
    t[static_cast<std::size_t>(StrId::TitleDeleteAnimation)] = "Delete Animation";
    t[static_cast<std::size_t>(StrId::TitleSendReceive)] = "Send / Receive";
    t[static_cast<std::size_t>(StrId::TitleSendAnimation)] = "Send Animation (LAN)";
    t[static_cast<std::size_t>(StrId::TitleReceiveAnimation)] = "Receive Animation (LAN)";
    t[static_cast<std::size_t>(StrId::TitleSendAnimationBle)] = "Send Animation (BLE)";
    t[static_cast<std::size_t>(StrId::TitleReceiveAnimationBle)] = "Receive Animation (BLE)";
    t[static_cast<std::size_t>(StrId::TitleSettings)] = "Settings";
    t[static_cast<std::size_t>(StrId::TitleGuide)] = "Enter: Select   Esc (hold) / Home: Quit";

    t[static_cast<std::size_t>(StrId::SendReceiveTitle)] = "Send / Receive";
    t[static_cast<std::size_t>(StrId::SendReceiveBack)] = "Back";
    t[static_cast<std::size_t>(StrId::SendReceiveGuide)] = "Enter: Select   Esc: Back";

    t[static_cast<std::size_t>(StrId::EditorTitle)] = "Editor   Frame %d/%d";
    t[static_cast<std::size_t>(StrId::EditorColorLabel)] = "Color:";
    t[static_cast<std::size_t>(StrId::EditorGuideMove)] = "Arrows:Move  Enter:Toggle  C:Color";
    t[static_cast<std::size_t>(StrId::EditorGuideFrameOps)] = "F/V:Flip  N:New  D:Dup  Bksp:Del";
    t[static_cast<std::size_t>(StrId::EditorGuideDurationSave)] = "+/-:Duration  S:Save  P:Play";
    t[static_cast<std::size_t>(StrId::EditorGuideGenerate)] = "G:Generate 10 frames from this one";
    t[static_cast<std::size_t>(StrId::EditorDurationHint)] = "%dms (+/-)";
    t[static_cast<std::size_t>(StrId::EditorSaved)] = "Saved";

    t[static_cast<std::size_t>(StrId::SimulatorTitle)] = "Simulator   Frame %d/%d  [%s]";
    t[static_cast<std::size_t>(StrId::SimulatorModeRgb)] = "RGB";
    t[static_cast<std::size_t>(StrId::SimulatorModeMono)] = "Mono";
    t[static_cast<std::size_t>(StrId::SimulatorGuide)] = "M:Mono/RGB  R:Real[%s]  Esc:Back";
    t[static_cast<std::size_t>(StrId::SimulatorRealOff)] = "OFF";
    t[static_cast<std::size_t>(StrId::SimulatorRealOn)] = "ON";
    t[static_cast<std::size_t>(StrId::SimulatorRealNoDevice)] = "no device";

    t[static_cast<std::size_t>(StrId::SendTitle)] = "Send Animation";
    t[static_cast<std::size_t>(StrId::SendFailedToOpenPort)] = "Failed to open port %u";
    t[static_cast<std::size_t>(StrId::SendWaitingForConnection)] = "Waiting for connection on port %u...";
    t[static_cast<std::size_t>(StrId::SendBleAdvertising)] = "Advertising as %s, waiting for connection...";
    t[static_cast<std::size_t>(StrId::SendBleUnavailable)] = "Bluetooth unavailable";
    t[static_cast<std::size_t>(StrId::SendSendingChunk)] = "Sending... chunk %d/%d";
    t[static_cast<std::size_t>(StrId::SendDone)] = "Done! Sent %d/%d chunks";
    t[static_cast<std::size_t>(StrId::SendGuide)] = "Esc: Cancel / Back";

    t[static_cast<std::size_t>(StrId::ReceiveTitle)] = "Receive Animation";
    t[static_cast<std::size_t>(StrId::ReceiveConnecting)] = "Connecting to 127.0.0.1:%u...";
    t[static_cast<std::size_t>(StrId::ReceiveBleScanning)] = "Scanning for a ChainLED host...";
    t[static_cast<std::size_t>(StrId::ReceiveBleUnavailable)] = "Bluetooth unavailable";
    t[static_cast<std::size_t>(StrId::ReceiveReceivingChunk)] = "Receiving... chunk %d/%d";
    t[static_cast<std::size_t>(StrId::ReceiveWaitingForSender)] = "Connected, waiting for sender...";
    t[static_cast<std::size_t>(StrId::ReceiveDone)] = "Received! Loaded into Editor.";
    t[static_cast<std::size_t>(StrId::ReceiveGuide)] = "Enter: View in Editor (when done)   Esc: Cancel / Back";

    t[static_cast<std::size_t>(StrId::TransferFailed)] = "Transfer failed";

    t[static_cast<std::size_t>(StrId::SettingsTitle)] = "Settings";
    t[static_cast<std::size_t>(StrId::SettingsLanguageLabel)] = "Language:";
    t[static_cast<std::size_t>(StrId::SettingsLanguageEnglish)] = "English";
    // These three always render in their own script regardless of the
    // active UI language (so a user can find their language even if the
    // current one is unreadable to them) -- same text in all three tables.
    t[static_cast<std::size_t>(StrId::SettingsLanguageJapanese)] = "日本語";
    t[static_cast<std::size_t>(StrId::SettingsLanguageChinese)] = "简体中文";
    t[static_cast<std::size_t>(StrId::SettingsBack)] = "Back";
    t[static_cast<std::size_t>(StrId::SettingsGuide)] = "Enter: Select   Esc: Back";

    return t;
}();

}  // namespace i18n::detail
