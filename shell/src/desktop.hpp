#pragma once
#include "../../../zanop-kernel/src/types.hpp"

namespace desktop_shell {
void draw_background();
bool warm_theme_enabled();
void set_warm_theme(bool enabled);
// The desktop icon grid, described once and consumed by both the painter
// (desktop.cpp) and the click hit-test (mod.cpp) -- so hiding the install
// shortcut can never leave the two disagreeing about where things are.
struct DesktopTile {
    const char* app;
    std::size_t x;
    std::size_t y;
};

// Rebuilt whenever the installed state changes; see desktop.cpp.
const DesktopTile* desktop_tiles(std::size_t& count);
// Every app reachable from the desktop grid, in tile order.
const char* const* desktop_apps(std::size_t& count);

// Installed-state handling for the install shortcut (see desktop.cpp).
void detect_installation();
bool install_shortcut_available();
void dismiss_removal_notice();
bool removal_notice_visible();
}