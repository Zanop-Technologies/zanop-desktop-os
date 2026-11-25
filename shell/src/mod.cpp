#include "mod.hpp"
#include "desktop.hpp"
#include "taskbar.hpp"
#include "launcher.hpp"
#include "apps_gui.hpp"
#include "pointer.hpp"
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
// Modifier scancodes (make/break pairs) for the Ctrl+Alt+T terminal
// shortcut. The kernel keyboard driver ignores these, so the desktop loop
// tracks the modifier state itself.
constexpr std::uint8_t SC_CTRL_MAKE  = 0x1D;  constexpr std::uint8_t SC_CTRL_BREAK  = 0x9D;
constexpr std::uint8_t SC_ALT_MAKE   = 0x38;  constexpr std::uint8_t SC_ALT_BREAK   = 0xB8;
constexpr std::uint8_t SC_T_MAKE     = 0x14;

// The desktop tiles -- and with them the app list -- shrink when the OS is
// already installed, since the Setup/install shortcut is no longer offered.
constexpr std::size_t ICON_SPAN = 56;
void redraw(bool menu_open, compositor::Window* menu);

const char* app_at(int x, int y) {
    // Reads the same tile table the desktop painter uses, so removing the
    // install shortcut cannot desync the hit-test from what is on screen.
    std::size_t count = 0;
    const DesktopTile* tiles = desktop_tiles(count);
    for (std::size_t index = 0; index < count; ++index) {
        const int left = static_cast<int>(tiles[index].x);
        const int top = static_cast<int>(tiles[index].y);
        const int span = static_cast<int>(ICON_SPAN);
        if (x >= left - 8 && x < left + span && y >= top - 4 && y < top + 64) {
            return tiles[index].app;
        }
    }
    return nullptr;
}

void launch_app(const char* app, bool& menu_open, compositor::Window& menu) {
    menu_open = false;
    redraw(menu_open, &menu);
    if (app[0] == 's' && app[1] == 'e' && app[2] == 't' &&
        app[3] == 'u' && app[4] == 'p') {
        // Setup draws and tracks its own pointer; keep ours out of the way.
        pointer::enabled = false;
        installer::run_graphical();
        pointer::enabled = true;
        pointer::left_down = false; // Setup consumed the button events itself
    }
    else if (surface::graphics_available()) run_graphical_app(app);
    else Apps::launch(app);
    redraw(menu_open, &menu);
}

void redraw(bool menu_open, compositor::Window* menu) {
    VGA::writer.clear_screen();
    draw_background();
    draw_top_bar();
    draw_bottom_bar();
    if (menu_open && menu) menu->draw();
    // Everything above landed in the back buffer; one present() flips the
    // finished frame out, so the user never sees a half-drawn desktop.
    surface::present();
}

} // namespace

void run() {
    VGA::init();
    // Before the first paint: an installed system must not offer the install
    // shortcut, and wants the "may be removed" banner for this boot only.
    detect_installation();
    // From here on the pointer is composited after every present(), so it
    // shows (and clicks work) on the desktop and inside every app.
    pointer::center();
    pointer::install();
    draw_background();
    draw_top_bar();
    draw_bottom_bar();
    surface::present();

    bool menu_open = false;
    compositor::Window menu = make_start_menu(); // constructed once, reused
    bool ctrl_down = false;
    bool alt_down = false;

    while (true) {
        if (auto scancode = input::try_read_scancode()) {
            const std::uint8_t sc = *scancode;
            if (sc == input::SC_MOUSE_CLICK) {
                const int click_x = input::last_click_x();
                const int click_y = input::last_click_y();
                const std::size_t screen_height = surface::pixel_height();
                const bool start_clicked = click_x >= 18 && click_x < 134 &&
                    click_y >= static_cast<int>(screen_height > 56 ? screen_height - 46 : 0) &&
                    click_y < static_cast<int>(screen_height > 10 ? screen_height - 10 : screen_height);
                if (start_clicked) {
                    menu_open = !menu_open;
                    if (menu_open) menu = make_start_menu();
                    redraw(menu_open, &menu);
                } else if (menu_open) {
                    if (const char* app = app_for_pointer(
                            static_cast<std::size_t>(click_x),
                            static_cast<std::size_t>(click_y))) {
                        launch_app(app, menu_open, menu);
                    }
                } else if (const char* app = app_at(click_x, click_y)) {
                    launch_app(app, menu_open, menu);
                }
                continue;
            }
            // Track Ctrl/Alt for the Ctrl+Alt+T terminal shortcut.
            if (sc == SC_CTRL_MAKE) { ctrl_down = true; continue; }
            if (sc == SC_CTRL_BREAK) { ctrl_down = false; continue; }
            if (sc == SC_ALT_MAKE) { alt_down = true; continue; }
            if (sc == SC_ALT_BREAK) { alt_down = false; continue; }
            if (sc == SC_T_MAKE && ctrl_down && alt_down) {
                // Ctrl+Alt+T: open the Terminal app (same entry point the
                // desktop tile and Start menu use).
                launch_app("terminal", menu_open, menu);
                continue;
            }
            // Any keypress acknowledges the "install shortcut may be removed"
            // banner; it never comes back on later boots.
            if (removal_notice_visible()) {
                dismiss_removal_notice();
                redraw(menu_open, &menu);
            }
            if (sc == SC_S && !menu_open) {
                menu = make_start_menu();
                menu_open = true;
                redraw(menu_open, &menu);
            } else if (sc == SC_ESC && menu_open) {
                menu_open = false;
                redraw(menu_open, &menu);
            } else if (menu_open) {
                if (auto ascii = input::scancode_to_ascii(sc)) {
                    if (const char* app = app_for_key(*ascii)) {
                        launch_app(app, menu_open, menu);
                    }
                }
            }
        }

        __asm__ volatile("pause");
    }
}

} // namespace desktop_shell
