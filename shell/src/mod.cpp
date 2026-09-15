#include "mod.hpp"
#include "desktop.hpp"
#include "taskbar.hpp"
#include "launcher.hpp"
#include "../../compositor/src/window.hpp"
#include "../../input/src/input.hpp"
#include "../../../zanop-kernel/src/drivers/vga.hpp"
#include "../../../zanop-kernel/src/apps/apps.hpp"

namespace desktop_shell {
namespace {

constexpr std::uint8_t SC_ESC = 0x01;
constexpr std::uint8_t SC_S   = 0x1F;

void redraw(bool menu_open, compositor::Window* menu) {
    VGA::writer.clear_screen();
    draw_background();
    draw_top_bar();
    draw_bottom_bar();
    if (menu_open && menu) menu->draw();
}

} // namespace

void run() {
    VGA::init();
    draw_background();
    draw_top_bar();
    draw_bottom_bar();

    bool menu_open = false;
    compositor::Window menu = make_start_menu(); // constructed once, reused

    while (true) {
        std::uint8_t sc = input::read_scancode();

        if (sc == SC_S && !menu_open) {
            menu = make_start_menu();
            menu_open = true;
            redraw(menu_open, &menu);
            continue;
        }
        if (sc == SC_ESC && menu_open) {
            menu_open = false;
            redraw(menu_open, &menu);
            continue;
        }
        if (menu_open) {
            if (auto ascii = input::scancode_to_ascii(sc)) {
                if (const char* app = app_for_key(*ascii)) {
                    menu_open = false;
                    redraw(menu_open, &menu);
                    apps::launch(app);
                    redraw(menu_open, &menu);
                }
            }
        }
    }
}

} // namespace desktop_shell