/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */
#include "animation_transfer.h"

#include <algorithm>

#include "core/animation_serializer.h"

namespace chainled::net {

namespace {

int chunk_count_for(size_t total_size) {
    return static_cast<int>((total_size + kChunkDataSize - 1) / kChunkDataSize);
}

}  // namespace

// ---- AnimationSender ----

AnimationSender::AnimationSender(ITransport& transport, std::string serialized_animation, uint32_t ack_timeout_ms,
                                  int max_retransmits)
    : transport_(transport),
      data_(serialized_animation.begin(), serialized_animation.end()),
      total_chunks_(chunk_count_for(data_.size())),
      ack_timeout_ms_(ack_timeout_ms),
      max_retransmits_(max_retransmits) {}

void AnimationSender::send_announce() {
    Frame frame;
    frame.type = MessageType::Announce;
    frame.payload = {
        static_cast<uint8_t>((data_.size() >> 8) & 0xFF),
        static_cast<uint8_t>(data_.size() & 0xFF),
        static_cast<uint8_t>(total_chunks_),
    };
    transport_.send(encode_frame(frame));
}

void AnimationSender::send_chunk(int index) {
    const size_t offset = static_cast<size_t>(index) * kChunkDataSize;
    const size_t len = std::min(kChunkDataSize, data_.size() - offset);

    Frame frame;
    frame.type = MessageType::Chunk;
    frame.payload.reserve(2 + len);
    frame.payload.push_back(static_cast<uint8_t>(index));
    frame.payload.push_back(static_cast<uint8_t>(total_chunks_));
    frame.payload.insert(frame.payload.end(), data_.begin() + static_cast<std::ptrdiff_t>(offset),
                          data_.begin() + static_cast<std::ptrdiff_t>(offset + len));
    transport_.send(encode_frame(frame));
}

void AnimationSender::poll(uint32_t now_ms) {
    transport_.poll();
    if (status_ != TransferStatus::InProgress) {
        // Once Done/Failed, stop running the retry state machine -- without
        // this, WaitingComplete's timeout-driven resend loop keeps firing
        // forever (nothing ever answers a resend after the receiver has
        // moved on), and enough idle ticks past max_retransmits_ would flip
        // a successful Done back to Failed. Found via a real 2-process
        // localhost transfer, not a unit test (see NOTES.md).
        return;
    }
    std::vector<uint8_t> raw;
    transport_.receive(raw);
    decoder_.feed(raw);

    switch (state_) {
        case State::WaitingConnection:
            if (transport_.state() == TransportState::Connected) {
                send_announce();
                state_ = State::SentAnnounce;
                last_sent_at_ms_ = now_ms;
                retransmit_count_ = 0;
            } else if (transport_.state() == TransportState::Disconnected) {
                status_ = TransferStatus::Failed;
            }
            break;

        case State::SentAnnounce: {
            bool got_request = false;
            std::optional<Frame> frame;
            while ((frame = decoder_.try_extract()).has_value()) {
                if (frame->type == MessageType::Request) {
                    got_request = true;
                }
            }
            if (got_request) {
                current_chunk_ = 0;
                send_chunk(current_chunk_);
                state_ = State::WaitingAck;
                last_sent_at_ms_ = now_ms;
                retransmit_count_ = 0;
            } else if (now_ms - last_sent_at_ms_ >= ack_timeout_ms_) {
                if (++retransmit_count_ > max_retransmits_) {
                    status_ = TransferStatus::Failed;
                } else {
                    send_announce();
                    last_sent_at_ms_ = now_ms;
                }
            }
            break;
        }

        case State::WaitingAck: {
            bool advanced = false;
            std::optional<Frame> frame;
            while ((frame = decoder_.try_extract()).has_value()) {
                if (!advanced && frame->type == MessageType::ChunkAck && frame->payload.size() == 1 &&
                    frame->payload[0] == current_chunk_) {
                    advanced = true;
                    chunks_acked_++;
                    current_chunk_++;
                    retransmit_count_ = 0;
                    if (current_chunk_ >= total_chunks_) {
                        state_ = State::WaitingComplete;
                    } else {
                        send_chunk(current_chunk_);
                    }
                    last_sent_at_ms_ = now_ms;
                }
                // else: stale/duplicate ack for an already-passed chunk -- ignore.
            }
            if (!advanced && now_ms - last_sent_at_ms_ >= ack_timeout_ms_) {
                if (++retransmit_count_ > max_retransmits_) {
                    status_ = TransferStatus::Failed;
                } else {
                    send_chunk(current_chunk_);
                    last_sent_at_ms_ = now_ms;
                }
            }
            break;
        }

        case State::WaitingComplete: {
            bool got_result = false;
            std::optional<Frame> frame;
            while ((frame = decoder_.try_extract()).has_value()) {
                if (frame->type == MessageType::Complete) {
                    status_ = TransferStatus::Done;
                    got_result = true;
                } else if (frame->type == MessageType::Error) {
                    status_ = TransferStatus::Failed;
                    got_result = true;
                }
            }
            if (!got_result && now_ms - last_sent_at_ms_ >= ack_timeout_ms_) {
                if (++retransmit_count_ > max_retransmits_) {
                    status_ = TransferStatus::Failed;
                } else {
                    // Re-send the last chunk as an implicit "are you done?"
                    // ping -- harmless if the receiver already has it (see
                    // AnimationReceiver::resend_outcome()).
                    send_chunk(total_chunks_ - 1);
                    last_sent_at_ms_ = now_ms;
                }
            }
            break;
        }
    }
}

// ---- AnimationReceiver ----

AnimationReceiver::AnimationReceiver(ITransport& transport, uint32_t ack_timeout_ms, int max_retransmits)
    : transport_(transport), ack_timeout_ms_(ack_timeout_ms), max_retransmits_(max_retransmits) {}

void AnimationReceiver::send_request() {
    Frame frame;
    frame.type = MessageType::Request;
    transport_.send(encode_frame(frame));
}

void AnimationReceiver::send_chunk_ack(uint8_t chunk_index) {
    Frame frame;
    frame.type = MessageType::ChunkAck;
    frame.payload = {chunk_index};
    transport_.send(encode_frame(frame));
}

void AnimationReceiver::finish_reassembly() {
    const std::string text(received_.begin(), received_.end());
    auto parsed = core::animation_from_text(text);
    finished_ = true;
    if (parsed.has_value()) {
        animation_ = std::move(parsed);
        succeeded_ = true;
        status_ = TransferStatus::Done;
    } else {
        succeeded_ = false;
        status_ = TransferStatus::Failed;
    }
    resend_outcome();
}

void AnimationReceiver::resend_outcome() {
    Frame frame;
    frame.type = succeeded_ ? MessageType::Complete : MessageType::Error;
    transport_.send(encode_frame(frame));
}

void AnimationReceiver::poll(uint32_t now_ms) {
    transport_.poll();
    std::vector<uint8_t> raw;
    transport_.receive(raw);
    decoder_.feed(raw);

    std::optional<Frame> frame;
    while ((frame = decoder_.try_extract()).has_value()) {
        switch (frame->type) {
            case MessageType::Announce: {
                if (frame->payload.size() != 3) {
                    break;
                }
                if (state_ == State::WaitingAnnounce) {
                    total_size_ = (static_cast<int>(frame->payload[0]) << 8) | frame->payload[1];
                    total_chunks_ = static_cast<int>(frame->payload[2]);
                    received_.assign(static_cast<size_t>(total_size_), 0);
                    chunk_received_flags_.assign(static_cast<size_t>(total_chunks_), false);
                    state_ = State::WaitingChunks;
                }
                // Always (re)send Request, even if already WaitingChunks --
                // covers the sender re-announcing because our first Request
                // was lost.
                send_request();
                last_request_sent_at_ms_ = now_ms;
                request_retransmit_count_ = 0;
                break;
            }
            case MessageType::Chunk: {
                if (state_ != State::WaitingChunks || frame->payload.size() < 2) {
                    break;
                }
                const uint8_t chunk_index = frame->payload[0];
                const uint8_t declared_total = frame->payload[1];
                if (total_chunks_ == 0 || declared_total != total_chunks_ || chunk_index >= total_chunks_) {
                    break;  // malformed or from a stale/mismatched session
                }
                if (!chunk_received_flags_[chunk_index]) {
                    const size_t data_len = frame->payload.size() - 2;
                    const size_t offset = static_cast<size_t>(chunk_index) * kChunkDataSize;
                    if (offset + data_len <= received_.size()) {
                        std::copy(frame->payload.begin() + 2, frame->payload.end(), received_.begin() + offset);
                    }
                    chunk_received_flags_[chunk_index] = true;
                    chunks_received_++;
                }
                send_chunk_ack(chunk_index);
                if (chunks_received_ == total_chunks_) {
                    if (!finished_) {
                        finish_reassembly();
                    } else {
                        resend_outcome();
                    }
                }
                break;
            }
            default:
                break;
        }
    }

    if (state_ == State::WaitingChunks && chunks_received_ == 0 &&
        now_ms - last_request_sent_at_ms_ >= ack_timeout_ms_) {
        if (++request_retransmit_count_ > max_retransmits_) {
            status_ = TransferStatus::Failed;
        } else {
            send_request();
            last_request_sent_at_ms_ = now_ms;
        }
    }
}

}  // namespace chainled::net
