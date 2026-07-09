/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 *
 * Wire frame for animation distribution (PLAN.md §2.3). Adapted from
 * Battleship's src/net/frame.h (same envelope shape, same CRC16 choice) --
 * that project's frame codec was already proven against a lossy/fragmenting
 * transport, so this reuses it rather than re-deriving a new one. Only the
 * MAGIC bytes and MessageType set differ (chunked bulk transfer here,
 * turn-based small messages there).
 *
 *   [MAGIC(2B)][ver(1B)][type(1B)][seq(1B)][len(1B)][payload(len B)][CRC16(2B)]
 *
 * All multi-byte integers are big-endian. CRC16 is CRC-16/CCITT-FALSE
 * (poly 0x1021, init 0xFFFF, no reflection, no final xor) over everything
 * from MAGIC through the end of the payload.
 */
#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace chainled::net {

constexpr uint8_t kMagicByte0 = 0xCE;
constexpr uint8_t kMagicByte1 = 0xD1;
constexpr uint8_t kProtocolVersion = 1;

constexpr size_t kHeaderLen = 6;   // MAGIC(2) + ver(1) + type(1) + seq(1) + len(1)
constexpr size_t kCrcLen = 2;
constexpr size_t kMaxPayloadLen = 255;  // len is a single byte
constexpr size_t kMaxFrameLen = kHeaderLen + kMaxPayloadLen + kCrcLen;

enum class MessageType : uint8_t {
    Announce = 1,  ///< sender -> receiver: "I have an animation ready" (total size/chunk count)
    Request = 2,   ///< receiver -> sender: "send it"
    Chunk = 3,     ///< sender -> receiver: one chunk of the serialized animation
    ChunkAck = 4,  ///< receiver -> sender: "got chunk N" (sent even for duplicates)
    Complete = 5,  ///< receiver -> sender: all chunks received and parsed successfully
    Error = 6,     ///< either direction: abort (e.g. reassembled data failed to parse)
};

struct Frame {
    uint8_t version = kProtocolVersion;
    MessageType type = MessageType::Error;
    uint8_t seq = 0;
    std::vector<uint8_t> payload;

    bool operator==(const Frame& other) const {
        return version == other.version && type == other.type && seq == other.seq && payload == other.payload;
    }
};

/// Serializes `frame` to its on-wire byte form. Silently clamps an
/// oversized payload to kMaxPayloadLen rather than throwing -- callers
/// (AnimationSender) are responsible for chunking within that limit.
std::vector<uint8_t> encode_frame(const Frame& frame);

/// Streaming decoder: feed bytes as they arrive off the transport, then
/// call try_extract() repeatedly to drain complete frames. Tolerant of a
/// transport that can split, duplicate, or corrupt bytes arbitrarily: on a
/// CRC mismatch it drops just the 2 magic bytes and resumes scanning,
/// rather than discarding the whole buffered frame, so it self-heals
/// mid-stream instead of getting stuck.
class FrameDecoder {
public:
    void feed(const uint8_t* data, size_t size);
    void feed(const std::vector<uint8_t>& data);

    std::optional<Frame> try_extract();

    [[nodiscard]] size_t buffered_bytes() const { return buffer_.size(); }
    [[nodiscard]] int crc_failures() const { return crc_failures_; }

private:
    std::vector<uint8_t> buffer_;
    int crc_failures_ = 0;
};

}  // namespace chainled::net
