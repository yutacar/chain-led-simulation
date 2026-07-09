/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 *
 * ITransport: a non-blocking, poll-driven byte-stream abstraction (PLAN.md
 * §2.3), adapted from Battleship's src/net/transport.h. Meant to be driven
 * from an LVGL timer tick, not a dedicated I/O thread -- poll() does all
 * the work, send()/receive() never block.
 */
#pragma once

#include <cstdint>
#include <vector>

namespace chainled::net {

enum class TransportState { Connecting, Connected, Disconnected };

class ITransport {
public:
    virtual ~ITransport() = default;

    /// Drives all I/O for this transport (accept/connect progress, socket
    /// reads/writes, fault injection delivery, ...). Call every tick.
    virtual void poll() = 0;

    [[nodiscard]] virtual TransportState state() const = 0;

    /// Queues/writes `len` bytes from `data`. Not a delivery guarantee --
    /// reliability is the caller's job (see AnimationSender/Receiver).
    virtual bool send(const uint8_t* data, size_t len) = 0;
    bool send(const std::vector<uint8_t>& data) { return send(data.data(), data.size()); }

    /// Appends newly-arrived bytes to `out` and clears the internal
    /// receive buffer. Returns the number of bytes appended.
    virtual size_t receive(std::vector<uint8_t>& out) = 0;

    /// Idempotent.
    virtual void close() = 0;
};

}  // namespace chainled::net
