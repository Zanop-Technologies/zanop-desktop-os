#pragma once
#include <cstdint>
#include <optional>

// Input handling for the Desktop OS GUI layer. Currently just wraps
// the kernel's existing keyboard driver -- there's nothing GUI-
// specific to add yet since navigation is number-key/Enter/Esc based
// (see installer/src/installer.cpp, shell/). This exists as the
// single place extended scancodes (arrow keys, function keys) and
// eventually mouse input will get decoded once those drivers exist,
// so callers depend on `input::`, not directly on the kernel's
// keyboard namespace -- isolates the risk if that namespace/API ever
// changes again (see FIXES.md's "Namespace / API mismatches" entry).
//
// NOTE: the wrapped functions below assume the kernel's keyboard
// driver lives in `Drivers::Keyboard::` with this signature -- this
// is UNVERIFIED as of this file's writing (the real current
// keyboard.hpp hasn't been confirmed this session). If it differs,
// only this file needs to change, not every caller.

namespace input {

std::uint8_t read_scancode();                    // blocks until a real key event
std::optional<std::uint8_t> try_read_scancode();  // non-blocking
std::optional<char> scancode_to_ascii(std::uint8_t code);

enum class ArrowKey { Up, Down, Left, Right };

// Always returns std::nullopt for now -- arrow keys send an 0xE0
// prefix byte followed by a second scancode (PS/2 Scan Code Set 1),
// which nothing in the kernel's keyboard driver currently decodes.
// This returns nothing rather than silently doing the wrong thing;
// wire this up once 0xE0-prefix handling lands in the kernel.
std::optional<ArrowKey> read_arrow_key();

} // namespace input