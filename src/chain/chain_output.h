/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 *
 * IChainOutput (PLAN.md §2.2): the abstraction that lets the Simulator
 * screen optionally mirror the on-screen animation to a real Chain Mono /
 * Chain RGB unit. RealChainOutput (real_chain_output.h) is the only
 * implementation so far; a desktop-only stand-in isn't needed since the
 * Simulator screen already renders on-screen directly via
 * view::widgets::PixelGrid.
 */
#pragma once

#include "core/frame.h"

namespace chainled::chain {

class IChainOutput {
public:
    virtual ~IChainOutput() = default;

    [[nodiscard]] virtual bool is_connected() const = 0;

    /// Sends `frame` to the connected unit as a SET_DISPLAY_BUFFER command.
    /// `mono` selects the 8-byte bitmask encoding (Chain Mono) over the
    /// 128-byte RGB565 encoding (Chain RGB). Returns false (no-op) if not
    /// connected.
    virtual bool show_frame(const core::Frame& frame, bool mono) = 0;
};

}  // namespace chainled::chain
