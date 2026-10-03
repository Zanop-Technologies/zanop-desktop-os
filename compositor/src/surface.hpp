#pragma once
#include "types.hpp"

// Thin drawing surface over the kernel's shared VGA writer.
// Deliberately does NOT open its own connection to 0xB8000 -- goes
// through VGA::writer.put_at() so screen contents stay consistent
// with anything else touching the display.

namespace surface {

constexpr std::size_t SCREEN_WIDTH = 80;
constexpr std::size_t SCREEN_HEIGHT = 25;

struct Color {
	std::uint8_t red;
	std::uint8_t green;
	std::uint8_t blue;
};

bool graphics_available();
std::size_t pixel_width();
std::size_t pixel_height();
void draw_pixel(std::size_t x, std::size_t y, Color color);
std::uint32_t read_pixel_raw(std::size_t x, std::size_t y);
void draw_pixel_raw(std::size_t x, std::size_t y, std::uint32_t pixel);
void fill_pixel_rect(std::size_t x, std::size_t y, std::size_t width,
					 std::size_t height, Color color);
void draw_rgb_bitmap(std::size_t x, std::size_t y, std::size_t width,
					 std::size_t height, const std::uint8_t* pixels);
void draw_text(std::size_t x, std::size_t y, const char* text, Color color,
			   std::size_t scale = 2);

void put_char(std::size_t col, std::size_t row, char c);
void put_str(std::size_t col, std::size_t row, const char* s);
void put_str_clipped(std::size_t col, std::size_t row, const char* s, std::size_t max_width);
void fill_rect(std::size_t col, std::size_t row, std::size_t w, std::size_t h, char c);

} // namespace surface