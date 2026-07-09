/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 *
 * Chain series opcode/device-type constants used by ChainLED (subset of
 * the full command set in m5stack/M5Chain's ChainCommon.hpp / ChainMono.hpp
 * / ChainRGB.hpp -- Mono and RGB devices share the same opcode values,
 * interpreted per device type). See PLAN.md §1.2.
 */
#pragma once

#include <cstdint>

namespace chainled::chain {

constexpr uint8_t kCmdSetMode = 0x10;
constexpr uint8_t kCmdSetPixel = 0x30;
constexpr uint8_t kCmdSetDisplayBuffer = 0x31;
constexpr uint8_t kCmdGetDisplayBuffer = 0x33;
constexpr uint8_t kCmdSetRotation = 0xE0;
constexpr uint8_t kCmdSetBrightness = 0xE2;
constexpr uint8_t kCmdClear = 0xE4;

constexpr uint8_t kCmdGetDeviceType = 0xFB;
constexpr uint8_t kCmdEnumPlease = 0xFC;
constexpr uint8_t kCmdHeartbeat = 0xFD;
constexpr uint8_t kCmdEnum = 0xFE;
constexpr uint8_t kCmdReset = 0xFF;

enum class DeviceType : uint16_t {
    Unknown = 0x0000,
    Mono = 0x000D,
    Rgb = 0x000E,
};

/// v1 targets exactly one Chain unit, addressed at chain position 1
/// (PLAN.md §2.2: daisy-chaining multiple units is deliberately out of
/// scope for now, though the protocol's `id` field already supports it).
constexpr uint8_t kSingleUnitId = 1;

}  // namespace chainled::chain
