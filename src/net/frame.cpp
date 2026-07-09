/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */
#include "frame.h"

#include <algorithm>

namespace chainled::net {
namespace {

uint16_t crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc = static_cast<uint16_t>(crc ^ (static_cast<uint16_t>(data[i]) << 8));
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021) : static_cast<uint16_t>(crc << 1);
        }
    }
    return crc;
}

}  // namespace

std::vector<uint8_t> encode_frame(const Frame& frame) {
    const size_t payload_len = std::min(frame.payload.size(), kMaxPayloadLen);

    std::vector<uint8_t> out;
    out.reserve(kHeaderLen + payload_len + kCrcLen);
    out.push_back(kMagicByte0);
    out.push_back(kMagicByte1);
    out.push_back(frame.version);
    out.push_back(static_cast<uint8_t>(frame.type));
    out.push_back(frame.seq);
    out.push_back(static_cast<uint8_t>(payload_len));
    out.insert(out.end(), frame.payload.begin(), frame.payload.begin() + static_cast<std::ptrdiff_t>(payload_len));

    const uint16_t crc = crc16(out.data(), out.size());
    out.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(crc & 0xFF));
    return out;
}

void FrameDecoder::feed(const uint8_t* data, size_t size) { buffer_.insert(buffer_.end(), data, data + size); }

void FrameDecoder::feed(const std::vector<uint8_t>& data) { feed(data.data(), data.size()); }

std::optional<Frame> FrameDecoder::try_extract() {
    while (true) {
        size_t offset = 0;
        bool found = false;
        for (; offset + 1 < buffer_.size(); ++offset) {
            if (buffer_[offset] == kMagicByte0 && buffer_[offset + 1] == kMagicByte1) {
                found = true;
                break;
            }
        }
        if (!found) {
            if (!buffer_.empty() && buffer_.back() == kMagicByte0) {
                buffer_.erase(buffer_.begin(), buffer_.end() - 1);
            } else {
                buffer_.clear();
            }
            return std::nullopt;
        }
        if (offset > 0) {
            buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<std::ptrdiff_t>(offset));
        }

        if (buffer_.size() < kHeaderLen) {
            return std::nullopt;  // header not fully arrived yet
        }

        const uint8_t len = buffer_[5];
        const size_t frame_len = kHeaderLen + len + kCrcLen;
        if (buffer_.size() < frame_len) {
            return std::nullopt;  // wait for the rest of the frame
        }

        const uint16_t expected_crc = crc16(buffer_.data(), kHeaderLen + len);
        const uint16_t actual_crc = static_cast<uint16_t>((static_cast<uint16_t>(buffer_[frame_len - 2]) << 8) |
                                                            buffer_[frame_len - 1]);
        if (expected_crc != actual_crc) {
            ++crc_failures_;
            buffer_.erase(buffer_.begin(), buffer_.begin() + 2);
            continue;
        }

        Frame frame;
        frame.version = buffer_[2];
        frame.type = static_cast<MessageType>(buffer_[3]);
        frame.seq = buffer_[4];
        frame.payload.assign(buffer_.begin() + static_cast<std::ptrdiff_t>(kHeaderLen),
                              buffer_.begin() + static_cast<std::ptrdiff_t>(kHeaderLen + len));
        buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<std::ptrdiff_t>(frame_len));
        return frame;
    }
}

}  // namespace chainled::net
