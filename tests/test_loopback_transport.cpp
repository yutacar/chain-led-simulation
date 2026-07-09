/**
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */
#include "net/loopback_transport.h"

#include <gtest/gtest.h>

namespace chainled::net {
namespace {

TEST(LoopbackTransport, DeliversDataToPeer) {
    auto [a, b] = make_loopback_pair();
    const std::vector<uint8_t> data = {1, 2, 3};
    EXPECT_TRUE(a->send(data));

    std::vector<uint8_t> received;
    EXPECT_EQ(b->receive(received), 3u);
    EXPECT_EQ(received, data);
}

TEST(LoopbackTransport, IsBidirectional) {
    auto [a, b] = make_loopback_pair();
    a->send(std::vector<uint8_t>{1});
    b->send(std::vector<uint8_t>{2});

    std::vector<uint8_t> a_received;
    std::vector<uint8_t> b_received;
    a->receive(a_received);
    b->receive(b_received);
    EXPECT_EQ(a_received, (std::vector<uint8_t>{2}));
    EXPECT_EQ(b_received, (std::vector<uint8_t>{1}));
}

TEST(LoopbackTransport, ReceiveDrainsAndClearsInbox) {
    auto [a, b] = make_loopback_pair();
    a->send(std::vector<uint8_t>{1});

    std::vector<uint8_t> first;
    std::vector<uint8_t> second;
    b->receive(first);
    b->receive(second);
    EXPECT_EQ(first.size(), 1u);
    EXPECT_TRUE(second.empty());
}

TEST(LoopbackTransport, LossRateOneDropsEverything) {
    LoopbackFaultConfig faults;
    faults.loss_rate = 1.0;
    auto [a, b] = make_loopback_pair(faults);

    for (int i = 0; i < 20; ++i) {
        a->send(std::vector<uint8_t>{static_cast<uint8_t>(i)});
    }
    std::vector<uint8_t> received;
    EXPECT_EQ(b->receive(received), 0u);
}

TEST(LoopbackTransport, DuplicateRateOneDeliversTwice) {
    LoopbackFaultConfig faults;
    faults.duplicate_rate = 1.0;
    auto [a, b] = make_loopback_pair(faults);

    a->send(std::vector<uint8_t>{42});
    std::vector<uint8_t> received;
    b->receive(received);
    EXPECT_EQ(received, (std::vector<uint8_t>{42, 42}));
}

TEST(LoopbackTransport, FragmentBytesSplitsIntoSingleByteChunks) {
    LoopbackFaultConfig faults;
    faults.fragment_bytes = true;
    auto [a, b] = make_loopback_pair(faults);

    a->send(std::vector<uint8_t>{1, 2, 3});
    // Content is unaffected by fragmentation (only chunk boundaries change,
    // invisible through receive()'s flat byte concatenation) -- fragmenting
    // matters to FrameDecoder tests fed one send() at a time, not here.
    std::vector<uint8_t> received;
    b->receive(received);
    EXPECT_EQ(received, (std::vector<uint8_t>{1, 2, 3}));
}

TEST(LoopbackTransport, FaultsAreIndependentPerDirection) {
    LoopbackFaultConfig a_to_b;
    a_to_b.loss_rate = 1.0;
    LoopbackFaultConfig b_to_a;  // no faults
    auto [a, b] = make_loopback_pair(a_to_b, b_to_a);

    a->send(std::vector<uint8_t>{1});  // lost
    b->send(std::vector<uint8_t>{2});  // delivered

    std::vector<uint8_t> a_received;
    std::vector<uint8_t> b_received;
    a->receive(a_received);
    b->receive(b_received);
    EXPECT_TRUE(b_received.empty());
    EXPECT_EQ(a_received, (std::vector<uint8_t>{2}));
}

TEST(LoopbackTransport, SendFailsWhenNotConnected) {
    auto [a, b] = make_loopback_pair();
    a->close();
    EXPECT_FALSE(a->send(std::vector<uint8_t>{1}));
}

}  // namespace
}  // namespace chainled::net
