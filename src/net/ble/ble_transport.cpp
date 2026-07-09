/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 *
 * Linux/BlueZ implementation of BleTransport/BleScanner over sd-bus
 * (libsystemd), adapted from Battleship's src/net/ble/ble_transport.cpp.
 * Only compiled when the CMake option CHAINLED_BLE is ON -- see
 * ble_transport.h for the design overview.
 *
 * sd-bus was chosen over GLib/GDBus for the same reasons Battleship chose
 * it: it ships with Raspberry Pi OS / Debian as part of libsystemd (already
 * present on the device for the init system, zero new runtime dependencies
 * for CardputerZero), has a plain C API without a GObject type system or a
 * GMainLoop requirement, and its explicit sd_bus_process() pump maps 1:1
 * onto the ITransport poll() model this codebase is built around.
 */
#include "net/ble/ble_transport.h"

#include <systemd/sd-bus.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace chainled::net {

namespace {

constexpr const char* kBluez = "org.bluez";
constexpr const char* kAdapterIface = "org.bluez.Adapter1";
constexpr const char* kDeviceIface = "org.bluez.Device1";
constexpr const char* kGattServiceIface = "org.bluez.GattService1";
constexpr const char* kGattCharIface = "org.bluez.GattCharacteristic1";
constexpr const char* kGattManagerIface = "org.bluez.GattManager1";
constexpr const char* kAdvManagerIface = "org.bluez.LEAdvertisingManager1";
constexpr const char* kAdvIface = "org.bluez.LEAdvertisement1";
constexpr const char* kPropsIface = "org.freedesktop.DBus.Properties";

// Our exported object hierarchy (host/peripheral role).
constexpr const char* kAppRoot = "/com/chainled/gatt";
constexpr const char* kServicePath = "/com/chainled/gatt/service0";
constexpr const char* kRxCharPath = "/com/chainled/gatt/service0/char_rx";
constexpr const char* kTxCharPath = "/com/chainled/gatt/service0/char_tx";
constexpr const char* kAdvPath = "/com/chainled/advertisement0";

/// Drains all pending D-Bus traffic without blocking. Every incoming method
/// call (WriteValue/StartNotify/...), async reply and signal is dispatched
/// from here -- i.e. from poll() or from the setup entry points.
void pump(sd_bus* bus) {
    if (!bus) return;
    while (sd_bus_process(bus, nullptr) > 0) {
    }
}

/// Blocking (setup-time only): finds the object path of the first
/// org.bluez.Adapter1 on the system bus. Empty string if none/bluetoothd
/// unreachable.
std::string find_adapter(sd_bus* bus) {
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message* reply = nullptr;
    std::string adapter;

    const int rc = sd_bus_call_method(bus, kBluez, "/", "org.freedesktop.DBus.ObjectManager",
                                      "GetManagedObjects", &error, &reply, "");
    if (rc < 0) {
        sd_bus_error_free(&error);
        return adapter;
    }

    // Signature: a{oa{sa{sv}}}
    if (sd_bus_message_enter_container(reply, 'a', "{oa{sa{sv}}}") > 0) {
        while (sd_bus_message_enter_container(reply, 'e', "oa{sa{sv}}") > 0) {
            const char* path = nullptr;
            sd_bus_message_read_basic(reply, 'o', &path);
            sd_bus_message_enter_container(reply, 'a', "{sa{sv}}");
            while (sd_bus_message_enter_container(reply, 'e', "sa{sv}") > 0) {
                const char* iface = nullptr;
                sd_bus_message_read_basic(reply, 's', &iface);
                if (adapter.empty() && iface && std::strcmp(iface, kAdapterIface) == 0 && path) {
                    adapter = path;
                }
                sd_bus_message_skip(reply, "a{sv}");
                sd_bus_message_exit_container(reply);
            }
            sd_bus_message_exit_container(reply);
            sd_bus_message_exit_container(reply);
        }
        sd_bus_message_exit_container(reply);
    }
    sd_bus_message_unref(reply);
    return adapter;
}

/// Blocking (setup-time only): best-effort Adapter1.Powered = true. A
/// failure is not fatal here -- if the adapter really is unusable the
/// subsequent register/discovery call fails and reports it.
void power_on_adapter(sd_bus* bus, const std::string& adapter) {
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_set_property(bus, kBluez, adapter.c_str(), kAdapterIface, "Powered", &error, "b", 1);
    sd_bus_error_free(&error);
}

/// Reads an `ay` byte array out of `m` into `out`. Returns >= 0 on success.
int read_byte_array(sd_bus_message* m, std::vector<uint8_t>& out) {
    const void* data = nullptr;
    size_t len = 0;
    const int rc = sd_bus_message_read_array(m, 'y', &data, &len);
    if (rc >= 0 && data && len > 0) {
        const auto* bytes = static_cast<const uint8_t*>(data);
        out.insert(out.end(), bytes, bytes + len);
    }
    return rc;
}

bool starts_with(const std::string& text, const char* prefix) {
    return text.rfind(prefix, 0) == 0;
}

}  // namespace

// ====================================================================
// BleScanner
// ====================================================================

struct BleScanner::Impl {
    sd_bus* bus = nullptr;
    std::string adapter;
    bool discovering = false;
    uint32_t last_enumerate_ms = 0;
    bool enumerate_in_flight = false;
    bool clock_initialized = false;
    std::vector<BleScanResult> results;

    ~Impl() { shutdown(); }

    void shutdown() {
        if (bus) {
            if (discovering) {
                // Best-effort; fire the call and flush it out before closing.
                sd_bus_call_method_async(bus, nullptr, kBluez, adapter.c_str(), kAdapterIface,
                                         "StopDiscovery", nullptr, nullptr, "");
                sd_bus_flush(bus);
            }
            sd_bus_unref(bus);  // floating slots (matches/async replies) die with the bus
            bus = nullptr;
        }
        discovering = false;
        enumerate_in_flight = false;
        results.clear();
    }

    static int enumerate_reply_cb(sd_bus_message* reply, void* userdata, sd_bus_error*) {
        auto* self = static_cast<Impl*>(userdata);
        self->enumerate_in_flight = false;
        if (sd_bus_message_is_method_error(reply, nullptr)) {
            return 0;  // transient; retried on the next interval
        }

        std::vector<BleScanResult> found;
        // Signature: a{oa{sa{sv}}}
        if (sd_bus_message_enter_container(reply, 'a', "{oa{sa{sv}}}") <= 0) {
            return 0;
        }
        while (sd_bus_message_enter_container(reply, 'e', "oa{sa{sv}}") > 0) {
            const char* path = nullptr;
            sd_bus_message_read_basic(reply, 'o', &path);

            std::string name;
            std::string address;
            bool is_device = false;
            bool has_nus_uuid = false;

            sd_bus_message_enter_container(reply, 'a', "{sa{sv}}");
            while (sd_bus_message_enter_container(reply, 'e', "sa{sv}") > 0) {
                const char* iface = nullptr;
                sd_bus_message_read_basic(reply, 's', &iface);
                if (iface && std::strcmp(iface, kDeviceIface) == 0) {
                    is_device = true;
                    sd_bus_message_enter_container(reply, 'a', "{sv}");
                    while (sd_bus_message_enter_container(reply, 'e', "sv") > 0) {
                        const char* key = nullptr;
                        sd_bus_message_read_basic(reply, 's', &key);
                        if (key && std::strcmp(key, "Name") == 0) {
                            const char* value = nullptr;
                            if (sd_bus_message_enter_container(reply, 'v', "s") > 0) {
                                sd_bus_message_read_basic(reply, 's', &value);
                                sd_bus_message_exit_container(reply);
                                if (value) name = value;
                            } else {
                                sd_bus_message_skip(reply, "v");
                            }
                        } else if (key && std::strcmp(key, "Address") == 0) {
                            const char* value = nullptr;
                            if (sd_bus_message_enter_container(reply, 'v', "s") > 0) {
                                sd_bus_message_read_basic(reply, 's', &value);
                                sd_bus_message_exit_container(reply);
                                if (value) address = value;
                            } else {
                                sd_bus_message_skip(reply, "v");
                            }
                        } else if (key && std::strcmp(key, "UUIDs") == 0) {
                            if (sd_bus_message_enter_container(reply, 'v', "as") > 0) {
                                sd_bus_message_enter_container(reply, 'a', "s");
                                const char* uuid = nullptr;
                                while (sd_bus_message_read_basic(reply, 's', &uuid) > 0) {
                                    if (uuid && std::strcmp(uuid, kNusServiceUuid) == 0) {
                                        has_nus_uuid = true;
                                    }
                                }
                                sd_bus_message_exit_container(reply);
                                sd_bus_message_exit_container(reply);
                            } else {
                                sd_bus_message_skip(reply, "v");
                            }
                        } else {
                            sd_bus_message_skip(reply, "v");
                        }
                        sd_bus_message_exit_container(reply);
                    }
                    sd_bus_message_exit_container(reply);
                } else {
                    sd_bus_message_skip(reply, "a{sv}");
                }
                sd_bus_message_exit_container(reply);
            }
            sd_bus_message_exit_container(reply);
            sd_bus_message_exit_container(reply);

            // A ChainLED host is recognized by advertised name prefix OR by
            // the NUS service UUID (the 31-byte ADV payload cannot always
            // carry both the 128-bit UUID and the full name, so either
            // signal alone must suffice).
            if (is_device && path && (starts_with(name, kBleNamePrefix) || has_nus_uuid)) {
                BleScanResult result;
                result.device_path = path;
                result.address = address;
                result.name = name.empty() ? address : name;
                found.push_back(std::move(result));
            }
        }
        sd_bus_message_exit_container(reply);

        self->results = std::move(found);
        return 0;
    }
};

BleScanner::BleScanner() : impl_(new Impl) {}
BleScanner::~BleScanner() = default;

bool BleScanner::start() {
    stop();
    if (sd_bus_open_system(&impl_->bus) < 0) {
        impl_->bus = nullptr;
        return false;
    }
    impl_->adapter = find_adapter(impl_->bus);
    if (impl_->adapter.empty()) {
        impl_->shutdown();
        return false;
    }
    power_on_adapter(impl_->bus, impl_->adapter);

    // LE-only discovery filter. Duplicates are fine (we re-enumerate the
    // device list rather than consuming per-advert events); filtering to LE
    // just keeps BR/EDR neighbours out of the cache.
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_call_method(impl_->bus, kBluez, impl_->adapter.c_str(), kAdapterIface,
                       "SetDiscoveryFilter", &error, nullptr, "a{sv}", 1, "Transport", "s", "le");
    sd_bus_error_free(&error);

    sd_bus_error start_error = SD_BUS_ERROR_NULL;
    const int rc = sd_bus_call_method(impl_->bus, kBluez, impl_->adapter.c_str(), kAdapterIface,
                                      "StartDiscovery", &start_error, nullptr, "");
    // "InProgress" (another app already scanning) still surfaces devices via
    // the shared BlueZ cache, so treat it as success.
    const bool in_progress =
        sd_bus_error_has_name(&start_error, "org.bluez.Error.InProgress") != 0;
    sd_bus_error_free(&start_error);
    if (rc < 0 && !in_progress) {
        impl_->shutdown();
        return false;
    }
    impl_->discovering = true;
    return true;
}

void BleScanner::poll(uint32_t now_ms) {
    if (!impl_->bus || !impl_->discovering) {
        return;
    }
    pump(impl_->bus);

    if (!impl_->clock_initialized) {
        impl_->clock_initialized = true;
        impl_->last_enumerate_ms = now_ms - 1000;  // enumerate immediately on first poll
    }
    if (!impl_->enumerate_in_flight && now_ms - impl_->last_enumerate_ms >= 1000) {
        impl_->last_enumerate_ms = now_ms;
        const int rc = sd_bus_call_method_async(
            impl_->bus, nullptr, kBluez, "/", "org.freedesktop.DBus.ObjectManager",
            "GetManagedObjects", &Impl::enumerate_reply_cb, impl_.get(), "");
        impl_->enumerate_in_flight = rc >= 0;
    }
    pump(impl_->bus);
}

const std::vector<BleScanResult>& BleScanner::results() const {
    return impl_->results;
}

void BleScanner::stop() {
    impl_->shutdown();
}

// ====================================================================
// BleTransport
// ====================================================================

struct BleTransport::Impl {
    enum class Role { None, Host, Join };

    sd_bus* bus = nullptr;
    std::string adapter;
    Role role = Role::None;
    TransportState state = TransportState::Disconnected;

    // ---- host (peripheral) ----
    std::string local_name;              ///< advertised "CHAINLED-<host>"
    std::vector<uint8_t> tx_value;       ///< payload of the notification being emitted
    bool notify_enabled = false;         ///< central subscribed to TX
    bool app_registered = false;
    bool adv_registered = false;

    // ---- join (central) ----
    std::string device_path;
    std::string rx_char_path;            ///< peer's RX char: we WriteValue here
    std::string tx_char_path;            ///< peer's TX char: we get notified here
    bool services_resolved = false;
    bool resolve_query_in_flight = false;
    bool char_query_in_flight = false;
    bool start_notify_sent = false;

    std::vector<uint8_t> recv_buffer;

    ~Impl() { shutdown(); }

    void fail() {
        state = TransportState::Disconnected;
    }

    void shutdown() {
        if (bus) {
            if (role == Role::Join && !device_path.empty()) {
                sd_bus_call_method_async(bus, nullptr, kBluez, device_path.c_str(), kDeviceIface,
                                         "Disconnect", nullptr, nullptr, "");
            }
            if (role == Role::Host && !adapter.empty()) {
                if (adv_registered) {
                    sd_bus_call_method_async(bus, nullptr, kBluez, adapter.c_str(),
                                             kAdvManagerIface, "UnregisterAdvertisement", nullptr,
                                             nullptr, "o", kAdvPath);
                }
                if (app_registered) {
                    sd_bus_call_method_async(bus, nullptr, kBluez, adapter.c_str(),
                                             kGattManagerIface, "UnregisterApplication", nullptr,
                                             nullptr, "o", kAppRoot);
                }
            }
            sd_bus_flush(bus);
            sd_bus_unref(bus);  // floating slots (vtables/matches/async) die with the bus
            bus = nullptr;
        }
        role = Role::None;
        state = TransportState::Disconnected;
        notify_enabled = false;
        app_registered = false;
        adv_registered = false;
        services_resolved = false;
        resolve_query_in_flight = false;
        char_query_in_flight = false;
        start_notify_sent = false;
        device_path.clear();
        rx_char_path.clear();
        tx_char_path.clear();
        tx_value.clear();
        // recv_buffer intentionally kept: bytes already received stay
        // drainable after close(), mirroring TcpTransport::close().
    }

    // ---------------- host: GATT server vtable handlers ----------------

    static int prop_string_cb(sd_bus_message* reply, const char* value) {
        return sd_bus_message_append(reply, "s", value);
    }

    static int service_uuid_cb(sd_bus*, const char*, const char*, const char*,
                               sd_bus_message* reply, void*, sd_bus_error*) {
        return prop_string_cb(reply, kNusServiceUuid);
    }

    static int service_primary_cb(sd_bus*, const char*, const char*, const char*,
                                  sd_bus_message* reply, void*, sd_bus_error*) {
        return sd_bus_message_append(reply, "b", 1);
    }

    static int rx_uuid_cb(sd_bus*, const char*, const char*, const char*, sd_bus_message* reply,
                          void*, sd_bus_error*) {
        return prop_string_cb(reply, kNusRxCharUuid);
    }

    static int tx_uuid_cb(sd_bus*, const char*, const char*, const char*, sd_bus_message* reply,
                          void*, sd_bus_error*) {
        return prop_string_cb(reply, kNusTxCharUuid);
    }

    static int char_service_cb(sd_bus*, const char*, const char*, const char*,
                               sd_bus_message* reply, void*, sd_bus_error*) {
        return sd_bus_message_append(reply, "o", kServicePath);
    }

    static int rx_flags_cb(sd_bus*, const char*, const char*, const char*, sd_bus_message* reply,
                           void*, sd_bus_error*) {
        // Both write shapes: write-without-response ("command") avoids a
        // round trip per fragment; plain write is the interoperable
        // fallback.
        const char* flags[] = {"write", "write-without-response", nullptr};
        return sd_bus_message_append_strv(reply, const_cast<char**>(flags));
    }

    static int tx_flags_cb(sd_bus*, const char*, const char*, const char*, sd_bus_message* reply,
                           void*, sd_bus_error*) {
        const char* flags[] = {"notify", nullptr};
        return sd_bus_message_append_strv(reply, const_cast<char**>(flags));
    }

    static int tx_value_cb(sd_bus*, const char*, const char*, const char*, sd_bus_message* reply,
                           void* userdata, sd_bus_error*) {
        auto* self = static_cast<Impl*>(userdata);
        return sd_bus_message_append_array(reply, 'y', self->tx_value.data(),
                                           self->tx_value.size());
    }

    static int tx_notifying_cb(sd_bus*, const char*, const char*, const char*,
                               sd_bus_message* reply, void* userdata, sd_bus_error*) {
        auto* self = static_cast<Impl*>(userdata);
        return sd_bus_message_append(reply, "b", self->notify_enabled ? 1 : 0);
    }

    /// Central wrote to our RX characteristic: one fragment of a frame (or a
    /// whole small frame -- see kBleFragmentSize). Order-preserving append,
    /// same as TcpTransport's recv buffer; FrameDecoder above resyncs
    /// regardless of how the bytes were split.
    static int rx_write_value_cb(sd_bus_message* m, void* userdata, sd_bus_error*) {
        auto* self = static_cast<Impl*>(userdata);
        read_byte_array(m, self->recv_buffer);
        // (options dict ignored; trailing args need no explicit skip)
        return sd_bus_reply_method_return(m, "");
    }

    static int tx_start_notify_cb(sd_bus_message* m, void* userdata, sd_bus_error*) {
        auto* self = static_cast<Impl*>(userdata);
        self->notify_enabled = true;
        if (self->state == TransportState::Connecting) {
            self->state = TransportState::Connected;
        }
        return sd_bus_reply_method_return(m, "");
    }

    /// BlueZ calls StopNotify when the subscribed central unsubscribes or
    /// disconnects -- for a 1:1 transfer either means the peer is gone.
    static int tx_stop_notify_cb(sd_bus_message* m, void* userdata, sd_bus_error*) {
        auto* self = static_cast<Impl*>(userdata);
        self->notify_enabled = false;
        if (self->state == TransportState::Connected) {
            self->fail();
        }
        return sd_bus_reply_method_return(m, "");
    }

    // ---------------- host: advertisement vtable handlers ----------------

    static int adv_type_cb(sd_bus*, const char*, const char*, const char*, sd_bus_message* reply,
                           void*, sd_bus_error*) {
        return prop_string_cb(reply, "peripheral");
    }

    static int adv_local_name_cb(sd_bus*, const char*, const char*, const char*,
                                 sd_bus_message* reply, void* userdata, sd_bus_error*) {
        auto* self = static_cast<Impl*>(userdata);
        return prop_string_cb(reply, self->local_name.c_str());
    }

    static int adv_service_uuids_cb(sd_bus*, const char*, const char*, const char*,
                                    sd_bus_message* reply, void*, sd_bus_error*) {
        const char* uuids[] = {kNusServiceUuid, nullptr};
        return sd_bus_message_append_strv(reply, const_cast<char**>(uuids));
    }

    static int adv_release_cb(sd_bus_message* m, void*, sd_bus_error*) {
        return sd_bus_reply_method_return(m, "");
    }

    // ---------------- host: registration replies ----------------

    static int register_app_reply_cb(sd_bus_message* reply, void* userdata, sd_bus_error*) {
        auto* self = static_cast<Impl*>(userdata);
        if (sd_bus_message_is_method_error(reply, nullptr)) {
            self->fail();
        } else {
            self->app_registered = true;
        }
        return 0;
    }

    static int register_adv_reply_cb(sd_bus_message* reply, void* userdata, sd_bus_error*) {
        auto* self = static_cast<Impl*>(userdata);
        if (sd_bus_message_is_method_error(reply, nullptr)) {
            self->fail();
        } else {
            self->adv_registered = true;
        }
        return 0;
    }

    // ---------------- join: central-side callbacks ----------------

    static int connect_reply_cb(sd_bus_message* reply, void* userdata, sd_bus_error*) {
        auto* self = static_cast<Impl*>(userdata);
        if (sd_bus_message_is_method_error(reply, nullptr)) {
            self->fail();
        }
        // Success: keep waiting for ServicesResolved (polled from poll()).
        return 0;
    }

    static int resolved_reply_cb(sd_bus_message* reply, void* userdata, sd_bus_error*) {
        auto* self = static_cast<Impl*>(userdata);
        self->resolve_query_in_flight = false;
        if (sd_bus_message_is_method_error(reply, nullptr)) {
            return 0;  // device object may not be ready yet; retried
        }
        int resolved = 0;
        if (sd_bus_message_enter_container(reply, 'v', "b") > 0) {
            sd_bus_message_read_basic(reply, 'b', &resolved);
            sd_bus_message_exit_container(reply);
        }
        if (resolved) {
            self->services_resolved = true;
        }
        return 0;
    }

    /// GetManagedObjects reply while joining: locate the NUS RX/TX
    /// characteristic objects beneath our device path.
    static int char_lookup_reply_cb(sd_bus_message* reply, void* userdata, sd_bus_error*) {
        auto* self = static_cast<Impl*>(userdata);
        self->char_query_in_flight = false;
        if (sd_bus_message_is_method_error(reply, nullptr)) {
            return 0;
        }
        if (sd_bus_message_enter_container(reply, 'a', "{oa{sa{sv}}}") <= 0) {
            return 0;
        }
        while (sd_bus_message_enter_container(reply, 'e', "oa{sa{sv}}") > 0) {
            const char* path = nullptr;
            sd_bus_message_read_basic(reply, 'o', &path);
            const bool under_device = path && starts_with(path, self->device_path.c_str());

            sd_bus_message_enter_container(reply, 'a', "{sa{sv}}");
            while (sd_bus_message_enter_container(reply, 'e', "sa{sv}") > 0) {
                const char* iface = nullptr;
                sd_bus_message_read_basic(reply, 's', &iface);
                if (under_device && iface && std::strcmp(iface, kGattCharIface) == 0) {
                    sd_bus_message_enter_container(reply, 'a', "{sv}");
                    while (sd_bus_message_enter_container(reply, 'e', "sv") > 0) {
                        const char* key = nullptr;
                        sd_bus_message_read_basic(reply, 's', &key);
                        if (key && std::strcmp(key, "UUID") == 0) {
                            const char* uuid = nullptr;
                            if (sd_bus_message_enter_container(reply, 'v', "s") > 0) {
                                sd_bus_message_read_basic(reply, 's', &uuid);
                                sd_bus_message_exit_container(reply);
                            }
                            if (uuid && std::strcmp(uuid, kNusRxCharUuid) == 0) {
                                self->rx_char_path = path;
                            } else if (uuid && std::strcmp(uuid, kNusTxCharUuid) == 0) {
                                self->tx_char_path = path;
                            }
                        } else {
                            sd_bus_message_skip(reply, "v");
                        }
                        sd_bus_message_exit_container(reply);
                    }
                    sd_bus_message_exit_container(reply);
                } else {
                    sd_bus_message_skip(reply, "a{sv}");
                }
                sd_bus_message_exit_container(reply);
            }
            sd_bus_message_exit_container(reply);
            sd_bus_message_exit_container(reply);
        }
        sd_bus_message_exit_container(reply);
        return 0;
    }

    /// PropertiesChanged on the peer's TX characteristic: incoming fragment
    /// bytes ride in the changed "Value" property.
    static int tx_props_changed_cb(sd_bus_message* m, void* userdata, sd_bus_error*) {
        auto* self = static_cast<Impl*>(userdata);
        const char* iface = nullptr;
        if (sd_bus_message_read_basic(m, 's', &iface) < 0 || !iface ||
            std::strcmp(iface, kGattCharIface) != 0) {
            return 0;
        }
        if (sd_bus_message_enter_container(m, 'a', "{sv}") <= 0) {
            return 0;
        }
        while (sd_bus_message_enter_container(m, 'e', "sv") > 0) {
            const char* key = nullptr;
            sd_bus_message_read_basic(m, 's', &key);
            if (key && std::strcmp(key, "Value") == 0) {
                if (sd_bus_message_enter_container(m, 'v', "ay") > 0) {
                    read_byte_array(m, self->recv_buffer);
                    sd_bus_message_exit_container(m);
                } else {
                    sd_bus_message_skip(m, "v");
                }
            } else {
                sd_bus_message_skip(m, "v");
            }
            sd_bus_message_exit_container(m);
        }
        sd_bus_message_exit_container(m);
        return 0;
    }

    static int start_notify_reply_cb(sd_bus_message* reply, void* userdata, sd_bus_error*) {
        auto* self = static_cast<Impl*>(userdata);
        if (sd_bus_message_is_method_error(reply, nullptr)) {
            self->fail();
            return 0;
        }
        if (self->state == TransportState::Connecting) {
            self->state = TransportState::Connected;
        }
        return 0;
    }

    static int write_reply_cb(sd_bus_message* reply, void* userdata, sd_bus_error*) {
        auto* self = static_cast<Impl*>(userdata);
        if (sd_bus_message_is_method_error(reply, nullptr)) {
            // A failed write means the link is gone (device disconnected).
            // The reliability layer would retransmit forever otherwise.
            self->fail();
        }
        return 0;
    }

    // sd-bus vtables for the objects we export in the host role. Static
    // members (not namespace-scope globals) because their entries name the
    // private handler functions above; out-of-class member definitions
    // retain class access.
    static const sd_bus_vtable kServiceVtable[];
    static const sd_bus_vtable kRxCharVtable[];
    static const sd_bus_vtable kTxCharVtable[];
    static const sd_bus_vtable kAdvVtable[];

    /// Emits one fragment (<= kBleFragmentSize bytes) as the peripheral,
    /// via a Value property-change signal on the TX characteristic.
    bool send_host_fragment(const uint8_t* data, size_t len) {
        tx_value.assign(data, data + len);
        const int rc = sd_bus_emit_properties_changed(bus, kTxCharPath, kGattCharIface, "Value",
                                                       nullptr);
        pump(bus);
        return rc >= 0 && state == TransportState::Connected;
    }

    /// Writes one fragment (<= kBleFragmentSize bytes) as the central, via
    /// WriteValue on the peer's RX characteristic.
    bool send_join_fragment(const uint8_t* data, size_t len) {
        sd_bus_message* call = nullptr;
        int rc = sd_bus_message_new_method_call(bus, &call, kBluez, rx_char_path.c_str(),
                                                 kGattCharIface, "WriteValue");
        if (rc >= 0) rc = sd_bus_message_append_array(call, 'y', data, len);
        if (rc >= 0) rc = sd_bus_message_append(call, "a{sv}", 1, "type", "s", "command");
        if (rc >= 0) rc = sd_bus_call_async(bus, nullptr, call, &Impl::write_reply_cb, this, 0);
        sd_bus_message_unref(call);
        pump(bus);
        return rc >= 0 && state == TransportState::Connected;
    }
};

// GattService1 on kServicePath.
const sd_bus_vtable BleTransport::Impl::kServiceVtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("UUID", "s", service_uuid_cb, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("Primary", "b", service_primary_cb, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_VTABLE_END,
};

// GattCharacteristic1 on kRxCharPath (central writes -> our receive path).
const sd_bus_vtable BleTransport::Impl::kRxCharVtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("UUID", "s", rx_uuid_cb, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("Service", "o", char_service_cb, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("Flags", "as", rx_flags_cb, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_METHOD("WriteValue", "aya{sv}", "", rx_write_value_cb, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END,
};

// GattCharacteristic1 on kTxCharPath (our notifications -> central).
const sd_bus_vtable BleTransport::Impl::kTxCharVtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("UUID", "s", tx_uuid_cb, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("Service", "o", char_service_cb, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("Flags", "as", tx_flags_cb, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("Notifying", "b", tx_notifying_cb, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("Value", "ay", tx_value_cb, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_METHOD("StartNotify", "", "", tx_start_notify_cb, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("StopNotify", "", "", tx_stop_notify_cb, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END,
};

// LEAdvertisement1 on kAdvPath.
const sd_bus_vtable BleTransport::Impl::kAdvVtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("Type", "s", adv_type_cb, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("LocalName", "s", adv_local_name_cb, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("ServiceUUIDs", "as", adv_service_uuids_cb, 0,
                    SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_METHOD("Release", "", "", adv_release_cb, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END,
};

BleTransport::BleTransport() : impl_(new Impl) {}

BleTransport::~BleTransport() {
    close();
}

bool BleTransport::start_host(const std::string& host_name) {
    close();
    if (sd_bus_open_system(&impl_->bus) < 0) {
        impl_->bus = nullptr;
        return false;
    }
    impl_->adapter = find_adapter(impl_->bus);
    if (impl_->adapter.empty()) {
        impl_->shutdown();
        return false;
    }
    power_on_adapter(impl_->bus, impl_->adapter);

    impl_->role = Impl::Role::Host;
    impl_->local_name = std::string(kBleNamePrefix) + host_name;

    // Export the NUS application (ObjectManager root + service + 2 chars)
    // and the advertisement. Slots are left floating: they live exactly as
    // long as the bus connection (released together in shutdown()).
    int rc = sd_bus_add_object_manager(impl_->bus, nullptr, kAppRoot);
    if (rc >= 0) {
        rc = sd_bus_add_object_vtable(impl_->bus, nullptr, kServicePath, kGattServiceIface,
                                      Impl::kServiceVtable, impl_.get());
    }
    if (rc >= 0) {
        rc = sd_bus_add_object_vtable(impl_->bus, nullptr, kRxCharPath, kGattCharIface,
                                      Impl::kRxCharVtable, impl_.get());
    }
    if (rc >= 0) {
        rc = sd_bus_add_object_vtable(impl_->bus, nullptr, kTxCharPath, kGattCharIface,
                                      Impl::kTxCharVtable, impl_.get());
    }
    if (rc >= 0) {
        rc = sd_bus_add_object_vtable(impl_->bus, nullptr, kAdvPath, kAdvIface, Impl::kAdvVtable,
                                      impl_.get());
    }
    if (rc < 0) {
        impl_->shutdown();
        return false;
    }

    rc = sd_bus_call_method_async(impl_->bus, nullptr, kBluez, impl_->adapter.c_str(),
                                  kGattManagerIface, "RegisterApplication",
                                  &Impl::register_app_reply_cb, impl_.get(), "oa{sv}", kAppRoot,
                                  0);
    if (rc >= 0) {
        rc = sd_bus_call_method_async(impl_->bus, nullptr, kBluez, impl_->adapter.c_str(),
                                      kAdvManagerIface, "RegisterAdvertisement",
                                      &Impl::register_adv_reply_cb, impl_.get(), "oa{sv}",
                                      kAdvPath, 0);
    }
    if (rc < 0) {
        impl_->shutdown();
        return false;
    }

    impl_->state = TransportState::Connecting;
    pump(impl_->bus);
    return true;
}

bool BleTransport::start_join(const std::string& device_path) {
    close();
    if (device_path.empty()) {
        return false;
    }
    if (sd_bus_open_system(&impl_->bus) < 0) {
        impl_->bus = nullptr;
        return false;
    }
    impl_->adapter = find_adapter(impl_->bus);
    if (impl_->adapter.empty()) {
        impl_->shutdown();
        return false;
    }

    impl_->role = Impl::Role::Join;
    impl_->device_path = device_path;

    const int rc =
        sd_bus_call_method_async(impl_->bus, nullptr, kBluez, device_path.c_str(), kDeviceIface,
                                 "Connect", &Impl::connect_reply_cb, impl_.get(), "");
    if (rc < 0) {
        impl_->shutdown();
        return false;
    }

    impl_->state = TransportState::Connecting;
    pump(impl_->bus);
    return true;
}

void BleTransport::poll() {
    auto& impl = *impl_;
    if (!impl.bus || impl.state == TransportState::Disconnected) {
        return;
    }
    pump(impl.bus);

    if (impl.role == Impl::Role::Join && impl.state == TransportState::Connecting) {
        // Drive the connect pipeline: ServicesResolved -> characteristic
        // lookup -> notification subscription. Each stage is async; the
        // in-flight flags keep it at one outstanding query per stage.
        if (!impl.services_resolved) {
            if (!impl.resolve_query_in_flight) {
                impl.resolve_query_in_flight =
                    sd_bus_call_method_async(impl.bus, nullptr, kBluez, impl.device_path.c_str(),
                                             kPropsIface, "Get", &Impl::resolved_reply_cb,
                                             impl_.get(), "ss", kDeviceIface,
                                             "ServicesResolved") >= 0;
            }
        } else if (impl.rx_char_path.empty() || impl.tx_char_path.empty()) {
            if (!impl.char_query_in_flight) {
                impl.char_query_in_flight =
                    sd_bus_call_method_async(impl.bus, nullptr, kBluez, "/",
                                             "org.freedesktop.DBus.ObjectManager",
                                             "GetManagedObjects", &Impl::char_lookup_reply_cb,
                                             impl_.get(), "") >= 0;
            }
        } else if (!impl.start_notify_sent) {
            impl.start_notify_sent = true;
            // Incoming fragments arrive as PropertiesChanged(Value) on the
            // TX characteristic -- subscribe before enabling notifications
            // so no early notification can slip past the match.
            sd_bus_match_signal(impl.bus, nullptr, kBluez, impl.tx_char_path.c_str(), kPropsIface,
                                "PropertiesChanged", &Impl::tx_props_changed_cb, impl_.get());
            sd_bus_call_method_async(impl.bus, nullptr, kBluez, impl.tx_char_path.c_str(),
                                     kGattCharIface, "StartNotify",
                                     &Impl::start_notify_reply_cb, impl_.get(), "");
        }
    }

    pump(impl.bus);
}

TransportState BleTransport::state() const {
    return impl_->state;
}

bool BleTransport::send(const uint8_t* data, size_t len) {
    auto& impl = *impl_;
    if (impl.state != TransportState::Connected || !data || len == 0) {
        return false;
    }

    // Split into <= kBleFragmentSize pieces, one GATT write/notification
    // each. No fragment header is needed on the wire: ATT delivery is
    // strictly ordered on a single connection, and FrameDecoder above
    // resyncs on magic+CRC regardless of how the byte stream was chunked
    // (same as TCP). See ble_transport.h for the full reasoning.
    size_t offset = 0;
    while (offset < len) {
        const size_t piece_len = std::min(kBleFragmentSize, len - offset);
        const bool ok = (impl.role == Impl::Role::Host)
                            ? impl.send_host_fragment(data + offset, piece_len)
                            : impl.send_join_fragment(data + offset, piece_len);
        if (!ok) {
            return false;
        }
        offset += piece_len;
    }
    return true;
}

size_t BleTransport::receive(std::vector<uint8_t>& out) {
    auto& buffer = impl_->recv_buffer;
    const size_t n = buffer.size();
    out.insert(out.end(), buffer.begin(), buffer.end());
    buffer.clear();
    return n;
}

void BleTransport::close() {
    impl_->shutdown();
}

}  // namespace chainled::net
