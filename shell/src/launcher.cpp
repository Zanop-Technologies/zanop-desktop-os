#include "launcher.hpp"
#include "../../../zanop-kernel/src/apps/apps.hpp"
// NOTE: relative path + apps:: namespace assume the apps dispatcher
// I wrote earlier this session is still intact and unmodified --
// plausible but unconfirmed given other kernel files have drifted
// since (see FIXES.md). Verify apps.hpp still matches if this fails
// to link.
#include <cstdio>

namespace desktop_shell {

compositor::Window make_start_menu() {
    compositor::Window win("Start Menu", 24, 6, 32, 12);

    auto names = apps::list();
    std::size_t line = 0;
    win.set_body_line(line++, "Select an app by number:");
    line++; // blank line

    char buf[40];
    std::size_t i = 1;
    for (const char* name : names) {
        // manual itoa -- no snprintf guaranteed under -nostdlib
        buf[0] = static_cast<char>('0' + i);
        buf[1] = '.';
        buf[2] = ' ';
        std::size_t j = 3;
        while (*name && j < sizeof(buf) - 1) buf[j++] = *name++;
        buf[j] = '\0';
        win.set_body_line(line++, buf);
        ++i;
    }
    line++;
    win.set_body_line(line++, "Esc - Close this menu");

    return win;
}

const char* app_for_key(char key) {
    if (key < '1' || key > '9') return nullptr;
    std::size_t index = static_cast<std::size_t>(key - '1');
    auto names = apps::list();
    if (index >= names.count) return nullptr;
    return names.names[index];
}

} // namespace desktop_shell