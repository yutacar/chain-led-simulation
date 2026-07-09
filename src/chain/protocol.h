/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 *
 * Chain Mono/Chain RGB UART daisy-chain protocol (PLAN.md §1.2), reverse
 * engineered from the official m5stack/M5Chain Arduino library source
 * (ChainCommon.cpp) since M5Stack's PDF spec isn't publicly fetchable:
 *
 *   [0xAA][0x55][len_lo][len_hi][id][cmd][payload...][crc8][0x55][0xAA]
 *
 * `len` = id(1) + cmd(1) + crc8(1) + payload(n), little-endian. `crc8` is a
 * plain sum (mod 256) of id, cmd, and payload -- not an actual CRC despite
 * the name inherited from the reference implementation. Pure C++, zero
 * LVGL/platform dependency so the framing logic is fully unit-testable
 * without hardware.
 */
#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace chainled::chain {

constexpr uint8_t kHeaderHigh = 0xAA;
constexpr uint8_t kHeaderLow = 0x55;
constexpr uint8_t kFooterHigh = 0x55;
constexpr uint8_t kFooterLow = 0xAA;

/// header(2) + len(2) + id(1) + cmd(1) + crc8(1) + footer(2), zero payload.
constexpr size_t kMinPacketSize = 9;
/// Generous cap on payload size (largest real payload is the 128-byte RGB
/// display buffer) so a corrupted length field can't make the decoder
/// buffer unbounded data while waiting for a "packet" that will never
/// complete.
constexpr size_t kMaxPacketSize = 300;

struct Packet {
    uint8_t id = 0;
    uint8_t cmd = 0;
    std::vector<uint8_t> payload;

    bool operator==(const Packet& other) const {
        return id == other.id && cmd == other.cmd && payload == other.payload;
    }
    bool operator!=(const Packet& other) const { return !(*this == other); }
};

/// Serializes `packet` to its on-wire byte form.
std::vector<uint8_t> encode_packet(const Packet& packet);

/// Streaming decoder: feed bytes as they arrive off the UART (in whatever
/// chunk sizes the reader happens to get), then poll() repeatedly to drain
/// any complete, valid packets. Invalid/corrupt data (bad CRC, bad footer)
/// is skipped one byte at a time so the decoder resynchronizes on the next
/// real header rather than getting stuck -- the same resilience strategy
/// the reference ChainCommon::processBufferData() uses.
class PacketDecoder {
public:
    void feed(const uint8_t* data, size_t size);
    void feed(const std::vector<uint8_t>& data);

    /// Returns the next complete, valid packet if one is available in the
    /// buffered bytes so far, or std::nullopt if not (either no header
    /// found yet, or a header was found but the rest of the packet hasn't
    /// arrived yet). Safe to call repeatedly to drain multiple packets.
    std::optional<Packet> poll();

private:
    std::vector<uint8_t> buffer_;
};

}  // namespace chainled::chain
