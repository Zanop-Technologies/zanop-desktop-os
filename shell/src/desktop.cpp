#include "desktop.hpp"
#include "../../compositor/src/surface.hpp"

namespace desktop_shell {

// Dither pattern fill -- the closest thing to "wallpaper" in text mode.
void draw_background() {
    for (std::size_t row = 0; row < surface::SCREEN_HEIGHT; ++row) {
        for (std::size_t col = 0; col < surface::SCREEN_WIDTH; ++col) {
            char c = ((row + col) % 4 == 0) ? '.' : ' ';
            surface::put_char(col, row, c);
        }
    }
}

} // namespace desktop_shell