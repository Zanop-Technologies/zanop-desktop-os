#include "surface.hpp"
#include "../../../zanop-kernel/src/drivers/vga.hpp"
// NOTE: relative path above assumes zanop-desktop-os and zanop-kernel
// sit as sibling folders. Adjust if your local layout differs.

namespace surface {

void put_char(std::size_t col, std::size_t row, char c) {
    if (col >= SCREEN_WIDTH || row >= SCREEN_HEIGHT) return;
    VGA::writer.put_at(col, row, c);
}

void put_str(std::size_t col, std::size_t row, const char* s) {
    std::size_t i = 0;
    while (s[i] != '\0') {
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