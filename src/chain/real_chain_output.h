/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 *
 * RealChainOutput: writes SET_DISPLAY_BUFFER packets to a real Chain Mono /
 * Chain RGB unit over a UART serial port (POSIX termios -- works on both
 * the Linux device target and macOS desktop builds, though only the device
 * target has real hardware to talk to). Never crashes or throws when no
 * device is attached; it simply stays disconnected.
 */
#pragma once

#include <string>

#include "chain_output.h"

namespace chainled::chain {

class RealChainOutput : public IChainOutput {
public:
    /// `device_path` empty (the default) reads the CHAINLED_UART_DEVICE
    /// environment variable instead -- the port is never hard-coded (App
    /// Submission Guide precedent: don't hard-code /dev/fb0 either). If
    /// that's also unset/empty, or the port can't be opened, the instance
    /// stays disconnected rather than guessing a path.
    explicit RealChainOutput(std::string device_path = "");
    ~RealChainOutput() override;

    RealChainOutput(const RealChainOutput&) = delete;
    RealChainOutput& operator=(const RealChainOutput&) = delete;

    [[nodiscard]] bool is_connected() const override;
    bool show_frame(const core::Frame& frame, bool mono) override;

private:
    int fd_ = -1;
};

}  // namespace chainled::chain
