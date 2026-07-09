/**
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */
#include "net/animation_transfer.h"

#include <gtest/gtest.h>

#include "core/animation_serializer.h"
#include "net/loopback_transport.h"
#include "net/tcp_transport.h"

#include <chrono>
#include <thread>

namespace chainled::net {
namespace {

using chainled::core::Animation;
using chainled::core::animation_to_text;
using chainled::core::rgb565;

Animation make_test_animation() {
    Animation animation;
    animation.frame(0).set_pixel(0, 0, rgb565(255, 0, 0));
    animation.frame(0).set_pixel(7, 7, rgb565(0, 255, 0));
    while (animation.frame_count() < 5) {
        animation.add_blank_frame();
        animation.frame(animation.frame_count() - 1).set_pixel(animation.frame_count(), 3, rgb565(0, 0, 255));
    }
    return animation;
}

/// Drives sender/receiver poll() on a simulated clock (ms per tick) until
/// both reach a terminal status or max_ticks is exhausted.
bool pump_until_done(AnimationSender& sender, AnimationReceiver& receiver, uint32_t ms_per_tick = 5,
                      int max_ticks = 20000) {
    uint32_t now_ms = 0;
    for (int i = 0; i < max_ticks; ++i) {
        sender.poll(now_ms);
        receiver.poll(now_ms);
        if (sender.status() != TransferStatus::InProgress && receiver.status() != TransferStatus::InProgress) {
            return true;
        }
        now_ms += ms_per_tick;
    }
    return false;
}

TEST(AnimationTransfer, CompletesOverCleanLoopback) {
    auto [sender_transport, receiver_transport] = make_loopback_pair();
    const Animation original = make_test_animation();

    AnimationSender sender(*sender_transport, animation_to_text(original));
    AnimationReceiver receiver(*receiver_transport);

    ASSERT_TRUE(pump_until_done(sender, receiver));
    EXPECT_EQ(sender.status(), TransferStatus::Done);
    EXPECT_EQ(receiver.status(), TransferStatus::Done);
    ASSERT_TRUE(receiver.animation().has_value());
    EXPECT_EQ(*receiver.animation(), original);
}

TEST(AnimationTransfer, ChunkAndAckCountsMatch) {
    auto [sender_transport, receiver_transport] = make_loopback_pair();
    const Animation original = make_test_animation();

    AnimationSender sender(*sender_transport, animation_to_text(original));
    AnimationReceiver receiver(*receiver_transport);

    ASSERT_TRUE(pump_until_done(sender, receiver));
    EXPECT_EQ(sender.chunks_acked(), sender.total_chunks());
    EXPECT_EQ(receiver.chunks_received(), receiver.total_chunks());
    EXPECT_EQ(sender.total_chunks(), receiver.total_chunks());
    EXPECT_GT(sender.total_chunks(), 1);  // the test animation is bigger than one chunk
}

TEST(AnimationTransfer, SenderStatusStaysDoneAfterReceiverDisconnects) {
    // Regression test for a real bug found via a real 2-process localhost
    // transfer (not caught by any of the tests above): once Done, the
    // sender kept running WaitingComplete's timeout-driven resend loop
    // forever, and enough idle polling past max_retransmits eventually
    // flipped a successful Done back to Failed.
    auto [sender_transport, receiver_transport] = make_loopback_pair();
    const Animation original = make_test_animation();

    AnimationSender sender(*sender_transport, animation_to_text(original), /*ack_timeout_ms=*/10,
                            /*max_retransmits=*/3);
    AnimationReceiver receiver(*receiver_transport, /*ack_timeout_ms=*/10, /*max_retransmits=*/3);

    ASSERT_TRUE(pump_until_done(sender, receiver, /*ms_per_tick=*/2));
    ASSERT_EQ(sender.status(), TransferStatus::Done);

    receiver_transport->close();  // the receiver has "moved on" (e.g. navigated away)

    uint32_t now_ms = 100000;  // far past every timeout, in one jump
    for (int i = 0; i < 50; ++i) {
        sender.poll(now_ms);
        now_ms += 20;
    }
    EXPECT_EQ(sender.status(), TransferStatus::Done);
}

TEST(AnimationTransfer, FailsCleanlyOnUnparsableData) {
    auto [sender_transport, receiver_transport] = make_loopback_pair();

    AnimationSender sender(*sender_transport, "this is not a valid animation file");
    AnimationReceiver receiver(*receiver_transport);

    ASSERT_TRUE(pump_until_done(sender, receiver));
    EXPECT_EQ(sender.status(), TransferStatus::Failed);
    EXPECT_EQ(receiver.status(), TransferStatus::Failed);
    EXPECT_FALSE(receiver.animation().has_value());
}

TEST(AnimationTransfer, SurvivesLossyDuplicatingFragmentingLink) {
    const Animation original = make_test_animation();
    const std::string serialized = animation_to_text(original);

    for (uint32_t seed = 1; seed <= 5; ++seed) {
        LoopbackFaultConfig a_to_b;
        a_to_b.loss_rate = 0.2;
        a_to_b.duplicate_rate = 0.2;
        a_to_b.seed = seed;
        LoopbackFaultConfig b_to_a;
        b_to_a.loss_rate = 0.2;
        b_to_a.duplicate_rate = 0.2;
        b_to_a.seed = seed + 1000;

        auto [sender_transport, receiver_transport] = make_loopback_pair(a_to_b, b_to_a);
        AnimationSender sender(*sender_transport, serialized, /*ack_timeout_ms=*/50, /*max_retransmits=*/40);
        AnimationReceiver receiver(*receiver_transport, /*ack_timeout_ms=*/50, /*max_retransmits=*/40);

        ASSERT_TRUE(pump_until_done(sender, receiver, /*ms_per_tick=*/5, /*max_ticks=*/40000)) << "seed=" << seed;
        EXPECT_EQ(sender.status(), TransferStatus::Done) << "seed=" << seed;
        EXPECT_EQ(receiver.status(), TransferStatus::Done) << "seed=" << seed;
        ASSERT_TRUE(receiver.animation().has_value()) << "seed=" << seed;
        EXPECT_EQ(*receiver.animation(), original) << "seed=" << seed;
    }
}

TEST(AnimationTransfer, CompletesOverRealTcpSockets) {
    TcpTransport host;
    ASSERT_TRUE(host.listen(0));
    TcpTransport client;
    ASSERT_TRUE(client.connect("127.0.0.1", host.local_port()));

    const Animation original = make_test_animation();
    AnimationSender sender(host, animation_to_text(original));
    AnimationReceiver receiver(client);

    uint32_t now_ms = 0;
    bool done = false;
    for (int i = 0; i < 5000 && !done; ++i) {
        sender.poll(now_ms);
        receiver.poll(now_ms);
        done = sender.status() != TransferStatus::InProgress && receiver.status() != TransferStatus::InProgress;
        now_ms += 5;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    ASSERT_TRUE(done);
    EXPECT_EQ(sender.status(), TransferStatus::Done);
    EXPECT_EQ(receiver.status(), TransferStatus::Done);
    ASSERT_TRUE(receiver.animation().has_value());
    EXPECT_EQ(*receiver.animation(), original);
}

}  // namespace
}  // namespace chainled::net
