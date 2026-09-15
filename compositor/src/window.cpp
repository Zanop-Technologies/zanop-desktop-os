#include "window.hpp"
#include "surface.hpp"
#include <cstring>

namespace compositor {

Window::Window(const char* title, std::size_t x, std::size_t y, std::size_t w, std::size_t h)
    : x_(x), y_(y), w_(w), h_(h), line_count_(0) {
    std::size_t i = 0;
    while (title[i] != '\0' && i < MAX_TITLE_LEN) {
        title_[i] = title[i];
        ++i;
    }
    title_[i] = '\0';
    for (auto& line : lines_) line[0] = '\0';
}

void Window::set_body_line(std::size_t index, const char* line) {
    if (index >= MAX_BODY_LINES) return;
    std::size_t i = 0;
    while (line[i] != '\0' && i < MAX_LINE_LEN) {
        lines_[index][i] = line[i];
        ++i;
    }
    lines_[index][i] = '\0';
    if (index + 1 > line_count_) line_count_ = index + 1;
}

void Window::clear_body() {
    line_count_ = 0;
    for (auto& line : lines_) line[0] = '\0';
}

void Window::draw() const {
    // corners + edges (CP437 box-drawing codes -- VGA text mode is
    // single-byte-per-cell, so raw UTF-8 box characters would not
    // render correctly here)
    surface::put_char(x_, y_, '\xda');                    // top-left
    surface::put_char(x_ + w_ - 1, y_, '\xbf');            // top-right
    surface::put_char(x_, y_ + h_ - 1, '\xc0');            // bottom-left
    surface::put_char(x_ + w_ - 1, y_ + h_ - 1, '\xd9');   // bottom-right

    for (std::size_t i = 1; i < w_ - 1; ++i) {
        surface::put_char(x_ + i, y_, '\xc4');             // top edge
        surface::put_char(x_ + i, y_ + h_ - 1, '\xc4');    // bottom edge
    }
    for (std::size_t i = 1; i < h_ - 1; ++i) {
        surface::put_char(x_, y_ + i, '\xb3');             // left edge
        surface::put_char(x_ + w_ - 1, y_ + i, '\xb3');    // right edge
    }

    // title bar row
    std::size_t title_row = y_ + 1;
    surface::fill_rect(x_ + 1, title_row, w_ - 2, 1, ' ');
    surface::put_str(x_ + 2, title_row, title_);

    // divider under the title bar
    surface::put_char(x_, y_ + 2, '\xc3');
    surface::put_char(x_ + w_ - 1, y_ + 2, '\xb4');
    for (std::size_t i = 1; i < w_ - 1; ++i) {
        surface::put_char(x_ + i, y_ + 2, '\xc4');
    }

    // body
    std::size_t body_start = y_ + 3;
    std::size_t body_height = h_ > 4 ? h_ - 4 : 0;
    std::size_t lines_to_draw = line_count_ < body_height ? line_count_ : body_height;
    for (std::size_t i = 0; i < lines_to_draw; ++i) {
        std::size_t row = body_start + i;
        surface::fill_rect(x_ + 1, row, w_ - 2, 1, ' ');
        surface::put_str(x_ + 2, row, lines_[i]);
    }
}

} // namespace compositor