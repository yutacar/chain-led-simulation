/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */
#include "loopback_transport.h"

#include <random>

namespace chainled::net {

void LoopbackTransport::poll() {
    // Delivery happens synchronously in send(); nothing to drive here. Kept
    // as a real override so callers can poll() every ITransport uniformly.
}

bool LoopbackTransport::send(const uint8_t* data, size_t len) {
    if (state_ != TransportState::Connected) {
        return false;
    }

    std::uniform_real_distribution<double> unit(0.0, 1.0);
    if (unit(*rng_) < outgoing_faults_->loss_rate) {
        return true;  // silently dropped
    }

    std::vector<uint8_t> chunk(data, data + len);
    const int copies = (unit(*rng_) < outgoing_faults_->duplicate_rate) ? 2 : 1;
    for (int c = 0; c < copies; ++c) {
        if (outgoing_faults_->fragment_bytes) {
            for (uint8_t b : chunk) {
                outbox_->push_back({b});
            }
        } else {
            outbox_->push_back(chunk);
        }
    }
    return true;
}

size_t LoopbackTransport::receive(std::vector<uint8_t>& out) {
    size_t total = 0;
    while (!inbox_->empty()) {
        const auto& chunk = inbox_->front();
        out.insert(out.end(), chunk.begin(), chunk.end());
        total += chunk.size();
        inbox_->pop_front();
    }
    return total;
}

void LoopbackTransport::close() { state_ = TransportState::Disconnected; }

std::pair<std::unique_ptr<LoopbackTransport>, std::unique_ptr<LoopbackTransport>> make_loopback_pair(
    LoopbackFaultConfig a_to_b, LoopbackFaultConfig b_to_a) {
    auto queue_a_to_b = std::make_shared<std::deque<std::vector<uint8_t>>>();
    auto queue_b_to_a = std::make_shared<std::deque<std::vector<uint8_t>>>();

    auto a = std::make_unique<LoopbackTransport>();
    a->outbox_ = queue_a_to_b;
    a->inbox_ = queue_b_to_a;
    a->outgoing_faults_ = std::make_shared<LoopbackFaultConfig>(a_to_b);
    a->rng_ = std::make_shared<std::mt19937>(a_to_b.seed);
    a->state_ = TransportState::Connected;

    auto b = std::make_unique<LoopbackTransport>();
    b->outbox_ = queue_b_to_a;
    b->inbox_ = queue_a_to_b;
    b->outgoing_faults_ = std::make_shared<LoopbackFaultConfig>(b_to_a);
    b->rng_ = std::make_shared<std::mt19937>(b_to_a.seed);
    b->state_ = TransportState::Connected;

    return {std::move(a), std::move(b)};
}

}  // namespace chainled::net
