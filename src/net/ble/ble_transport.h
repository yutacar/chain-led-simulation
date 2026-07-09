/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 *
 * BleTransport + BleScanner: BLE implementation of ITransport for Linux/
 * BlueZ (Phase 7, PLAN.md §2.3 "BleTransport" row). Adapted from
 * Battleship's src/net/ble/ble_transport.{h,cpp} (same sd-bus/NUS
 * architecture, same Host=peripheral/Join=central role split) -- see
 * [[battleship-cardputerzero-project]]. The one real difference: Battleship's
 * game messages are tiny (<=13 bytes) and fit one ATT write/notification at
 * the un-negotiated BLE 4.2 default MTU, so it never needed fragmentation.
 * ChainLED's animation Chunk frames are much larger (kChunkDataSize=200 in
 * animation_transfer.h, ~210 bytes on the wire) -- rather than depend on MTU
 * negotiation actually succeeding between two real devices (unverifiable
 * without hardware), BleTransport::send() splits every call into
 * kBleFragmentSize-byte pieces and issues one GATT write/notification per
 * piece. This needs no protocol changes above ITransport: fragment
 * boundaries carry no meaning, exactly like TCP's stream chunking -- the
 * receiving FrameDecoder resyncs on magic+CRC regardless of how bytes
 * arrived split. A dropped fragment just corrupts one Chunk frame, which the
 * existing Chunk/ChunkAck timeout+retry layer (already proven against
 * LoopbackTransport fault injection) resends whole.
 *
 * Design (Nordic UART Service compatible GATT, over the BlueZ D-Bus API via
 * sd-bus/libsystemd):
 *
 *   Host (SendScreen)    = GATT peripheral: registers a NUS-compatible
 *           application (service 6E400001-..., RX characteristic = write /
 *           write-without-response, TX characteristic = notify) with
 *           org.bluez.GattManager1 and advertises via
 *           org.bluez.LEAdvertisingManager1 with local name
 *           "CHAINLED-<host>".
 *   Join (ReceiveScreen) = GATT central: BleScanner runs
 *           Adapter1.StartDiscovery with a NUS-service-UUID + name-prefix
 *           filter and exposes the result list; BleTransport then
 *           Device1.Connect()s the chosen device, resolves the NUS
 *           characteristics, subscribes to TX notifications and writes
 *           outgoing bytes to RX.
 *
 * Security scope (deliberate, matching Battleship's Phase 6 decision): Just
 * Works connections only. No pairing/bonding requested, no link encryption
 * -- an animation is not sensitive data, so a sniffed link reveals nothing
 * of consequence.
 *
 * Platform scope: Linux/BlueZ only. This header is safe to include (and this
 * class safe to name) on any platform -- the implementation is pimpl'd --
 * but ble_transport.cpp is only compiled when the CMake option CHAINLED_BLE
 * is ON (Linux builds only; the macOS desktop build has no BlueZ and gets
 * CHAINLED_BLE=0, see CMakeLists.txt). UI code must guard construction with
 * `#if CHAINLED_BLE`.
 *
 * Threading: single-threaded poll-driven like every other ITransport. poll()
 * pumps the sd-bus connection non-blockingly (sd_bus_process with no wait);
 * all BlueZ method calls are issued asynchronously except a small number of
 * one-shot setup lookups performed inside start_host()/start_join()/
 * BleScanner::start() themselves (adapter enumeration).
 */
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "net/transport.h"

namespace chainled::net {

/// Nordic UART Service UUIDs (lowercase, as BlueZ reports them). Standard
/// NUS UUIDs, reused as-is -- no reason to diverge from a recognized
/// convention, and it lets any NUS-aware BLE tool talk to a host for
/// debugging.
inline constexpr const char* kNusServiceUuid = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
inline constexpr const char* kNusRxCharUuid = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";  ///< central -> peripheral (Write)
inline constexpr const char* kNusTxCharUuid = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";  ///< peripheral -> central (Notify)

/// Prefix of the advertised LE local name.
inline constexpr const char* kBleNamePrefix = "CHAINLED-";

/// Max payload bytes per single GATT WriteValue/notification (comfortably
/// under the BLE 4.0+ baseline ATT_MTU=23 -> 20 usable bytes after the
/// 3-byte ATT header, with margin -- deliberately not dependent on MTU
/// negotiation succeeding). BleTransport::send() fragments any longer call
/// into pieces this size; see file header for why no fragment header is
/// needed on the wire.
inline constexpr size_t kBleFragmentSize = 18;

/// One advertising ChainLED host as seen by a scanning joiner.
struct BleScanResult {
    std::string device_path;  ///< BlueZ D-Bus object path (org.bluez.Device1)
    std::string address;      ///< BD_ADDR "AA:BB:CC:DD:EE:FF" (diagnostics/UI)
    std::string name;         ///< advertised local name ("CHAINLED-<host>")
};

/// Join side: LE discovery with a NUS/name filter, poll-driven host list.
class BleScanner {
public:
    BleScanner();
    ~BleScanner();
    BleScanner(const BleScanner&) = delete;
    BleScanner& operator=(const BleScanner&) = delete;

    /// Connects to the system bus, powers the first BlueZ adapter and starts
    /// filtered LE discovery. Returns false if there is no usable adapter /
    /// bluetoothd (UI then shows "Bluetooth unavailable").
    bool start();

    /// Re-enumerates discovered devices at a modest interval and services
    /// D-Bus traffic. Call from the LVGL-side net poll timer.
    void poll(uint32_t now_ms);

    [[nodiscard]] const std::vector<BleScanResult>& results() const;

    /// Stops discovery and releases the bus connection. Idempotent.
    void stop();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/// BLE ITransport. One instance handles either role:
///   start_host()  -> peripheral (advertise + GATT server), state Connecting
///                    until a central connects AND subscribes to TX.
///   start_join()  -> central (connect + resolve + subscribe), state
///                    Connecting until the TX subscription is live.
class BleTransport final : public ITransport {
public:
    BleTransport();
    ~BleTransport() override;
    BleTransport(const BleTransport&) = delete;
    BleTransport& operator=(const BleTransport&) = delete;

    /// Host side: register the NUS GATT application and start advertising as
    /// "CHAINLED-<host_name>". Returns false on immediate setup failure (no
    /// adapter, no bluetoothd, D-Bus permission denied). On success state()
    /// == Connecting until a central subscribes to the TX characteristic.
    bool start_host(const std::string& host_name);

    /// Join side: connect to a device previously found by BleScanner.
    /// `device_path` is BleScanResult::device_path. Returns false on
    /// immediate failure; on success state() == Connecting until the NUS
    /// characteristics are resolved and TX notifications are subscribed.
    bool start_join(const std::string& device_path);

    void poll() override;
    [[nodiscard]] TransportState state() const override;
    using ITransport::send;  // keep the std::vector convenience overload visible
    bool send(const uint8_t* data, size_t len) override;
    size_t receive(std::vector<uint8_t>& out) override;
    void close() override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace chainled::net
