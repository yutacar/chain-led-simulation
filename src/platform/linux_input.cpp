/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */
#include "linux_input.h"

#if !USE_DESKTOP
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#ifndef APP_KEY_INPUT_DEVICE
#define APP_KEY_INPUT_DEVICE ""
#endif

namespace platform {
namespace {

#if !USE_DESKTOP
struct EvdevKeypad {
    int fd{-1};
    lv_indev_state_t state{LV_INDEV_STATE_RELEASED};
    uint32_t key{0};
    bool shift{false};  ///< Left/Right Shift currently held on this keyboard
    bool tca8418_symbols{false};  ///< built-in CardputerZero keyboard (see tca8418_symbol)
};

// The CardputerZero's built-in keyboard (tca8418_keypad_m5stack driver) has
// no punctuation keys on its base layer; its Sym layer emits these codes as
// "symbol slots" whose meaning differs from the standard evdev names (e.g.
// Sym+R sends 71/KEY_KP7 for '-', Sym+T 72/KEY_KP8 for '+', Sym+U
// 74/KEY_KPMINUS for '['). Table from M5's own FactoryTest app
// (CardputerZero/FactoryTest src/platform/linux_input.cpp), matching the
// device-tree linux,keymap1 layer. Applied only to that keyboard so a USB
// keyboard on the USB-A port keeps standard evdev meanings.
uint32_t tca8418_symbol(uint16_t code) {
    static constexpr struct { uint16_t code; char symbol; } kSymbols[] = {
        {26, '!'}, {27, '@'}, {39, '#'}, {40, '$'}, {41, '%'}, {43, '^'}, {51, '&'}, {52, '*'},
        {53, '('}, {94, ')'}, {55, '~'}, {69, '`'}, {70, '_'}, {71, '-'}, {72, '+'}, {73, '='},
        {74, '['}, {75, ']'}, {76, '{'}, {77, '}'}, {79, ';'}, {80, ':'}, {81, '\''}, {82, '"'},
        {83, '<'}, {85, '>'}, {86, '\\'}, {89, '|'}, {90, ','}, {91, '.'}, {92, '/'}, {93, '?'},
    };
    for (const auto& entry : kSymbols) {
        if (entry.code == code) {
            return static_cast<uint32_t>(static_cast<unsigned char>(entry.symbol));
        }
    }
    return 0;
}

// Maps the CardputerZero's 46-key matrix to LVGL group-navigation keys.
// Arrow keys move focus/cursor, Enter activates/toggles, Esc/Home drive the
// short-back / quit convention centrally (see ScreenManager::on_indev_key).
// Screen-specific single-letter shortcuts (Editor's C/F/V/N/D/P/S, etc.) are
// plain ASCII; the desktop SDL keyboard driver already produces those
// natively, and device support for them is added when the corresponding
// evdev key codes are needed on real hardware.
uint32_t map_evdev_key(uint16_t code, bool shift) {
    // Letters: the CardputerZero's tca8418 driver emits plain KEY_A..KEY_Z on
    // its base layer (Fn+F/Z/X/C produce KEY_UP/LEFT/DOWN/RIGHT). Without
    // these, every single-letter screen shortcut was silently dropped on the
    // real device while still working in the SDL desktop build.
    static constexpr struct { uint16_t code; char lower; } kLetters[] = {
        {KEY_A, 'a'}, {KEY_B, 'b'}, {KEY_C, 'c'}, {KEY_D, 'd'}, {KEY_E, 'e'}, {KEY_F, 'f'},
        {KEY_G, 'g'}, {KEY_H, 'h'}, {KEY_I, 'i'}, {KEY_J, 'j'}, {KEY_K, 'k'}, {KEY_L, 'l'},
        {KEY_M, 'm'}, {KEY_N, 'n'}, {KEY_O, 'o'}, {KEY_P, 'p'}, {KEY_Q, 'q'}, {KEY_R, 'r'},
        {KEY_S, 's'}, {KEY_T, 't'}, {KEY_U, 'u'}, {KEY_V, 'v'}, {KEY_W, 'w'}, {KEY_X, 'x'},
        {KEY_Y, 'y'}, {KEY_Z, 'z'},
    };
    for (const auto& letter : kLetters) {
        if (letter.code == code) {
            return static_cast<uint32_t>(shift ? letter.lower - 'a' + 'A' : letter.lower);
        }
    }
    // Digits and their US-layout shifted symbols (system xkb layout is "us").
    static constexpr char kDigits[] = "1234567890";
    static constexpr char kDigitsShifted[] = "!@#$%^&*()";
    if (code >= KEY_1 && code <= KEY_0) {
        const int index = code - KEY_1;
        return static_cast<uint32_t>(shift ? kDigitsShifted[index] : kDigits[index]);
    }

    switch (code) {
        case KEY_ESC:         return LV_KEY_ESC;
        case KEY_HOME:        return LV_KEY_HOME;
        case KEY_END:         return LV_KEY_END;
        case KEY_UP:          return LV_KEY_UP;
        case KEY_DOWN:        return LV_KEY_DOWN;
        case KEY_LEFT:        return LV_KEY_LEFT;
        case KEY_RIGHT:       return LV_KEY_RIGHT;
        case KEY_ENTER:
        case KEY_KPENTER:     return LV_KEY_ENTER;
        case KEY_BACKSPACE:   return LV_KEY_BACKSPACE;
        case KEY_DELETE:      return LV_KEY_DEL;
        case KEY_SPACE:       return ' ';
        case KEY_MINUS:       return shift ? '_' : '-';
        case KEY_EQUAL:       return shift ? '+' : '=';
        case KEY_LEFTBRACE:   return shift ? '{' : '[';
        case KEY_RIGHTBRACE:  return shift ? '}' : ']';
        case KEY_SEMICOLON:   return shift ? ':' : ';';
        case KEY_APOSTROPHE:  return shift ? '"' : '\'';
        case KEY_GRAVE:       return shift ? '~' : '`';
        case KEY_BACKSLASH:   return shift ? '|' : '\\';
        case KEY_COMMA:       return shift ? '<' : ',';
        case KEY_DOT:         return shift ? '>' : '.';
        case KEY_SLASH:       return shift ? '?' : '/';
        // Symbols the keyboard's Sym layer sends as keypad codes.
        case KEY_KPPLUS:      return '+';
        case KEY_KPMINUS:     return '-';
        case KEY_KPASTERISK:  return '*';
        case KEY_KPSLASH:     return '/';
        case KEY_KPDOT:       return '.';
        case KEY_KPEQUAL:     return '=';
        default:              return 0;
    }
}

bool has_nav_keys(int fd) {
    unsigned long key_bits[(KEY_MAX / (sizeof(unsigned long) * 8)) + 1] = {};
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits) < 0) {
        return false;
    }

    auto has_key = [&](int code) {
        const auto bits_per_word = static_cast<int>(sizeof(unsigned long) * 8);
        return (key_bits[code / bits_per_word] & (1UL << (code % bits_per_word))) != 0;
    };

    return has_key(KEY_ESC) || has_key(KEY_UP) || has_key(KEY_DOWN) || has_key(KEY_LEFT) ||
           has_key(KEY_RIGHT) || has_key(KEY_ENTER);
}

void evdev_read_cb(lv_indev_t* indev, lv_indev_data_t* data) {
    auto* keypad = static_cast<EvdevKeypad*>(lv_indev_get_driver_data(indev));
    if (!keypad) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    input_event input{};
    while (read(keypad->fd, &input, sizeof(input)) == sizeof(input)) {
        if (input.type != EV_KEY) {
            continue;
        }

        if (input.code == KEY_LEFTSHIFT || input.code == KEY_RIGHTSHIFT) {
            if (input.value != 2) {
                keypad->shift = input.value != 0;
            }
            continue;
        }

        uint32_t key = keypad->tca8418_symbols ? tca8418_symbol(input.code) : 0;
        if (!key) {
            key = map_evdev_key(input.code, keypad->shift);
        }
        if (!key || input.value == 2) {
            continue;
        }

        keypad->key = key;
        keypad->state = input.value ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
        data->continue_reading = true;
        break;
    }

    data->key = keypad->key;
    data->state = keypad->state;
}

void evdev_delete_cb(lv_event_t* event) {
    auto* indev = static_cast<lv_indev_t*>(lv_event_get_target(event));
    auto* keypad = static_cast<EvdevKeypad*>(lv_indev_get_driver_data(indev));
    if (!keypad) {
        return;
    }

    if (keypad->fd >= 0) {
        close(keypad->fd);
    }
    delete keypad;
}

lv_indev_t* create_keypad_from_fd(int fd) {
    auto* keypad = new EvdevKeypad;
    keypad->fd = fd;
    char name[64] = {};
    if (ioctl(fd, EVIOCGNAME(sizeof(name) - 1), name) >= 0) {
        keypad->tca8418_symbols = std::strstr(name, "tca8418") != nullptr;
    }

    auto* indev = lv_indev_create();
    if (!indev) {
        delete keypad;
        close(fd);
        return nullptr;
    }

    lv_indev_set_type(indev, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(indev, evdev_read_cb);
    lv_indev_set_driver_data(indev, keypad);
    lv_indev_add_event_cb(indev, evdev_delete_cb, LV_EVENT_DELETE, nullptr);
    lv_indev_set_group(indev, lv_group_get_default());
    return indev;
}

lv_indev_t* try_create_keypad(const char* path) {
    if (!path || path[0] == '\0') {
        return nullptr;
    }

    const int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        LV_LOG_WARN("failed to open input device %s: %s", path, strerror(errno));
        return nullptr;
    }

    if (!has_nav_keys(fd)) {
        close(fd);
        return nullptr;
    }

    LV_LOG_INFO("using evdev key input %s", path);
    return create_keypad_from_fd(fd);
}

void discover_keypads(lv_display_t* display, const KeypadIndevCallback& on_indev_created) {
    const char* configured_device = APP_KEY_INPUT_DEVICE;
    if (configured_device[0] != '\0') {
        auto* indev = try_create_keypad(configured_device);
        if (indev) {
            lv_indev_set_display(indev, display);
            if (on_indev_created) {
                on_indev_created(indev);
            }
        }
        return;
    }

    auto* dir = opendir("/dev/input");
    if (!dir) {
        LV_LOG_WARN("failed to open /dev/input: %s", strerror(errno));
        return;
    }

    while (auto* entry = readdir(dir)) {
        if (std::strncmp(entry->d_name, "event", 5) != 0) {
            continue;
        }

        std::string path = "/dev/input/";
        path += entry->d_name;
        auto* indev = try_create_keypad(path.c_str());
        if (indev) {
            lv_indev_set_display(indev, display);
            if (on_indev_created) {
                on_indev_created(indev);
            }
        }
    }

    closedir(dir);
}
#endif

} // namespace

void init_key_input(lv_display_t* display, const KeypadIndevCallback& on_indev_created) {
#if !USE_DESKTOP
    discover_keypads(display, on_indev_created);
#else
    LV_UNUSED(display);
    LV_UNUSED(on_indev_created);
#endif
}

} // namespace platform
