#pragma once
// The mouse pointer, shared by the desktop and by every app.
//
// The pointer is never drawn into the back buffer. It is composited onto the
// *display* after each flip: surface::set_present_hook() runs pointer::
// on_present() after every present()/present_rect(), which copies the pointer
// onto the screen only. Apps therefore never have to know about the pointer;
// whatever they paint and flip, the pointer reappears on top of it.
//
// Header-only on purpose: the kernel's Makefile lists source files explicitly,
// and this needs no new object file.
#include "../../compositor/src/surface.hpp"
#include "../../input/src/input.hpp"

namespace desktop_shell {
namespace pointer {

constexpr std::size_t WIDTH = 12;
constexpr std::size_t HEIGHT = 16;

inline int cursor_x = 0;        // logical position
inline int cursor_y = 0;
inline int shown_x = 0;         // where the pointer currently is on the display
inline int shown_y = 0;
inline bool left_down = false;  // last seen state of the left button
inline bool enabled = true;     // false while a screen draws its own pointer
inline bool compositing = false;
inline std::uint32_t underlay[WIDTH * HEIGHT];

inline void center() {
    cursor_x = static_cast<int>(surface::pixel_width() / 2);
    cursor_y = static_cast<int>(surface::pixel_height() / 2);
    shown_x = cursor_x;
    shown_y = cursor_y;
}

// Draws the pointer onto the display (not into the persistent back buffer): it
// is painted into the back buffer, that small rectangle is flipped, and the
// saved pixels are put back so the back buffer stays pointer-free.
inline void overlay() {
    if (!enabled || !surface::graphics_available()) return;
    const std::size_t x = static_cast<std::size_t>(cursor_x);
    const std::size_t y = static_cast<std::size_t>(cursor_y);
    for (std::size_t row = 0; row < HEIGHT; ++row)
        for (std::size_t column = 0; column < WIDTH; ++column)
            underlay[row * WIDTH + column] = surface::read_pixel_raw(x + column, y + row);
    surface::fill_pixel_rect(x, y, 12, 16, {5, 15, 22});
    surface::fill_pixel_rect(x + 2, y + 2, 8, 12, {244, 248, 242});
    surface::fill_pixel_rect(x + 4, y + 8, 4, 2, {83, 220, 206});
    compositing = true;
    surface::present_rect(x, y, WIDTH, HEIGHT);
    compositing = false;
    for (std::size_t row = 0; row < HEIGHT; ++row)
        for (std::size_t column = 0; column < WIDTH; ++column)
            surface::draw_pixel_raw(x + column, y + row, underlay[row * WIDTH + column]);
    shown_x = cursor_x;
    shown_y = cursor_y;
}

// Registered with surface::set_present_hook().
inline void on_present() {
    if (!compositing) overlay();
}

// Moves the pointer on the display after cursor_x/cursor_y changed: the old
// spot is repainted from the pointer-free back buffer, then the pointer is
// drawn at the new one.
inline void refresh() {
    if (!surface::graphics_available()) return;
    surface::present_rect(static_cast<std::size_t>(shown_x), static_cast<std::size_t>(shown_y),
                          WIDTH, HEIGHT); // the present hook redraws the pointer
    if (shown_x != cursor_x || shown_y != cursor_y) overlay(); // no hook installed
}

// Applies one mouse packet. Returns true if it is a new left-button press.
inline bool apply(const input::MouseEvent& event) {
    const int max_x = surface::pixel_width() > WIDTH
        ? static_cast<int>(surface::pixel_width() - WIDTH) : 0;
    const int max_y = surface::pixel_height() > HEIGHT
        ? static_cast<int>(surface::pixel_height() - HEIGHT) : 0;
    const int next_x = cursor_x + event.delta_x;
    const int next_y = cursor_y + event.delta_y;
    cursor_x = next_x < 0 ? 0 : (next_x > max_x ? max_x : next_x);
    cursor_y = next_y < 0 ? 0 : (next_y > max_y ? max_y : next_y);
    const bool down = (event.buttons & 0x01) != 0;
    const bool pressed = down && !left_down;
    left_down = down;
    return pressed;
}

// Handles every queued mouse packet, moves the pointer once, and reports a left
// click (where the press happened). Installed as the input layer's mouse sink,
// so it runs whenever any screen waits for or polls the keyboard.
inline bool service(int& click_x, int& click_y) {
    if (!enabled) return false;
    bool moved = false;
    bool clicked = false;
    while (auto event = input::try_read_mouse_event()) {
        moved = true;
        if (apply(*event)) {
            clicked = true;
            click_x = cursor_x;
            click_y = cursor_y;
        }
    }
    if (moved) refresh();
    return clicked;
}

inline void install() {
    surface::set_present_hook(on_present);
    input::set_mouse_sink(service);
}

} // namespace pointer
} // namespace desktop_shell
