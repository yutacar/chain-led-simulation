/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */
#include "strings_table.h"

namespace i18n::detail {

const Table kChineseTable = [] {
    Table t{};
    t[static_cast<std::size_t>(StrId::TitleNewAnimation)] = "新建动画";
    t[static_cast<std::size_t>(StrId::TitleLoadAnimation)] = "打开动画";
    t[static_cast<std::size_t>(StrId::TitleSendReceive)] = "收发";
    t[static_cast<std::size_t>(StrId::TitleSendAnimation)] = "发送动画 (局域网)";
    t[static_cast<std::size_t>(StrId::TitleReceiveAnimation)] = "接收动画 (局域网)";
    t[static_cast<std::size_t>(StrId::TitleSendAnimationBle)] = "发送动画 (BLE)";
    t[static_cast<std::size_t>(StrId::TitleReceiveAnimationBle)] = "接收动画 (BLE)";
    t[static_cast<std::size_t>(StrId::TitleSettings)] = "设置";
    t[static_cast<std::size_t>(StrId::TitleGuide)] = "Enter:选择   长按Esc/Home:退出";

    t[static_cast<std::size_t>(StrId::SendReceiveTitle)] = "收发";
    t[static_cast<std::size_t>(StrId::SendReceiveBack)] = "返回";
    t[static_cast<std::size_t>(StrId::SendReceiveGuide)] = "Enter:选择   Esc:返回";

    t[static_cast<std::size_t>(StrId::EditorTitle)] = "编辑器   帧 %d/%d";
    t[static_cast<std::size_t>(StrId::EditorColorLabel)] = "颜色:";
    t[static_cast<std::size_t>(StrId::EditorGuideMove)] = "方向键:移动  Enter:切换  C:颜色";
    t[static_cast<std::size_t>(StrId::EditorGuideFrameOps)] = "F/V:翻转  N:新增  D:复制  Bksp:删除";
    t[static_cast<std::size_t>(StrId::EditorGuideDurationSave)] = "+/-:时长  S:保存  P:播放  X:删除";
    t[static_cast<std::size_t>(StrId::EditorGuideGenerate)] = "G:从此帧生成10帧动画";
    t[static_cast<std::size_t>(StrId::EditorDurationHint)] = "%dms (+/-)";
    t[static_cast<std::size_t>(StrId::EditorSaved)] = "已保存";
    t[static_cast<std::size_t>(StrId::EditorDeleted)] = "已删除";

    t[static_cast<std::size_t>(StrId::SimulatorTitle)] = "模拟器   帧 %d/%d  [%s]";
    t[static_cast<std::size_t>(StrId::SimulatorModeRgb)] = "RGB";
    t[static_cast<std::size_t>(StrId::SimulatorModeMono)] = "Mono";
    t[static_cast<std::size_t>(StrId::SimulatorGuide)] = "M:Mono/RGB  R:实机输出[%s]  Esc:返回";
    t[static_cast<std::size_t>(StrId::SimulatorRealOff)] = "OFF";
    t[static_cast<std::size_t>(StrId::SimulatorRealOn)] = "ON";
    t[static_cast<std::size_t>(StrId::SimulatorRealNoDevice)] = "未连接";

    t[static_cast<std::size_t>(StrId::SendTitle)] = "发送动画";
    t[static_cast<std::size_t>(StrId::SendFailedToOpenPort)] = "无法打开端口 %u";
    t[static_cast<std::size_t>(StrId::SendWaitingForConnection)] = "正在端口 %u 等待连接...";
    t[static_cast<std::size_t>(StrId::SendBleAdvertising)] = "正在以 %s 广播，等待连接...";
    t[static_cast<std::size_t>(StrId::SendBleUnavailable)] = "蓝牙不可用";
    t[static_cast<std::size_t>(StrId::SendSendingChunk)] = "发送中... 数据块 %d/%d";
    t[static_cast<std::size_t>(StrId::SendDone)] = "完成！已发送 %d/%d 块";
    t[static_cast<std::size_t>(StrId::SendGuide)] = "Esc:取消 / 返回";

    t[static_cast<std::size_t>(StrId::ReceiveTitle)] = "接收动画";
    t[static_cast<std::size_t>(StrId::ReceiveConnecting)] = "正在连接 127.0.0.1:%u...";
    t[static_cast<std::size_t>(StrId::ReceiveBleScanning)] = "正在扫描 ChainLED 主机...";
    t[static_cast<std::size_t>(StrId::ReceiveBleUnavailable)] = "蓝牙不可用";
    t[static_cast<std::size_t>(StrId::ReceiveReceivingChunk)] = "接收中... 数据块 %d/%d";
    t[static_cast<std::size_t>(StrId::ReceiveWaitingForSender)] = "已连接，等待发送方...";
    t[static_cast<std::size_t>(StrId::ReceiveDone)] = "接收完成！已加载到编辑器";
    t[static_cast<std::size_t>(StrId::ReceiveGuide)] = "Enter:在编辑器中查看（完成后）   Esc:取消 / 返回";

    t[static_cast<std::size_t>(StrId::TransferFailed)] = "传输失败";

    t[static_cast<std::size_t>(StrId::SettingsTitle)] = "设置";
    t[static_cast<std::size_t>(StrId::SettingsLanguageLabel)] = "语言:";
    t[static_cast<std::size_t>(StrId::SettingsLanguageEnglish)] = "English";
    t[static_cast<std::size_t>(StrId::SettingsLanguageJapanese)] = "日本語";
    t[static_cast<std::size_t>(StrId::SettingsLanguageChinese)] = "简体中文";
    t[static_cast<std::size_t>(StrId::SettingsBack)] = "返回";
    t[static_cast<std::size_t>(StrId::SettingsGuide)] = "Enter:选择   Esc:返回";

    return t;
}();

}  // namespace i18n::detail
