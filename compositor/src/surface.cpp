#include "surface.hpp"
#include "embedded_fonts.hpp"
#include "../../../zanop-kernel/src/drivers/vga.hpp"
#include "../../../zanop-kernel/src/framebuffer/framebuffer.hpp"
// NOTE: relative path above assumes zanop-desktop-os and zanop-kernel
// sit as sibling folders. Adjust if your local layout differs.

extern "C" void* memcpy(void* destination, const void* source, std::size_t count) {
    auto* dst = static_cast<unsigned char*>(destination);
    auto* src = static_cast<const unsigned char*>(source);
    for (std::size_t i = 0; i < count; ++i) {
        dst[i] = src[i];
    }
    return destination;
}

namespace surface {

namespace {

const std::uint8_t* glyph_for(Font font, char character) {
    using Glyph = const std::uint8_t*;
    Glyph table = nullptr;
    switch (font) {
        case Font::Ui:       table = reinterpret_cast<Glyph>(fonts::Ubuntu);      break;
        case Font::Terminal: table = reinterpret_cast<Glyph>(fonts::UbuntuMono);  break;
        case Font::Wobbly:   table = reinterpret_cast<Glyph>(fonts::WobblyDoodle); break;
        case Font::Mono:     table = reinterpret_cast<Glyph>(fonts::SpaceMono);   break;
    }

    // Only ASCII is baked in; anything outside that renders as blank rather
    // than indexing off the end of the table.
    const unsigned char code = static_cast<unsigned char>(character);
    if (code < fonts::FIRST_GLYPH || code >= fonts::FIRST_GLYPH + fonts::GLYPH_COUNT) {
        static constexpr std::uint8_t blank[fonts::GLYPH_WIDTH] = {};
        return blank;
    }
    return table + (code - fonts::FIRST_GLYPH) * fonts::GLYPH_WIDTH;
}

} // namespace

void present() {
    Framebuffer::present();
}

bool double_buffered() {
    return Framebuffer::is_double_buffered();
}

bool graphics_available() {
    return Framebuffer::is_available();
}

std::size_t pixel_width() {
    return Framebuffer::width();
}

std::size_t pixel_height() {
    return Framebuffer::height();
}

void draw_pixel(std::size_t x, std::size_t y, Color color) {
    if (!graphics_available() || x >= pixel_width() || y >= pixel_height()) return;
    Framebuffer::put_pixel(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y),
                           color.red, color.green, color.blue);
}

std::uint32_t read_pixel_raw(std::size_t x, std::size_t y) {
    if (!graphics_available() || x >= pixel_width() || y >= pixel_height()) return 0;
    return Framebuffer::read_pixel_raw(static_cast<std::uint32_t>(x),
                                      static_cast<std::uint32_t>(y));
}

void draw_pixel_raw(std::size_t x, std::size_t y, std::uint32_t pixel) {
    if (!graphics_available() || x >= pixel_width() || y >= pixel_height()) return;
    Framebuffer::put_pixel_raw(static_cast<std::uint32_t>(x),
                               static_cast<std::uint32_t>(y), pixel);
}

void fill_pixel_rect(std::size_t x, std::size_t y, std::size_t width,
                     std::size_t height, Color color) {
    if (!graphics_available() || x >= pixel_width() || y >= pixel_height()) return;
    const std::size_t clipped_width = width < pixel_width() - x ? width : pixel_width() - x;
    const std::size_t clipped_height = height < pixel_height() - y ? height : pixel_height() - y;
    Framebuffer::fill_rect(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y),
                           static_cast<std::uint32_t>(clipped_width),
                           static_cast<std::uint32_t>(clipped_height),
                           color.red, color.green, color.blue);
}

void draw_rgb_bitmap(std::size_t x, std::size_t y, std::size_t width,
                     std::size_t height, const std::uint8_t* pixels) {
    if (!graphics_available() || pixels == nullptr) return;
    Framebuffer::draw_rgb_bitmap(static_cast<std::uint32_t>(x),
                                 static_cast<std::uint32_t>(y),
                                 static_cast<std::uint32_t>(width),
                                 static_cast<std::uint32_t>(height), pixels);
}

void draw_text(std::size_t x, std::size_t y, const char* text, Color color,
               std::size_t scale, Font font) {
    if (!graphics_available() || text == nullptr || scale == 0) return;
    // Fully off-screen (or scrolled past) text costs nothing at all. This is
    // the common case for the window chrome, which is laid out for a fixed
    // frame size and gets clipped by smaller modes.
    const std::size_t limit_x = pixel_width();
    const std::size_t limit_y = pixel_height();
    if (x >= limit_x || y >= limit_y) return;
    if (text_height(scale) > limit_y - y) return;

    const std::size_t advance = text_advance(scale);
    for (std::size_t index = 0; text[index] != '\0'; ++index) {
        if (index > (limit_x - x) / advance) break;
        const std::size_t origin = x + index * advance;
        // Past the right edge, and every following character is further
        // right still, so the whole rest of the string can be skipped.
        if (origin >= limit_x) break;
        const std::uint8_t* glyph = glyph_for(font, text[index]);
        for (std::size_t row = 0; row < fonts::GLYPH_HEIGHT; ++row) {
            // Merge horizontal runs of lit pixels into a single span. A
            // typical glyph run is only 2-4 pixels wide, so this replaces
            // dozens of tiny scale-by-scale rectangles with a handful of
            // spans -- and each span is now a straight word-wide store.
            std::size_t column = 0;
            while (column < fonts::GLYPH_WIDTH) {
                if ((glyph[column] & (1u << row)) == 0) {
                    ++column;
                    continue;
                }
                std::size_t run = 1;
                while (column + run < fonts::GLYPH_WIDTH &&
                       (glyph[column + run] & (1u << row)) != 0) {
                    ++run;
                }
                fill_pixel_rect(origin + column * scale, y + row * scale,
                                run * scale, scale, color);
                column += run;
            }
        }
    }
}

void put_char(std::size_t col, std::size_t row, char c) {
    if (col >= SCREEN_WIDTH || row >= SCREEN_HEIGHT) return;
    VGA::writer.put_at(col, row, c);
}

void put_str(std::size_t col, std::size_t row, const char* s) {
    put_str_clipped(col, row, s, SCREEN_WIDTH - col);
}

void put_str_clipped(std::size_t col, std::size_t row, const char* s, std::size_t max_width) {
    if (col >= SCREEN_WIDTH || row >= SCREEN_HEIGHT || s == nullptr) return;
    std::size_t i = 0;
    while (s[i] != '\0' && i < max_width && col + i < SCREEN_WIDTH) {
        put_char(col + i, row, s[i]);
        ++i;
    }
}

void fill_rect(std::size_t col, std::size_t row, std::size_t w, std::size_t h, char c) {
    std::size_t max_row = row + h < SCREEN_HEIGHT ? row + h : SCREEN_HEIGHT;
    std::size_t max_col = col + w < SCREEN_WIDTH ? col + w : SCREEN_WIDTH;
    for (std::size_t y = row; y < max_row; ++y) {
        for (std::size_t x = col; x < max_col; ++x) {
            put_char(x, y, c);
        }
    }
}

} // namespace surface