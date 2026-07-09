/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 *
 * AnimationSender / AnimationReceiver: the chunked animation-distribution
 * protocol (PLAN.md §2.3) built directly on ITransport. Every message type
 * (Announce/Request/Chunk/ChunkAck/Complete/Error) is individually subject
 * to loss under LoopbackTransport fault injection, so each phase transition
 * is a bounded stop-and-wait retry, not just the per-chunk transfer:
 *
 *   sender --Announce(size,chunks)--> receiver
 *   sender <--Request-- receiver
 *   sender --Chunk(i)--> receiver, sender <--ChunkAck(i)-- receiver   (repeat per chunk)
 *   sender <--Complete or Error-- receiver                            (once fully reassembled+parsed)
 *
 * If Complete/Error is lost, the sender's periodic re-send of the *last*
 * chunk (harmless -- the receiver just re-ACKs an already-received chunk)
 * doubles as an implicit "are you done?" ping that makes the receiver
 * re-send Complete/Error too, so a single dropped Complete doesn't strand
 * an otherwise-successful transfer.
 */
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "frame.h"
#include "transport.h"

#include "core/animation.h"

namespace chainled::net {

/// Bytes of animation data per CHUNK frame (well under frame.h's 255-byte
/// payload cap once the 2-byte chunk header is subtracted).
constexpr size_t kChunkDataSize = 200;
constexpr uint32_t kDefaultAckTimeoutMs = 400;
constexpr int kDefaultMaxRetransmits = 5;

/// TCP port used by the Send/Receive screens' desktop/LAN dev path (BLE is
/// the real-device default, added in Phase 7). Receive always dials
/// 127.0.0.1 -- LAN host discovery/manual IP entry is out of scope for the
/// TCP dev path and isn't needed for the 2-processes-on-one-Mac
/// verification this phase targets.
constexpr uint16_t kDevTransferPort = 35455;

enum class TransferStatus { InProgress, Done, Failed };

class AnimationSender {
public:
    AnimationSender(ITransport& transport, std::string serialized_animation,
                     uint32_t ack_timeout_ms = kDefaultAckTimeoutMs, int max_retransmits = kDefaultMaxRetransmits);

    /// `now_ms` is an injectable clock so tests can drive retransmit timing
    /// deterministically without real sleeps.
    void poll(uint32_t now_ms);

    [[nodiscard]] TransferStatus status() const { return status_; }
    [[nodiscard]] int chunks_acked() const { return chunks_acked_; }
    [[nodiscard]] int total_chunks() const { return total_chunks_; }

private:
    enum class State { WaitingConnection, SentAnnounce, WaitingAck, WaitingComplete };

    void send_announce();
    void send_chunk(int index);

    ITransport& transport_;
    std::vector<uint8_t> data_;
    int total_chunks_ = 0;
    int current_chunk_ = 0;
    int chunks_acked_ = 0;
    int retransmit_count_ = 0;
    uint32_t last_sent_at_ms_ = 0;
    uint32_t ack_timeout_ms_;
    int max_retransmits_;
    State state_ = State::WaitingConnection;
    TransferStatus status_ = TransferStatus::InProgress;
    FrameDecoder decoder_;
};

class AnimationReceiver {
public:
    explicit AnimationReceiver(ITransport& transport, uint32_t ack_timeout_ms = kDefaultAckTimeoutMs,
                                int max_retransmits = kDefaultMaxRetransmits);

    void poll(uint32_t now_ms);

    [[nodiscard]] TransferStatus status() const { return status_; }
    [[nodiscard]] int chunks_received() const { return chunks_received_; }
    [[nodiscard]] int total_chunks() const { return total_chunks_; }
    /// Populated once status() == Done.
    [[nodiscard]] const std::optional<core::Animation>& animation() const { return animation_; }

private:
    enum class State { WaitingAnnounce, WaitingChunks };

    void send_request();
    void send_chunk_ack(uint8_t chunk_index);
    void finish_reassembly();
    void resend_outcome();

    ITransport& transport_;
    uint32_t ack_timeout_ms_;
    int max_retransmits_;
    State state_ = State::WaitingAnnounce;
    TransferStatus status_ = TransferStatus::InProgress;
    FrameDecoder decoder_;

    int total_size_ = 0;
    int total_chunks_ = 0;
    int chunks_received_ = 0;
    bool finished_ = false;
    bool succeeded_ = false;
    std::vector<uint8_t> received_;
    std::vector<bool> chunk_received_flags_;
    std::optional<core::Animation> animation_;

    uint32_t last_request_sent_at_ms_ = 0;
    int request_retransmit_count_ = 0;
};

}  // namespace chainled::net
