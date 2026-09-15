#pragma once
#include <cstddef>

// Thin drawing surface over the kernel's shared VGA writer.
// Deliberately does NOT open its own connection to 0xB8000 -- goes
// through VGA::writer.put_at() so screen contents stay consistent
// with anything else touching the display.

namespace surface {

constexpr std::size_t SCREEN_WIDTH = 80;
constexpr std::size_t SCREEN_HEIGHT = 25;

void put_char(std::size_t col, std::size_t row, char c);
void put_str(std::size_t col, std::size_t row, const char* s);
void fill_rect(std::size_t col, std::size_t row, std::size_t w, std::size_t h, char c);

} // namespace surface