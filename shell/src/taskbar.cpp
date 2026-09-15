#include "taskbar.hpp"
#include "../../compositor/src/surface.hpp"

// NOTE: no live clock here yet -- the kernel's RTC driver's real
// current namespace hasn't been confirmed this session (same risk
// class as the vga::/VGA:: mismatch found earlier). Deliberately not
// guessing a fifth unverified cross-repo namespace on top of the ones
// already in play. Add the clock once rtc.hpp is confirmed.

namespace desktop_shell {

const char* BUILD_NAME = "Zanop Desktop OS -- Build \"Chicago\" (C++)";

void draw_top_bar() {
    surface::fill_rect(0, 0, surface::SCREEN_WIDTH, 1, ' ');
    surface::put_str(1, 0, BUILD_NAME);
}

void draw_bottom_bar() {
    std::size_t row = surface::SCREEN_HEIGHT - 1;
    surface::fill_rect(0, row, surface::SCREEN_WIDTH, 1, ' ');
    surface::put_str(1, row, "[ Start ]");
}

} // namespace desktop_shell