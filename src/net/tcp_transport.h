/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 *
 * TcpTransport: the desktop/LAN development path for animation transfer
 * (PLAN.md §2.3 -- BLE is the real-device default, added in Phase 7).
 * Plain non-blocking POSIX sockets, adapted from Battleship's
 * src/net/tcp_transport.h. No message framing at this layer -- send()/
 * receive() just move raw bytes; frame.h's FrameDecoder above imposes
 * message boundaries.
 */
#pragma once

#include <string>
#include <vector>

#include "transport.h"

namespace chainled::net {

class TcpTransport : public ITransport {
public:
    TcpTransport() = default;
    ~TcpTransport() override;

    TcpTransport(const TcpTransport&) = delete;
    TcpTransport& operator=(const TcpTransport&) = delete;

    /// Host side. `port` 0 lets the OS assign a port (see local_port()).
    bool listen(uint16_t port);
    /// Client side. Non-blocking; state() reports Connecting until the
    /// handshake completes.
    bool connect(const std::string& host, uint16_t port);

    [[nodiscard]] uint16_t local_port() const { return local_port_; }
    [[nodiscard]] const std::string& peer_address() const { return peer_address_; }

    using ITransport::send;  // otherwise the override below hides the vector<uint8_t> overload

    void poll() override;
    [[nodiscard]] TransportState state() const override { return state_; }
    bool send(const uint8_t* data, size_t len) override;
    size_t receive(std::vector<uint8_t>& out) override;
    void close() override;

private:
    void service_accept();
    void service_connect();
    void service_io();
    void fail();

    int listen_fd_ = -1;
    int fd_ = -1;
    TransportState state_ = TransportState::Disconnected;
    uint16_t local_port_ = 0;
    std::string peer_address_;

    std::vector<uint8_t> send_buffer_;
    std::vector<uint8_t> recv_buffer_;
};

}  // namespace chainled::net
