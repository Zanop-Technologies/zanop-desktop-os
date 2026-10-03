#include "desktop.hpp"
#include "../../compositor/src/surface.hpp"
#include "embedded_icons.hpp"

namespace desktop_shell {

namespace {

constexpr surface::Color INK{13, 25, 39};
constexpr surface::Color CYAN{83, 220, 206};
constexpr surface::Color WHITE{238, 245, 248};
constexpr surface::Color MUTED{165, 184, 193};
bool warm_theme = false;

void draw_app_icon(std::size_t x, std::size_t y, const std::uint8_t* pixels,
                   const char* label) {
    surface::draw_rgb_bitmap(x, y, ICON_SIZE, ICON_SIZE, pixels);
    surface::draw_text(x, y + ICON_SIZE + 5, label, WHITE, 1);
}

void draw_graphical_background() {
    const std::size_t width = surface::pixel_width();
    const std::size_t height = surface::pixel_height();
    for (std::size_t y = 0; y < height; y += 4) {
        const std::size_t blend = height == 0 ? 0 : y * 255 / height;
        const std::uint8_t red = static_cast<std::uint8_t>(warm_theme ? 46 + blend / 3 : 12 + blend / 7);
        const std::uint8_t green = static_cast<std::uint8_t>(warm_theme ? 25 + blend / 4 : 38 + blend / 2);
        const std::uint8_t blue = static_cast<std::uint8_t>(warm_theme ? 38 + blend / 5 : 59 + blend / 2);
        surface::fill_pixel_rect(0, y, width, 4, {red, green, blue});
    }

    const std::size_t horizon = height * 3 / 5;
    for (std::size_t y = horizon; y < height; y += 5) {
        const std::size_t offset = (y - horizon) / 5;
        surface::fill_pixel_rect(width / 3 + offset * 2, y,
                                 width - (width / 3 + offset * 2), 5,
                                 warm_theme
                                     ? surface::Color{102, static_cast<std::uint8_t>(65 + offset / 3), 63}
                                     : surface::Color{18, static_cast<std::uint8_t>(86 + offset / 2), 91});
    }

    const std::size_t sun_x = width * 3 / 4;
    const std::size_t sun_y = height / 3;
    const std::size_t radius = height / 9;
    for (std::size_t dy = 0; dy < radius * 2; dy += 3) {
        const std::size_t relative_y = dy > radius ? dy - radius : radius - dy;
        const std::size_t span = radius * radius > relative_y * relative_y
            ? radius - relative_y / 2 : 0;
        if (span > 0) {
            surface::fill_pixel_rect(sun_x - span, sun_y + dy - radius, span * 2, 3,
                                     {246, 166, 105});
        }
    }

    draw_app_icon(32, 86, ICON_CLOCK_RGB, "CLOCK");
    draw_app_icon(32, 174, ICON_CALCULATOR_RGB, "CALC");
    draw_app_icon(32, 262, ICON_SNAKE_RGB, "SNAKE");
    draw_app_icon(32, 350, ICON_NOTES_RGB, "NOTES");
    draw_app_icon(132, 86, ICON_SETUP_RGB, "SETUP");
    draw_app_icon(132, 174, ICON_FILE_EXPLORER_RGB, "FILES");
    draw_app_icon(132, 262, ICON_SETUP_RGB, "SETTINGS");
    surface::fill_pixel_rect(0, height > 0 ? height - 2 : 0, width, 2, CYAN);
    surface::draw_text(width > 170 ? width - 170 : 8, height > 28 ? height - 26 : 8,
                       "DESKTOP 01", MUTED, 1);
    (void)INK;
}

} // namespace

bool warm_theme_enabled() {
    return warm_theme;
}

void set_warm_theme(bool enabled) {
    warm_theme = enabled;
}

void draw_background() {
    if (surface::graphics_available()) {
        draw_graphical_background();
        return;
    }

    surface::fill_rect(0, 0, surface::SCREEN_WIDTH, surface::SCREEN_HEIGHT, ' ');

    for (std::size_t row = 0; row < surface::SCREEN_HEIGHT; ++row) {
        for (std::size_t col = 0; col < surface::SCREEN_WIDTH; ++col) {
            const bool accent = ((row + col) % 5 == 0) || ((row * 2 + col) % 9 == 0);
            const char glyph = accent ? '\xdb' : ' ';
            surface::put_char(col, row, glyph);
        }
    }

    surface::fill_rect(4, 3, 18, 7, ' ');
    for (std::size_t row = 3; row < 10; ++row) {
        surface::put_char(5, row, '\xb3');
        surface::put_char(18, row, '\xb3');
    }
    surface::put_char(4, 3, '\xda');
    surface::put_char(21, 3, '\xbf');
    surface::put_char(4, 9, '\xc0');
    surface::put_char(21, 9, '\xd9');

    for (std::size_t col = 5; col < 21; ++col) {
        surface::put_char(col, 3, '\xc4');
        surface::put_char(col, 9, '\xc4');
    }

    surface::put_str_clipped(7, 5, "Home", 8);
    surface::put_str_clipped(7, 6, "Documents", 12);
    surface::put_str_clipped(7, 7, "Files", 8);
}

} // namespace desktop_shell