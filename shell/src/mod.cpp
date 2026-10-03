#include "mod.hpp"
#include "desktop.hpp"
#include "taskbar.hpp"
#include "launcher.hpp"
#include "apps_gui.hpp"
#include "../../compositor/src/window.hpp"
#include "../../compositor/src/surface.hpp"
#include "../../input/src/input.hpp"
#include "../../installer/src/installer.hpp"
#include "../../../zanop-kernel/src/drivers/vga.hpp"
#include "../../../zanop-kernel/src/apps/apps.hpp"

namespace desktop_shell {
namespace {

constexpr std::uint8_t SC_ESC = 0x01;
constexpr std::uint8_t SC_S   = 0x1F;
constexpr const char* DESKTOP_APPS[] = {
    "clock", "calc", "snake", "notes", "files", "settings", "setup"
};
constexpr std::size_t POINTER_WIDTH = 12;
constexpr std::size_t POINTER_HEIGHT = 16;
std::uint32_t pointer_underlay[POINTER_WIDTH * POINTER_HEIGHT];
bool pointer_visible = false;
int pointer_x = 0;
int pointer_y = 0;

void redraw(bool menu_open, compositor::Window* menu);

void erase_pointer() {
    if (!pointer_visible) return;
    for (std::size_t y = 0; y < POINTER_HEIGHT; ++y) {
        for (std::size_t x = 0; x < POINTER_WIDTH; ++x) {
            surface::draw_pixel_raw(static_cast<std::size_t>(pointer_x) + x,
                                    static_cast<std::size_t>(pointer_y) + y,
                                    pointer_underlay[y * POINTER_WIDTH + x]);
        }
    }
    pointer_visible = false;
}

void draw_pointer(int x, int y) {
    pointer_x = x;
    pointer_y = y;
    for (std::size_t row = 0; row < POINTER_HEIGHT; ++row) {
        for (std::size_t column = 0; column < POINTER_WIDTH; ++column) {
            pointer_underlay[row * POINTER_WIDTH + column] = surface::read_pixel_raw(
                static_cast<std::size_t>(x) + column,
                static_cast<std::size_t>(y) + row);
        }
    }
    surface::fill_pixel_rect(static_cast<std::size_t>(x), static_cast<std::size_t>(y),
                             12, 16, {5, 15, 22});
    surface::fill_pixel_rect(static_cast<std::size_t>(x + 2), static_cast<std::size_t>(y + 2),
                             8, 12, {244, 248, 242});
    surface::fill_pixel_rect(static_cast<std::size_t>(x + 4), static_cast<std::size_t>(y + 8),
                             4, 2, {83, 220, 206});
    pointer_visible = true;
}

const char* app_at(int x, int y) {
    if (x >= 124 && x < 188 && y >= 82 && y < 150) return DESKTOP_APPS[6];
    if (x >= 124 && x < 188 && y >= 170 && y < 238) return DESKTOP_APPS[4];
    if (x >= 124 && x < 188 && y >= 258 && y < 326) return DESKTOP_APPS[5];
    if (x < 24 || x >= 88) return nullptr;
    constexpr int ICON_TOPS[] = {86, 174, 262, 350};
    for (std::size_t index = 0; index < 4; ++index) {
        if (y >= ICON_TOPS[index] - 4 && y < ICON_TOPS[index] + 64) {
            return DESKTOP_APPS[index];
        }
    }
    return nullptr;
}

void launch_app(const char* app, bool& menu_open, compositor::Window& menu,
                int cursor_x, int cursor_y) {
    menu_open = false;
    redraw(menu_open, &menu);
    if (app[0] == 's' && app[1] == 'e' && app[2] == 't' &&
        app[3] == 'u' && app[4] == 'p') installer::run_graphical();
    else if (surface::graphics_available()) run_graphical_app(app);
    else Apps::launch(app);
    redraw(menu_open, &menu);
    (void)cursor_x;
    (void)cursor_y;
}

void redraw(bool menu_open, compositor::Window* menu) {
    pointer_visible = false;
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
    int cursor_x = static_cast<int>(surface::pixel_width() / 2);
    int cursor_y = static_cast<int>(surface::pixel_height() / 2);
    if (surface::graphics_available()) draw_pointer(cursor_x, cursor_y);
    bool left_was_down = false;

    while (true) {
        if (auto scancode = input::try_read_scancode()) {
            const std::uint8_t sc = *scancode;
            if (sc == SC_S && !menu_open) {
                menu = make_start_menu();
                menu_open = true;
                redraw(menu_open, &menu);
                if (surface::graphics_available()) draw_pointer(cursor_x, cursor_y);
            } else if (sc == SC_ESC && menu_open) {
                menu_open = false;
                redraw(menu_open, &menu);
                if (surface::graphics_available()) draw_pointer(cursor_x, cursor_y);
            } else if (menu_open) {
                if (auto ascii = input::scancode_to_ascii(sc)) {
                    if (const char* app = app_for_key(*ascii)) {
                        launch_app(app, menu_open, menu, cursor_x, cursor_y);
                        if (surface::graphics_available()) draw_pointer(cursor_x, cursor_y);
                    }
                }
            }
        }

        while (auto event = input::try_read_mouse_event()) {
            erase_pointer();
            const int max_x = surface::pixel_width() > 12
                ? static_cast<int>(surface::pixel_width() - 12) : 0;
            const int max_y = surface::pixel_height() > 16
                ? static_cast<int>(surface::pixel_height() - 16) : 0;
            const int next_x = cursor_x + (*event).delta_x;
            const int next_y = cursor_y + (*event).delta_y;
            cursor_x = next_x < 0 ? 0 : (next_x > max_x ? max_x : next_x);
            cursor_y = next_y < 0 ? 0 : (next_y > max_y ? max_y : next_y);

            const bool left_down = ((*event).buttons & 0x01) != 0;
            if (left_down && !left_was_down) {
                const std::size_t screen_height = surface::pixel_height();
                const bool start_clicked = cursor_x >= 18 && cursor_x < 134 &&
                    cursor_y >= static_cast<int>(screen_height > 56 ? screen_height - 46 : 0) &&
                    cursor_y < static_cast<int>(screen_height > 10 ? screen_height - 10 : screen_height);
                if (start_clicked) {
                    menu_open = !menu_open;
                    if (menu_open) menu = make_start_menu();
                    redraw(menu_open, &menu);
                } else if (menu_open) {
                    if (const char* app = app_for_pointer(
                            static_cast<std::size_t>(cursor_x),
                            static_cast<std::size_t>(cursor_y))) {
                        launch_app(app, menu_open, menu, cursor_x, cursor_y);
                    }
                } else if (!menu_open) {
                    if (const char* app = app_at(cursor_x, cursor_y)) {
                        launch_app(app, menu_open, menu, cursor_x, cursor_y);
                    }
                }
            }
            left_was_down = left_down;
            if (surface::graphics_available()) draw_pointer(cursor_x, cursor_y);
        }

        __asm__ volatile("pause");
    }
}

} // namespace desktop_shell