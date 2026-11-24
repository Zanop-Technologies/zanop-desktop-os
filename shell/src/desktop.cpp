#include "desktop.hpp"
#include "../../compositor/src/surface.hpp"
#include "embedded_icons.hpp"
#include "../../../zanop-kernel/src/fs/fs.hpp"

namespace desktop_shell {

namespace {

constexpr surface::Color INK{13, 25, 39};
constexpr surface::Color CYAN{83, 220, 206};
constexpr surface::Color WHITE{238, 245, 248};
constexpr surface::Color MUTED{165, 184, 193};
bool warm_theme = false;

// The installer writes /system/BARKS.CFG when it finishes formatting, so the
// presence of that file is what "already installed" means to the desktop.
// Once it is there the Setup shortcut is pointless -- re-running it would
// reformat the disk -- so we stop offering it.
bool os_installed = false;
bool install_shortcut_removed = false;

// One-time banner. The request is "after reboot it says the install shortcut
// is allowed to be removed" -- so the first boot after installing explains
// it, and every later boot stays quiet.
bool removal_notice_pending = false;

void detect_install_state() {
    os_installed = FS::read_file("/system/BARKS.CFG") != nullptr;
    install_shortcut_removed = os_installed;
    removal_notice_pending = os_installed;
}

// Both layouts. Column 0 always holds the four general apps; column 1 holds
// Setup (the install shortcut) first, and drops that row once installed so
// nothing moves underneath the user without reason.
constexpr DesktopTile TILES_NOT_INSTALLED[] = {
    {"clock",    32,  86},
    {"calc",     32, 174},
    {"snake",    32, 262},
    {"notes",    32, 350},
    {"setup",   132,  86},   // install shortcut
    {"files",   132, 174},
    {"settings",132, 262},
    {"terminal",132, 350},
};
constexpr DesktopTile TILES_INSTALLED[] = {
    {"clock",    32,  86},
    {"calc",     32, 174},
    {"snake",    32, 262},
    {"notes",    32, 350},
    {"files",   132,  86},
    {"settings",132, 174},
    {"terminal",132, 262},
};

// Icon + label for one app, matched by the same string the tile table uses.
// Setup and Settings deliberately share an icon in this build, as they did
// before the installed-state change.
const std::uint8_t* icon_for_app(const char* app) {
    if (app[0] == 'c' && app[1] == 'l') return ICON_CLOCK_RGB;
    if (app[0] == 'c') return ICON_CALCULATOR_RGB;
    if (app[0] == 's' && app[1] == 'n') return ICON_SNAKE_RGB;
    if (app[0] == 'n') return ICON_NOTES_RGB;
    if (app[0] == 'f') return ICON_FILE_EXPLORER_RGB;
    if (app[0] == 't') return ICON_LAUNCHER_RGB;
    return ICON_SETUP_RGB;
}

const char* label_for_app(const char* app) {
    if (app[0] == 'c' && app[1] == 'l') return "CLOCK";
    if (app[0] == 'c') return "CALC";
    if (app[0] == 's' && app[1] == 'n') return "SNAKE";
    if (app[0] == 'n') return "NOTES";
    if (app[0] == 'f') return "FILES";
    if (app[0] == 't') return "TERMINAL";
    if (app[0] == 's' && app[1] == 'e') return "SETUP";
    return "SETTINGS";
}

void draw_removal_notice(std::size_t width) {
    if (!removal_notice_pending) return;
    const std::size_t notice_height = 58;
    const std::size_t notice_y = surface::pixel_height() > 120
        ? surface::pixel_height() - notice_height - 30 : 4;
    surface::fill_pixel_rect(24, notice_y, width > 520 ? width - 48 : width, notice_height,
                             {18, 34, 46});
    surface::fill_pixel_rect(24, notice_y, 5, notice_height, CYAN);
    surface::draw_text(44, notice_y + 12,
                       "ZANOP DESKTOP OS IS INSTALLED", WHITE, 1);
    surface::draw_text(44, notice_y + 32,
                       "THE INSTALL SHORTCUT MAY BE REMOVED - THIS NOTICE WILL NOT SHOW AGAIN",
                       MUTED, 1);
}

void draw_app_icon(std::size_t x, std::size_t y, const std::uint8_t* pixels,
                   const char* label) {
    surface::draw_rgb_bitmap(x, y, ICON_SIZE, ICON_SIZE, pixels);
    surface::draw_text(x, y + ICON_SIZE + 5, label, WHITE, 1);
}

void draw_graphical_background() {
    const std::size_t width = surface::pixel_width();
    const std::size_t height = surface::pixel_height();
    for (std::size_t y = 0; y < height; y += 4) {
        const std::size_t blend = height == 0 ? 0 : y * 255 / height;
        const std::uint8_t red = static_cast<std::uint8_t>(warm_theme ? 46 + blend / 3 : 12 + blend / 7);
        const std::uint8_t green = static_cast<std::uint8_t>(warm_theme ? 25 + blend / 4 : 38 + blend / 2);
        const std::uint8_t blue = static_cast<std::uint8_t>(warm_theme ? 38 + blend / 5 : 59 + blend / 2);
        surface::fill_pixel_rect(0, y, width, 4, {red, green, blue});
    }

    const std::size_t horizon = height * 3 / 5;
    for (std::size_t y = horizon; y < height; y += 5) {
        const std::size_t offset = (y - horizon) / 5;
        surface::fill_pixel_rect(width / 3 + offset * 2, y,
                                 width - (width / 3 + offset * 2), 5,
                                 warm_theme
                                     ? surface::Color{102, static_cast<std::uint8_t>(65 + offset / 3), 63}
                                     : surface::Color{18, static_cast<std::uint8_t>(86 + offset / 2), 91});
    }

    const std::size_t sun_x = width * 3 / 4;
    const std::size_t sun_y = height / 3;
    const std::size_t radius = height / 9;
    for (std::size_t dy = 0; dy < radius * 2; dy += 3) {
        const std::size_t relative_y = dy > radius ? dy - radius : radius - dy;
        const std::size_t span = radius * radius > relative_y * relative_y
            ? radius - relative_y / 2 : 0;
        if (span > 0) {
            surface::fill_pixel_rect(sun_x - span, sun_y + dy - radius, span * 2, 3,
                                     {246, 166, 105});
        }
    }

    std::size_t tile_count = 0;
    const DesktopTile* tiles = desktop_tiles(tile_count);
    for (std::size_t index = 0; index < tile_count; ++index) {
        draw_app_icon(tiles[index].x, tiles[index].y,
                      icon_for_app(tiles[index].app),
                      label_for_app(tiles[index].app));
    }
    surface::fill_pixel_rect(0, height > 0 ? height - 2 : 0, width, 2, CYAN);
    surface::draw_text(width > 170 ? width - 170 : 8, height > 28 ? height - 26 : 8,
                       "DESKTOP 01", MUTED, 1);
    draw_removal_notice(width);
    (void)INK;
}

} // namespace

bool warm_theme_enabled() {
    return warm_theme;
}

bool install_shortcut_available() {
    return !install_shortcut_removed;
}

// Called once at shell startup, before the first background draw.
void detect_installation() {
    detect_install_state();
}

// The notice is shown on the first frame after boot; the caller dismisses it
// (e.g. on the first key or click) so it does not sit there forever.
void dismiss_removal_notice() {
    removal_notice_pending = false;
}

bool removal_notice_visible() {
    return removal_notice_pending;
}

void set_warm_theme(bool enabled) {
    warm_theme = enabled;
}

const DesktopTile* desktop_tiles(std::size_t& count) {
    if (install_shortcut_removed) {
        count = sizeof(TILES_INSTALLED) / sizeof(TILES_INSTALLED[0]);
        return TILES_INSTALLED;
    }
    count = sizeof(TILES_NOT_INSTALLED) / sizeof(TILES_NOT_INSTALLED[0]);
    return TILES_NOT_INSTALLED;
}

const char* const* desktop_apps(std::size_t& count) {
    // Both tables are column-major within a column, but the caller only ever
    // needs "every app, once each" -- tile order is what the pointer hit-test
    // in mod.cpp walks.
    static const char* not_installed[] = {
        "clock", "calc", "snake", "notes", "setup", "files", "settings", "terminal"
    };
    static const char* installed[] = {
        "clock", "calc", "snake", "notes", "files", "settings", "terminal"
    };
    if (install_shortcut_removed) {
        count = sizeof(installed) / sizeof(installed[0]);
        return installed;
    }
    count = sizeof(not_installed) / sizeof(not_installed[0]);
    return not_installed;
}

void draw_background() {
    if (surface::graphics_available()) {
        draw_graphical_background();
        return;
    }

    surface::fill_rect(0, 0, surface::SCREEN_WIDTH, surface::SCREEN_HEIGHT, ' ');

    for (std::size_t row = 0; row < surface::SCREEN_HEIGHT; ++row) {
        for (std::size_t col = 0; col < surface::SCREEN_WIDTH; ++col) {
            const bool accent = ((row + col) % 5 == 0) || ((row * 2 + col) % 9 == 0);
            const char glyph = accent ? '\xdb' : ' ';
            surface::put_char(col, row, glyph);
        }
    }

    surface::fill_rect(4, 3, 18, 7, ' ');
    for (std::size_t row = 3; row < 10; ++row) {
        surface::put_char(5, row, '\xb3');
        surface::put_char(18, row, '\xb3');
    }
    surface::put_char(4, 3, '\xda');
    surface::put_char(21, 3, '\xbf');
    surface::put_char(4, 9, '\xc0');
    surface::put_char(21, 9, '\xd9');

    for (std::size_t col = 5; col < 21; ++col) {
        surface::put_char(col, 3, '\xc4');
        surface::put_char(col, 9, '\xc4');
    }

    surface::put_str_clipped(7, 5, "Home", 8);
    surface::put_str_clipped(7, 6, "Documents", 12);
    surface::put_str_clipped(7, 7, "Files", 8);
}

} // namespace desktop_shell