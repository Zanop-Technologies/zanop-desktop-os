#pragma once
#include "types.hpp"

// Central input adapter for the desktop shell. It keeps keyboard and
// PS/2 mouse callers independent of the kernel driver namespaces.
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

struct MouseEvent {
	int delta_x;
	int delta_y;
	std::uint8_t buttons;
};

// Always returns std::nullopt for now -- arrow keys send an 0xE0
// prefix byte followed by a second scancode (PS/2 Scan Code Set 1),
// which nothing in the kernel's keyboard driver currently decodes.
// This returns nothing rather than silently doing the wrong thing;
// wire this up once 0xE0-prefix handling lands in the kernel.
std::optional<ArrowKey> read_arrow_key();
std::optional<MouseEvent> try_read_mouse_event();
bool mouse_available();
std::uint8_t mouse_init_error();

} // namespace input