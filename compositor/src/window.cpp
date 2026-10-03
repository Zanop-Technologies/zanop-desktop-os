#include "window.hpp"
#include "surface.hpp"

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
    if (surface::graphics_available()) {
        const std::size_t shadow = 8;
        surface::fill_pixel_rect(x_ + shadow, y_ + shadow, w_, h_, {7, 16, 25});
        surface::fill_pixel_rect(x_, y_, w_, h_, {237, 243, 245});
        surface::fill_pixel_rect(x_, y_, w_, 48, {19, 39, 52});
        surface::fill_pixel_rect(x_, y_ + 48, w_, 3, {83, 220, 206});
        surface::draw_text(x_ + 22, y_ + 19, title_, {238, 245, 248}, 2);
        surface::fill_pixel_rect(x_ + w_ - 76, y_ + 16, 12, 12, {82, 105, 113});
        surface::fill_pixel_rect(x_ + w_ - 52, y_ + 16, 12, 12, {82, 105, 113});
        surface::fill_pixel_rect(x_ + w_ - 28, y_ + 16, 12, 12, {226, 106, 94});

        for (std::size_t index = 0; index < line_count_; ++index) {
            const char* line = lines_[index];
            if (line[0] == '\0') continue;
            const std::size_t row_y = y_ + 76 + index * 31;
            const bool app_item = line[0] >= '1' && line[0] <= '4';
            if (app_item) {
                surface::fill_pixel_rect(x_ + 18, row_y - 6, w_ - 36, 28,
                                         index % 2 == 0 ? surface::Color{220, 231, 233}
                                                        : surface::Color{228, 236, 237});
                const surface::Color colors[] = {{83, 180, 194}, {235, 169, 92},
                                                  {127, 187, 133}, {224, 126, 122}};
                surface::fill_pixel_rect(x_ + 28, row_y - 1, 18, 18, colors[index % 4]);
                surface::draw_text(x_ + 58, row_y + 4, line, {29, 48, 58}, 1);
            } else {
                surface::draw_text(x_ + 24, row_y + 4, line, {66, 89, 98}, 1);
            }
        }
        return;
    }

    if (w_ < 4 || h_ < 4) {
        surface::fill_rect(x_, y_, w_ > 0 ? w_ : 1, h_ > 0 ? h_ : 1, ' ');
        return;
    }

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

    const std::size_t title_row = y_ + 1;
    const std::size_t body_start = y_ + 3;
    const std::size_t inner_width = w_ > 2 ? w_ - 2 : 1;
    const std::size_t body_height = h_ > 4 ? h_ - 4 : 0;

    surface::fill_rect(x_ + 1, title_row, inner_width, 1, ' ');
    surface::put_str_clipped(x_ + 2, title_row, title_, inner_width - 1);

    surface::put_char(x_, y_ + 2, '\xc3');
    surface::put_char(x_ + w_ - 1, y_ + 2, '\xb4');
    for (std::size_t i = 1; i < w_ - 1; ++i) {
        surface::put_char(x_ + i, y_ + 2, '\xc4');
    }

    const std::size_t lines_to_draw = line_count_ < body_height ? line_count_ : body_height;
    for (std::size_t i = 0; i < lines_to_draw; ++i) {
        const std::size_t row = body_start + i;
        surface::fill_rect(x_ + 1, row, inner_width, 1, ' ');
        surface::put_str_clipped(x_ + 2, row, lines_[i], inner_width - 1);
    }
}

} // namespace compositor