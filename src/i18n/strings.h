/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 *
 * Self-rolled string catalog (no gettext/ICU dependency), adapted from
 * Battleship's src/i18n/strings.h. Two lookup functions rather than one:
 * tr() for a plain string, trf() for a template with printf-style
 * placeholders -- kept separate because JA/ZH word order can differ from
 * EN, so a translator needs to see "this is a template with N slots" up
 * front rather than a caller silently concatenating pre-translated
 * fragments in English order.
 */
#pragma once

#include <cstdint>
#include <string>

namespace i18n {

enum class Lang : std::uint8_t {
    EN,
    JA,
    ZH,
};

enum class StrId {
    // ---- Title screen ----
    AppTitle,
    TitleNewAnimation,
    TitleLoadAnimation,
    TitleSendAnimation,
    TitleReceiveAnimation,
    TitleSendAnimationBle,     ///< device build (CHAINLED_BLE): "Send Animation (BLE)"
    TitleReceiveAnimationBle,  ///< device build (CHAINLED_BLE): "Receive Animation (BLE)"
    TitleSettings,
    TitleQuit,
    TitleGuide,

    // ---- Editor screen ----
    EditorTitle,             ///< "Editor   Frame %d/%d" (template, %d = index, %d = count)
    EditorColorLabel,
    EditorGuideMove,
    EditorGuideFrameOps,
    EditorGuideDurationSave,
    EditorGuideGenerate,
    EditorDurationHint,      ///< "%dms (+/-)" (template, %d = milliseconds)
    EditorSaved,

    // ---- Simulator screen ----
    SimulatorTitle,          ///< "Simulator   Frame %d/%d  [%s]" (template, %d/%d = index/count, %s = mode label)
    SimulatorModeRgb,
    SimulatorModeMono,
    SimulatorGuide,          ///< "M:Mono/RGB  R:Real[%s]  Esc:Back" (template, %s = real-output status)
    SimulatorRealOff,
    SimulatorRealOn,
    SimulatorRealNoDevice,

    // ---- Send screen ----
    SendTitle,
    SendFailedToOpenPort,    ///< template, %u = port
    SendWaitingForConnection,///< template, %u = port
    SendBleAdvertising,      ///< template, %s = advertised BLE name (device build only)
    SendBleUnavailable,      ///< device build only: no BlueZ adapter/bluetoothd
    SendSendingChunk,        ///< template, %d/%d = chunk/total
    SendDone,                ///< template, %d/%d = chunk/total
    SendGuide,

    // ---- Receive screen ----
    ReceiveTitle,
    ReceiveConnecting,       ///< template, %u = port
    ReceiveBleScanning,      ///< device build only: scanning for a BLE host
    ReceiveBleUnavailable,   ///< device build only: no BlueZ adapter/bluetoothd
    ReceiveReceivingChunk,   ///< template, %d/%d = chunk/total
    ReceiveWaitingForSender,
    ReceiveDone,
    ReceiveGuide,

    // ---- Shared (Send + Receive) ----
    TransferFailed,

    // ---- Settings screen ----
    SettingsTitle,
    SettingsLanguageLabel,
    SettingsLanguageEnglish,
    SettingsLanguageJapanese,
    SettingsLanguageChinese,
    SettingsBack,
    SettingsGuide,

    kCount,  ///< sentinel -- number of entries, not a real string
};

constexpr std::size_t kStrIdCount = static_cast<std::size_t>(StrId::kCount);

Lang current_language();
void set_language(Lang lang);

/// Raw/unformatted lookup. Returns "" (never nullptr) for an out-of-range id.
const char* tr(StrId id);

/// tr(id) treated as a printf-style format string, formatted with the given
/// arguments.
std::string trf(StrId id, ...);

}  // namespace i18n
