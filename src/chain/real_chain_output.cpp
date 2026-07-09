/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */
#include "real_chain_output.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include "chain_commands.h"
#include "frame_codec.h"
#include "protocol.h"

namespace chainled::chain {
namespace {

std::string resolve_device_path(std::string device_path) {
    if (!device_path.empty()) {
        return device_path;
    }
    if (const char* env = std::getenv("CHAINLED_UART_DEVICE"); env && *env) {
        return env;
    }
    return "";
}

int open_serial_port(const std::string& path) {
    if (path.empty()) {
        return -1;
    }

    const int fd = ::open(path.c_str(), O_RDWR | O_NOCTTY);
    if (fd < 0) {
        return -1;
    }

    termios tty{};
    if (tcgetattr(fd, &tty) != 0) {
        ::close(fd);
        return -1;
    }

    cfmakeraw(&tty);
    cfsetispeed(&tty, B115200);
    cfsetospeed(&tty, B115200);
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= static_cast<tcflag_t>(~PARENB);  // 8N1
    tty.c_cflag &= static_cast<tcflag_t>(~CSTOPB);
    tty.c_cflag &= static_cast<tcflag_t>(~CSIZE);
    tty.c_cflag |= CS8;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        ::close(fd);
        return -1;
    }

    return fd;
}

}  // namespace

RealChainOutput::RealChainOutput(std::string device_path) : fd_(open_serial_port(resolve_device_path(device_path))) {}

RealChainOutput::~RealChainOutput() {
    if (fd_ >= 0) {
        ::close(fd_);
    }
}

bool RealChainOutput::is_connected() const { return fd_ >= 0; }

bool RealChainOutput::show_frame(const core::Frame& frame, bool mono) {
    if (!is_connected()) {
        return false;
    }

    Packet packet;
    packet.id = kSingleUnitId;
    packet.cmd = kCmdSetDisplayBuffer;
    if (mono) {
        const auto payload = encode_mono_display_buffer(frame);
        packet.payload.assign(payload.begin(), payload.end());
    } else {
        const auto payload = encode_rgb_display_buffer(frame);
        packet.payload.assign(payload.begin(), payload.end());
    }

    const auto bytes = encode_packet(packet);
    const ssize_t written = ::write(fd_, bytes.data(), bytes.size());
    return written == static_cast<ssize_t>(bytes.size());
}

}  // namespace chainled::chain
