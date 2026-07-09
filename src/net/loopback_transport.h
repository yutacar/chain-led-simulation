/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 *
 * LoopbackTransport: an in-process ITransport pair for headless testing,
 * with deliberate fault injection (loss/duplication/fragmentation) so the
 * chunked-transfer protocol above it can be soak-tested against a lossy
 * link without any real network. Adapted from Battleship's
 * src/net/loopback_transport.h (same design, same fault knobs).
 */
#pragma once

#include <deque>
#include <memory>
#include <random>
#include <utility>

#include "transport.h"

namespace chainled::net {

struct LoopbackFaultConfig {
    double loss_rate = 0.0;       ///< [0,1] probability a send() is dropped entirely
    double duplicate_rate = 0.0;  ///< [0,1] probability a send() is delivered twice
    bool fragment_bytes = false;  ///< if true, split each send() into single-byte chunks
    uint32_t seed = 1;            ///< seeds the RNG so fault scenarios are reproducible
};

class LoopbackTransport : public ITransport {
public:
    LoopbackTransport() = default;

    using ITransport::send;  // otherwise the override below hides the vector<uint8_t> overload

    void poll() override;
    [[nodiscard]] TransportState state() const override { return state_; }
    bool send(const uint8_t* data, size_t len) override;
    size_t receive(std::vector<uint8_t>& out) override;
    void close() override;

    void set_state_for_testing(TransportState state) { state_ = state; }
    [[nodiscard]] LoopbackFaultConfig& outgoing_faults() { return *outgoing_faults_; }

private:
    friend std::pair<std::unique_ptr<LoopbackTransport>, std::unique_ptr<LoopbackTransport>> make_loopback_pair(
        LoopbackFaultConfig, LoopbackFaultConfig);

    TransportState state_ = TransportState::Connected;
    std::shared_ptr<std::deque<std::vector<uint8_t>>> outbox_;  ///< where this endpoint's sends land (peer's inbox)
    std::shared_ptr<std::deque<std::vector<uint8_t>>> inbox_;   ///< where this endpoint receives from
    std::shared_ptr<LoopbackFaultConfig> outgoing_faults_;
    std::shared_ptr<std::mt19937> rng_;
};

/// Builds a connected pair of LoopbackTransports that deliver into each
/// other. `a_to_b`/`b_to_a` configure fault injection independently per
/// direction (a.send() faults are a_to_b; b.send() faults are b_to_a).
std::pair<std::unique_ptr<LoopbackTransport>, std::unique_ptr<LoopbackTransport>> make_loopback_pair(
    LoopbackFaultConfig a_to_b = {}, LoopbackFaultConfig b_to_a = {});

}  // namespace chainled::net
