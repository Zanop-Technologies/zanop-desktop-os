#include "launcher.hpp"
#include "../../compositor/src/surface.hpp"
#include "../../../zanop-kernel/src/apps/apps.hpp"

namespace desktop_shell {

namespace {

constexpr const char* APP_NAMES[] = {
    "clock", "calc", "snake", "notes", "files", "settings", "setup"
};
constexpr std::size_t APP_COUNT = sizeof(APP_NAMES) / sizeof(APP_NAMES[0]);

}

compositor::Window make_start_menu() {
    std::size_t x = 24;
    std::size_t y = 6;
    std::size_t width = 32;
    std::size_t height = 12;
    if (surface::graphics_available()) {
        const std::size_t screen_width = surface::pixel_width();
        const std::size_t screen_height = surface::pixel_height();
        width = screen_width > 560 ? 520 : (screen_width > 40 ? screen_width - 40 : screen_width);
        height = screen_height > 460 ? 400 : (screen_height > 96 ? screen_height - 96 : screen_height);
        x = (screen_width - width) / 2;
        y = (screen_height - height) / 2;
    }
    compositor::Window win("Start Menu", x, y, width, height);

    std::size_t line = 0;
    win.set_body_line(line++, "SELECT AN APP");
    line++; // blank line

    char buf[40];
    for (std::size_t index = 0; index < APP_COUNT; ++index) {
        const char* name = APP_NAMES[index];
        // manual itoa -- no snprintf guaranteed under -nostdlib
        buf[0] = static_cast<char>('1' + index);
        buf[1] = ' ';
        buf[2] = ' ';
        std::size_t j = 3;
        while (*name && j < sizeof(buf) - 1) buf[j++] = *name++;
        buf[j] = '\0';
        win.set_body_line(line++, buf);
    }
    line++;
    win.set_body_line(line++, "ESC - CLOSE");

    return win;
}

const char* app_for_key(char key) {
    if (key < '1' || key > static_cast<char>('1' + APP_COUNT - 1)) return nullptr;
    std::size_t index = static_cast<std::size_t>(key - '1');
    return APP_NAMES[index];
}

const char* app_for_pointer(std::size_t x, std::size_t y) {
    if (!surface::graphics_available()) return nullptr;
    const std::size_t screen_width = surface::pixel_width();
    const std::size_t screen_height = surface::pixel_height();
    const std::size_t width = screen_width > 560 ? 520
        : (screen_width > 40 ? screen_width - 40 : screen_width);
    const std::size_t height = screen_height > 460 ? 400
        : (screen_height > 96 ? screen_height - 96 : screen_height);
    const std::size_t menu_x = (screen_width - width) / 2;
    const std::size_t menu_y = (screen_height - height) / 2;
    if (x < menu_x + 18 || x >= menu_x + width - 18) return nullptr;

    for (std::size_t index = 0; index < APP_COUNT; ++index) {
        const std::size_t row_y = menu_y + 76 + (index + 2) * 31;
        if (y >= row_y - 6 && y < row_y + 22) return APP_NAMES[index];
    }
    return nullptr;
}

} // namespace desktop_shell