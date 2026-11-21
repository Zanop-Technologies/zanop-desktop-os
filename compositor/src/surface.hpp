#pragma once
#include "../../../zanop-kernel/src/types.hpp"

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

// The TTFs baked into the binary by assets/fonts/build_fonts.py.
//   Ui       -> Ubuntu, an alternate UI face
//   Terminal -> Ubuntu Mono, reserved for the Terminal app
//   Mono     -> Space Mono, the default face throughout the desktop
//   Wobbly   -> Wobblydoodle, the heading/wordmark face
enum class Font {
	Ui,
	Terminal,
	Mono,
	Wobbly,
};

constexpr Font DEFAULT_FONT = Font::Mono;

// Width in pixels of one character cell for the given scale (glyph + spacing).
constexpr std::size_t text_advance(std::size_t scale) { return 9 * scale; }
// Height in pixels of a line of text for the given scale.
constexpr std::size_t text_height(std::size_t scale) { return 12 * scale; }

// Swap the framebuffer to its back buffer and present. Every draw_* call
// above goes to the back buffer only, so nothing is visible until present()
// copies the finished frame out -- that is what removes the tearing /
// flicker you would otherwise get from drawing straight to the display.
void present();
bool double_buffered();

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
			   std::size_t scale = 2, Font font = DEFAULT_FONT);

void put_char(std::size_t col, std::size_t row, char c);
void put_str(std::size_t col, std::size_t row, const char* s);
void put_str_clipped(std::size_t col, std::size_t row, const char* s, std::size_t max_width);
void fill_rect(std::size_t col, std::size_t row, std::size_t w, std::size_t h, char c);

} // namespace surface