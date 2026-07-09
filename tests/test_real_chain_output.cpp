/**
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 *
 * RealChainOutput's actual serial I/O can only be verified against real
 * Chain Mono/RGB hardware (Phase 9). What's testable headlessly is that it
 * never hard-codes a device path and degrades to "not connected" safely
 * when no port is configured or the configured one doesn't exist.
 */
#include "chain/real_chain_output.h"

#include <gtest/gtest.h>

#include <cstdlib>

#include "core/frame.h"

namespace chainled::chain {
namespace {

class RealChainOutputTest : public ::testing::Test {
protected:
    void SetUp() override { unsetenv("CHAINLED_UART_DEVICE"); }
    void TearDown() override { unsetenv("CHAINLED_UART_DEVICE"); }
};

TEST_F(RealChainOutputTest, NotConnectedWithNoDevicePathOrEnvVar) {
    RealChainOutput output;
    EXPECT_FALSE(output.is_connected());
}

TEST_F(RealChainOutputTest, NotConnectedWithNonexistentExplicitPath) {
    RealChainOutput output("/nonexistent/chainled/test/device");
    EXPECT_FALSE(output.is_connected());
}

TEST_F(RealChainOutputTest, NotConnectedWithNonexistentEnvPath) {
    setenv("CHAINLED_UART_DEVICE", "/nonexistent/chainled/test/device", 1);
    RealChainOutput output;
    EXPECT_FALSE(output.is_connected());
}

TEST_F(RealChainOutputTest, ShowFrameIsNoOpWhenNotConnected) {
    RealChainOutput output;
    ASSERT_FALSE(output.is_connected());
    EXPECT_FALSE(output.show_frame(core::Frame(), /*mono=*/true));
}

}  // namespace
}  // namespace chainled::chain
