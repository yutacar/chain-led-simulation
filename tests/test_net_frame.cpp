/**
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */
#include "net/frame.h"

#include <gtest/gtest.h>

namespace chainled::net {
namespace {

TEST(NetFrame, RoundTripsThroughDecoder) {
    Frame frame;
    frame.type = MessageType::Chunk;
    frame.seq = 7;
    frame.payload = {1, 2, 3, 4, 5};

    FrameDecoder decoder;
    decoder.feed(encode_frame(frame));

    const auto decoded = decoder.try_extract();
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, frame);
    EXPECT_FALSE(decoder.try_extract().has_value());
}

TEST(NetFrame, RoundTripsEmptyPayload) {
    Frame frame;
    frame.type = MessageType::Request;

    FrameDecoder decoder;
    decoder.feed(encode_frame(frame));

    const auto decoded = decoder.try_extract();
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, frame);
}

TEST(NetFrame, RoundTripsMaxPayload) {
    Frame frame;
    frame.type = MessageType::Chunk;
    frame.payload.assign(kMaxPayloadLen, 0xAB);

    FrameDecoder decoder;
    decoder.feed(encode_frame(frame));

    const auto decoded = decoder.try_extract();
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->payload.size(), kMaxPayloadLen);
}

TEST(NetFrame, DecodesMultipleFramesFromOneBuffer) {
    Frame a;
    a.type = MessageType::Announce;
    a.payload = {0x01, 0x02};
    Frame b;
    b.type = MessageType::Complete;

    FrameDecoder decoder;
    decoder.feed(encode_frame(a));
    decoder.feed(encode_frame(b));

    const auto first = decoder.try_extract();
    const auto second = decoder.try_extract();
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(*first, a);
    EXPECT_EQ(*second, b);
    EXPECT_FALSE(decoder.try_extract().has_value());
}

TEST(NetFrame, WaitsForDataAcrossPartialFeeds) {
    Frame frame;
    frame.type = MessageType::Chunk;
    frame.payload = {9, 8, 7, 6};
    const auto bytes = encode_frame(frame);

    FrameDecoder decoder;
    for (size_t i = 0; i + 1 < bytes.size(); ++i) {
        decoder.feed(&bytes[i], 1);
        EXPECT_FALSE(decoder.try_extract().has_value());
    }
    decoder.feed(&bytes.back(), 1);
    const auto decoded = decoder.try_extract();
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, frame);
}

TEST(NetFrame, SkipsGarbageBeforeAValidFrame) {
    Frame frame;
    frame.type = MessageType::ChunkAck;
    frame.payload = {3};

    FrameDecoder decoder;
    std::vector<uint8_t> noise = {0x00, 0xFF, kMagicByte0, 0x11, 0x22};  // lone magic-byte0 red herring
    decoder.feed(noise);
    decoder.feed(encode_frame(frame));

    const auto decoded = decoder.try_extract();
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, frame);
}

TEST(NetFrame, RejectsCorruptedCrcAndResyncsToNextFrame) {
    Frame good;
    good.type = MessageType::Chunk;
    good.payload = {1};

    Frame corrupt;
    corrupt.type = MessageType::Chunk;
    corrupt.payload = {2};
    auto corrupt_bytes = encode_frame(corrupt);
    corrupt_bytes[kHeaderLen] ^= 0xFF;  // flip a payload byte without fixing the CRC

    FrameDecoder decoder;
    decoder.feed(corrupt_bytes);
    decoder.feed(encode_frame(good));

    const auto decoded = decoder.try_extract();
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, good);
    EXPECT_GT(decoder.crc_failures(), 0);
}

TEST(NetFrame, EmptyBufferHasNoFrame) {
    FrameDecoder decoder;
    EXPECT_FALSE(decoder.try_extract().has_value());
}

}  // namespace
}  // namespace chainled::net
