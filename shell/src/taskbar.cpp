#include "taskbar.hpp"
#include "../../compositor/src/surface.hpp"
#include "../../input/src/input.hpp"
#include "embedded_icons.hpp"

// NOTE: no live clock here yet -- the kernel's RTC driver's real
// current namespace hasn't been confirmed this session (same risk
// class as the vga::/VGA:: mismatch found earlier). Deliberately not
// guessing a fifth unverified cross-repo namespace on top of the ones
// already in play. Add the clock once rtc.hpp is confirmed.

namespace desktop_shell {

const char* BUILD_NAME = "Zanop Desktop OS -- Build \"Chicago\" (C++)";

void draw_top_bar() {
    if (surface::graphics_available()) {
        const std::size_t width = surface::pixel_width();
        surface::fill_pixel_rect(0, 0, width, 42, {12, 24, 36});
        surface::fill_pixel_rect(0, 40, width, 2, {83, 220, 206});
        surface::draw_rgb_bitmap(18, 1, ICON_SIZE, ICON_SIZE, ICON_LAUNCHER_RGB);
        surface::draw_text(64, 15, "ZANOP", {238, 245, 248}, 2);
        surface::draw_text(190, 18, "FILE   VIEW   APPS   SYSTEM", {165, 184, 193}, 1);
        surface::draw_text(width > 100 ? width - 92 : 8, 18, "READY", {83, 220, 206}, 1);
        const char* mouse_status = "MOUSE OK";
        switch (input::mouse_init_error()) {
            case 1: mouse_status = "PS2 CTRL"; break;
            case 2: mouse_status = "PS2 CFG"; break;
            case 3: mouse_status = "PS2 DEF"; break;
            case 4: mouse_status = "PS2 DATA"; break;
            default: break;
        }
        surface::draw_text(width > 100 ? width - 92 : 8, 29, mouse_status,
                           input::mouse_available() ? surface::Color{165, 184, 193}
                                                    : surface::Color{226, 106, 94}, 1);
        return;
    }

    surface::fill_rect(0, 0, surface::SCREEN_WIDTH, 2, ' ');
    surface::put_str_clipped(2, 0, BUILD_NAME, surface::SCREEN_WIDTH - 4);
    surface::put_str_clipped(2, 1, "File   View   Apps   System", 30);
    surface::put_str_clipped(62, 1, "status: ready", 14);
}

void draw_bottom_bar() {
    if (surface::graphics_available()) {
        const std::size_t width = surface::pixel_width();
        const std::size_t height = surface::pixel_height();
        const std::size_t bar_y = height > 56 ? height - 56 : 0;
        surface::fill_pixel_rect(0, bar_y, width, height - bar_y, {12, 24, 36});
        surface::fill_pixel_rect(18, bar_y + 10, 116, 36, {83, 220, 206});
        surface::draw_text(38, bar_y + 22, "START", {12, 24, 36}, 1);
        surface::fill_pixel_rect(150, bar_y + 10, 2, 36, {62, 83, 94});
        surface::draw_text(172, bar_y + 22, "DESKTOP", {238, 245, 248}, 1);
        surface::draw_text(width > 112 ? width - 102 : 8, bar_y + 22,
                           "6 APPS", {165, 184, 193}, 1);
        return;
    }

    std::size_t row = surface::SCREEN_HEIGHT - 1;
    surface::fill_rect(0, row, surface::SCREEN_WIDTH, 1, ' ');
    surface::put_str_clipped(1, row, "[ Start ]   Home   Documents   Run   Power", 52);
}

} // namespace desktop_shell