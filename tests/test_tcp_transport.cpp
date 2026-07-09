/**
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 *
 * Real localhost TCP sockets (not mocked) -- this is the desktop dev path
 * (PLAN.md §2.3) and its own headless in-process substitute for the
 * "2 real processes on Mac" verification a running app can't do inside a
 * unit test binary.
 */
#include "net/tcp_transport.h"

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

namespace chainled::net {
namespace {

template <typename Predicate>
bool pump_until(TcpTransport& a, TcpTransport& b, Predicate predicate, int max_iterations = 2000) {
    for (int i = 0; i < max_iterations; ++i) {
        a.poll();
        b.poll();
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return false;
}

TEST(TcpTransport, ConnectsOverLocalhost) {
    TcpTransport host;
    ASSERT_TRUE(host.listen(0));
    ASSERT_GT(host.local_port(), 0);

    TcpTransport client;
    ASSERT_TRUE(client.connect("127.0.0.1", host.local_port()));

    ASSERT_TRUE(pump_until(host, client, [&] {
        return host.state() == TransportState::Connected && client.state() == TransportState::Connected;
    }));
}

TEST(TcpTransport, TransfersDataBothWays) {
    TcpTransport host;
    ASSERT_TRUE(host.listen(0));
    TcpTransport client;
    ASSERT_TRUE(client.connect("127.0.0.1", host.local_port()));
    ASSERT_TRUE(pump_until(
        host, client, [&] { return host.state() == TransportState::Connected && client.state() == TransportState::Connected; }));

    const std::vector<uint8_t> to_client = {1, 2, 3};
    const std::vector<uint8_t> to_host = {9, 8};
    ASSERT_TRUE(host.send(to_client));
    ASSERT_TRUE(client.send(to_host));

    std::vector<uint8_t> client_received;
    std::vector<uint8_t> host_received;
    ASSERT_TRUE(pump_until(host, client, [&] {
        client.receive(client_received);
        host.receive(host_received);
        return client_received.size() == to_client.size() && host_received.size() == to_host.size();
    }));
    EXPECT_EQ(client_received, to_client);
    EXPECT_EQ(host_received, to_host);
}

TEST(TcpTransport, ConnectToNothingStaysDisconnected) {
    TcpTransport client;
    // A port nothing is listening on -- connect() itself may still return
    // true (non-blocking), but polling must never reach Connected.
    client.connect("127.0.0.1", 1);
    for (int i = 0; i < 50; ++i) {
        client.poll();
        ASSERT_NE(client.state(), TransportState::Connected);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

TEST(TcpTransport, ClosingPeerDisconnectsTheOtherSide) {
    TcpTransport host;
    ASSERT_TRUE(host.listen(0));
    TcpTransport client;
    ASSERT_TRUE(client.connect("127.0.0.1", host.local_port()));
    ASSERT_TRUE(pump_until(
        host, client, [&] { return host.state() == TransportState::Connected && client.state() == TransportState::Connected; }));

    client.close();
    ASSERT_TRUE(pump_until(host, client, [&] { return host.state() == TransportState::Disconnected; }));
}

}  // namespace
}  // namespace chainled::net
