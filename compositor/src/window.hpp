#pragma once
#include "types.hpp"

// Pixel window when a framebuffer is available, with a VGA text-mode
// fallback for boot configurations that do not provide one.

namespace compositor {

constexpr std::size_t MAX_TITLE_LEN = 40;
constexpr std::size_t MAX_BODY_LINES = 16;
constexpr std::size_t MAX_LINE_LEN = 76;

class Window {
public:
    Window(const char* title, std::size_t x, std::size_t y, std::size_t w, std::size_t h);

    void set_body_line(std::size_t index, const char* line);
    void clear_body();
    void draw() const;

    const char* title() const { return title_; }

private:
    char title_[MAX_TITLE_LEN + 1];
    std::size_t x_, y_, w_, h_;
    char lines_[MAX_BODY_LINES][MAX_LINE_LEN + 1];
    std::size_t line_count_;
};

} // namespace compositor