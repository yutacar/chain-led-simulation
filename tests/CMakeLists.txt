#
# SPDX-FileCopyrightText: 2026 yutacar
#
# SPDX-License-Identifier: MIT
#
# Unit tests for chainled_animation_core, chainled_generator, chainled_chain,
# chainled_net, and chainled_i18n. Desktop-only (see parent CMakeLists.txt);
# GoogleTest is fetched the same way LVGL is fetched in the top-level file.

include(FetchContent)

set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(googletest)

add_executable(chainled_animation_core_tests
    test_frame.cpp
    test_animation.cpp
    test_animation_serializer.cpp
    test_procedural_completer.cpp
    test_chain_protocol.cpp
    test_frame_codec.cpp
    test_real_chain_output.cpp
    test_net_frame.cpp
    test_loopback_transport.cpp
    test_tcp_transport.cpp
    test_animation_transfer.cpp
    test_i18n_catalog.cpp
    test_i18n_settings.cpp
)

target_link_libraries(chainled_animation_core_tests
    PRIVATE
        chainled_animation_core
        chainled_generator
        chainled_chain
        chainled_net
        chainled_i18n
        GTest::gtest
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(chainled_animation_core_tests)
