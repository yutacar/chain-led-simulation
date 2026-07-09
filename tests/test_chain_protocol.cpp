/**
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */
#include "chain/protocol.h"

#include <gtest/gtest.h>

namespace chainled::chain {
namespace {

TEST(ChainProtocol, EncodesKnownPacketBytes) {
    // Verified against the reference ChainCommon::sendPacket() layout
    // (PLAN.md §1.2): header, little-endian length (id+cmd+crc+payload),
    // id, cmd, payload, crc8 (sum of id/cmd/payload), footer.
    Packet packet;
    packet.id = 1;
    packet.cmd = 0x31;
    packet.payload = {0x10, 0x20, 0x30};

    const auto bytes = encode_packet(packet);
    // header(2) + len(2) + id(1) + cmd(1) + payload(3) + crc(1) + footer(2) = 12
    ASSERT_EQ(bytes.size(), 12u);
    EXPECT_EQ(bytes[0], 0xAA);
    EXPECT_EQ(bytes[1], 0x55);
    // len = 3 (id+cmd+crc) + 3 (payload) = 6
    EXPECT_EQ(bytes[2], 0x06);
    EXPECT_EQ(bytes[3], 0x00);
    EXPECT_EQ(bytes[4], 0x01);  // id
    EXPECT_EQ(bytes[5], 0x31);  // cmd
    EXPECT_EQ(bytes[6], 0x10);
    EXPECT_EQ(bytes[7], 0x20);
    EXPECT_EQ(bytes[8], 0x30);
    // crc8 = sum(id, cmd, payload) mod 256 = 1 + 0x31 + 0x10 + 0x20 + 0x30
    EXPECT_EQ(bytes[9], static_cast<uint8_t>(1 + 0x31 + 0x10 + 0x20 + 0x30));
    EXPECT_EQ(bytes[10], 0x55);
    EXPECT_EQ(bytes[11], 0xAA);
}

TEST(ChainProtocol, RoundTripsThroughDecoder) {
    Packet packet;
    packet.id = 1;
    packet.cmd = 0x31;
    packet.payload = {0, 1, 2, 3, 4, 5, 6, 7};

    PacketDecoder decoder;
    decoder.feed(encode_packet(packet));

    const auto decoded = decoder.poll();
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, packet);
    EXPECT_FALSE(decoder.poll().has_value());
}

TEST(ChainProtocol, RoundTripsEmptyPayload) {
    Packet packet;
    packet.id = 1;
    packet.cmd = 0xFC;

    PacketDecoder decoder;
    decoder.feed(encode_packet(packet));

    const auto decoded = decoder.poll();
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, packet);
}

TEST(ChainProtocol, DecodesMultiplePacketsFromOneBuffer) {
    Packet a;
    a.id = 1;
    a.cmd = 0x31;
    a.payload = {0xAB};
    Packet b;
    b.id = 1;
    b.cmd = 0xE4;

    PacketDecoder decoder;
    decoder.feed(encode_packet(a));
    decoder.feed(encode_packet(b));

    const auto first = decoder.poll();
    const auto second = decoder.poll();
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(*first, a);
    EXPECT_EQ(*second, b);
    EXPECT_FALSE(decoder.poll().has_value());
}

TEST(ChainProtocol, WaitsForDataAcrossPartialFeeds) {
    Packet packet;
    packet.id = 1;
    packet.cmd = 0x31;
    packet.payload = {1, 2, 3, 4};
    const auto bytes = encode_packet(packet);

    PacketDecoder decoder;
    // Feed one byte at a time -- no complete packet until the last byte.
    for (size_t i = 0; i + 1 < bytes.size(); ++i) {
        decoder.feed(&bytes[i], 1);
        EXPECT_FALSE(decoder.poll().has_value());
    }
    decoder.feed(&bytes.back(), 1);
    const auto decoded = decoder.poll();
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, packet);
}

TEST(ChainProtocol, SkipsGarbageBeforeAValidPacket) {
    Packet packet;
    packet.id = 1;
    packet.cmd = 0x31;
    packet.payload = {0x42};

    PacketDecoder decoder;
    std::vector<uint8_t> noise = {0x00, 0xFF, 0xAA, 0x12, 0x34};  // includes a lone 0xAA red herring
    decoder.feed(noise);
    decoder.feed(encode_packet(packet));

    const auto decoded = decoder.poll();
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, packet);
}

TEST(ChainProtocol, RejectsCorruptedCrcAndResyncsToNextPacket) {
    Packet good;
    good.id = 1;
    good.cmd = 0x31;
    good.payload = {0x01};

    Packet corrupt;
    corrupt.id = 1;
    corrupt.cmd = 0x31;
    corrupt.payload = {0x02};
    auto corrupt_bytes = encode_packet(corrupt);
    corrupt_bytes[6] ^= 0xFF;  // flip a payload byte without fixing the CRC

    PacketDecoder decoder;
    decoder.feed(corrupt_bytes);
    decoder.feed(encode_packet(good));

    const auto decoded = decoder.poll();
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, good);
}

TEST(ChainProtocol, RejectsBadFooterAndResyncs) {
    Packet good;
    good.id = 1;
    good.cmd = 0x31;
    good.payload = {0x09};

    Packet broken_footer;
    broken_footer.id = 1;
    broken_footer.cmd = 0x31;
    broken_footer.payload = {0x0A};
    auto broken_bytes = encode_packet(broken_footer);
    broken_bytes.back() = 0x00;  // corrupt the footer's low byte

    PacketDecoder decoder;
    decoder.feed(broken_bytes);
    decoder.feed(encode_packet(good));

    const auto decoded = decoder.poll();
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, good);
}

TEST(ChainProtocol, EmptyBufferHasNoPacket) {
    PacketDecoder decoder;
    EXPECT_FALSE(decoder.poll().has_value());
}

}  // namespace
}  // namespace chainled::chain
