/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */
#include "protocol.h"

namespace chainled::chain {

std::vector<uint8_t> encode_packet(const Packet& packet) {
    const uint16_t cmd_size = static_cast<uint16_t>(3 + packet.payload.size());

    std::vector<uint8_t> out;
    out.reserve(kMinPacketSize + packet.payload.size());
    out.push_back(kHeaderHigh);
    out.push_back(kHeaderLow);
    out.push_back(static_cast<uint8_t>(cmd_size & 0xFF));
    out.push_back(static_cast<uint8_t>((cmd_size >> 8) & 0xFF));
    out.push_back(packet.id);
    out.push_back(packet.cmd);
    out.insert(out.end(), packet.payload.begin(), packet.payload.end());

    uint8_t crc = 0;
    for (size_t i = 4; i < out.size(); ++i) {
        crc = static_cast<uint8_t>(crc + out[i]);
    }
    out.push_back(crc);
    out.push_back(kFooterHigh);
    out.push_back(kFooterLow);
    return out;
}

void PacketDecoder::feed(const uint8_t* data, size_t size) { buffer_.insert(buffer_.end(), data, data + size); }

void PacketDecoder::feed(const std::vector<uint8_t>& data) { feed(data.data(), data.size()); }

std::optional<Packet> PacketDecoder::poll() {
    while (true) {
        size_t start = 0;
        bool found = false;
        for (; start + 1 < buffer_.size(); ++start) {
            if (buffer_[start] == kHeaderHigh && buffer_[start + 1] == kHeaderLow) {
                found = true;
                break;
            }
        }
        if (!found) {
            // Keep a lone trailing byte that could be the start of a header
            // split across two feed() calls; drop everything else, since
            // none of it can ever become part of a valid packet.
            if (!buffer_.empty() && buffer_.back() == kHeaderHigh) {
                buffer_.erase(buffer_.begin(), buffer_.end() - 1);
            } else {
                buffer_.clear();
            }
            return std::nullopt;
        }
        if (start > 0) {
            buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<std::ptrdiff_t>(start));
        }

        if (buffer_.size() < 4) {
            return std::nullopt;  // header found, but length field hasn't arrived yet
        }

        const uint16_t length = static_cast<uint16_t>(buffer_[2] | (static_cast<uint16_t>(buffer_[3]) << 8));
        const size_t packet_size = static_cast<size_t>(length) + 6;
        if (packet_size < kMinPacketSize || packet_size > kMaxPacketSize) {
            // Not a real header (or corrupted length); resync past it.
            buffer_.erase(buffer_.begin(), buffer_.begin() + 1);
            continue;
        }
        if (buffer_.size() < packet_size) {
            return std::nullopt;  // wait for the rest of the packet
        }

        const bool footer_ok =
            buffer_[packet_size - 2] == kFooterHigh && buffer_[packet_size - 1] == kFooterLow;
        uint8_t crc = 0;
        for (size_t i = 4; i < packet_size - 3; ++i) {
            crc = static_cast<uint8_t>(crc + buffer_[i]);
        }
        const bool crc_ok = crc == buffer_[packet_size - 3];

        if (!footer_ok || !crc_ok) {
            buffer_.erase(buffer_.begin(), buffer_.begin() + 1);
            continue;
        }

        Packet packet;
        packet.id = buffer_[4];
        packet.cmd = buffer_[5];
        packet.payload.assign(buffer_.begin() + 6, buffer_.begin() + static_cast<std::ptrdiff_t>(packet_size - 3));
        buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<std::ptrdiff_t>(packet_size));
        return packet;
    }
}

}  // namespace chainled::chain
