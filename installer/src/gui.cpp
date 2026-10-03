#include "installer.hpp"
#include "../../compositor/src/surface.hpp"
#include "../../input/src/input.hpp"
#include "../../../zanop-kernel/src/drivers/ata.hpp"
#include "../../../zanop-kernel/src/fs/fs.hpp"

namespace installer {
namespace {

using surface::Color;

constexpr Color NAVY{16, 30, 43};
constexpr Color DEEP{9, 19, 29};
constexpr Color PAPER{240, 244, 242};
constexpr Color WHITE{252, 253, 250};
constexpr Color INK{28, 43, 52};
constexpr Color MUTED{108, 128, 136};
constexpr Color TEAL{66, 201, 183};
constexpr Color GOLD{236, 174, 92};
constexpr Color RED{197, 73, 70};
constexpr std::size_t CURSOR_WIDTH = 10;
constexpr std::size_t CURSOR_HEIGHT = 14;

enum class Page : std::uint8_t {
    Welcome, License, Edition, Review, Target, Confirm, Formatting, Complete, Blocked
};

Page page = Page::Welcome;
bool accepted = false;
bool maximum = false;
std::size_t target_device = 0;
const char* blocked_reason = "NO IDE DISK FOUND";
int cursor_x = 32;
int cursor_y = 32;
bool cursor_visible = false;
std::uint32_t cursor_underlay[CURSOR_WIDTH * CURSOR_HEIGHT];

bool inside(int x, int y, int left, int top, int width, int height) {
    return x >= left && y >= top && x < left + width && y < top + height;
}

void draw_button(int x, int y, int width, const char* label, bool primary) {
    surface::fill_pixel_rect(x, y, width, 44, primary ? TEAL : NAVY);
    surface::draw_text(x + 16, y + 17, label, primary ? DEEP : WHITE, 1);
}

void draw_simple_page(std::size_t width, std::size_t height) {
    surface::fill_pixel_rect(0, 0, width, height, {226, 235, 235});
    surface::fill_pixel_rect(0, 0, width, 46, NAVY);
    surface::draw_text(28, 18, "ZANOP SETUP", WHITE, 1);
    surface::fill_pixel_rect(0, height > 6 ? height - 6 : 0, width, 6, TEAL);

    const std::size_t panel_width = width > 720 ? 640 : width - 48;
    const std::size_t panel_x = (width - panel_width) / 2;
    const std::size_t panel_y = 96;
    surface::fill_pixel_rect(panel_x, panel_y, panel_width, height - 194, WHITE);

    if (page == Page::Welcome) {
        surface::draw_text(panel_x + 40, panel_y + 38, "WELCOME", INK, 3);
        surface::draw_text(panel_x + 42, panel_y + 112,
                           "SET UP YOUR ZANOP COMPUTER", MUTED, 1);
        surface::fill_pixel_rect(panel_x + 42, panel_y + 156, 8, 72, TEAL);
        surface::draw_text(panel_x + 66, panel_y + 162,
                           "CHOOSE A BARKS DESKTOP EDITION", INK, 1);
        surface::draw_text(panel_x + 66, panel_y + 190,
                           "THE NEXT PAGES SHOW SYSTEM REQUIREMENTS", MUTED, 1);
        surface::draw_text(panel_x + 66, panel_y + 218,
                           "PRESS ENTER OR SELECT NEXT TO CONTINUE", MUTED, 1);
    } else {
        surface::draw_text(panel_x + 40, panel_y + 36, "LICENSE", INK, 3);
        surface::draw_text(panel_x + 42, panel_y + 108,
                           "DEVELOPMENT BUILD - PROVIDED AS IS", MUTED, 1);
        surface::draw_text(panel_x + 42, panel_y + 140,
                           "SEE LICENSE FILE FOR THE FULL TERMS", MUTED, 1);
        surface::fill_pixel_rect(panel_x + 42, panel_y + 194, 24, 24,
                                 accepted ? TEAL : PAPER);
        if (accepted) {
            surface::fill_pixel_rect(panel_x + 48, panel_y + 200, 12, 12, NAVY);
        }
        surface::draw_text(panel_x + 82, panel_y + 202,
                           "I AGREE TO THE LICENSE", INK, 1);
        surface::draw_text(panel_x + 42, panel_y + 250,
                           "PRESS SPACE TO TOGGLE", MUTED, 1);
    }

    const int button_y = static_cast<int>(height) - 94;
    if (page == Page::License) {
        draw_button(24, button_y, 120, "BACK", false);
        draw_button(static_cast<int>(width) - 180, button_y, 156,
                    accepted ? "NEXT" : "AGREE FIRST", accepted);
    } else {
        draw_button(static_cast<int>(width) - 180, button_y, 156, "NEXT", true);
    }
}

void draw_full_shell(std::size_t width, std::size_t height, const char* section) {
    surface::fill_pixel_rect(0, 0, width, height, PAPER);
    surface::fill_pixel_rect(0, 0, 196, height, NAVY);
    surface::fill_pixel_rect(0, 0, 6, height, TEAL);
    surface::draw_text(28, 24, "BARKS SETUP", WHITE, 2);
    surface::draw_text(28, 82, "INSTALL", TEAL, 1);
    surface::draw_text(28, 112, section, WHITE, 1);
    surface::fill_pixel_rect(28, 154, 140, 1, {65, 84, 96});
    surface::draw_text(28, 182, "01  WELCOME", MUTED, 1);
    surface::draw_text(28, 214, "02  LICENSE", MUTED, 1);
    surface::draw_text(28, 246, "03  EDITION", page == Page::Edition ? TEAL : MUTED, 1);
    surface::draw_text(28, 278, "04  REVIEW", page == Page::Review ? TEAL : MUTED, 1);
    surface::draw_text(28, 310, "05  INSTALL", page == Page::Blocked ? GOLD : MUTED, 1);

    surface::draw_text(232, 28, "ZANOP DESKTOP OS", INK, 1);
    surface::fill_pixel_rect(232, 52, width - 260, 1, {204, 215, 215});
}

void draw_edition(std::size_t width, std::size_t height) {
    draw_full_shell(width, height, "03  EDITION");
    surface::draw_text(232, 84, "CHOOSE YOUR BARKS GUI", INK, 2);
    surface::draw_text(232, 122, "SELECT A PROFILE FOR THIS MACHINE", MUTED, 1);

    const int card_y = 176;
    const int card_width = static_cast<int>((width - 292) / 2);
    const int left = 232;
    const int right = left + card_width + 24;
    const Color lite_border = maximum ? Color{207, 217, 216} : TEAL;
    const Color max_border = maximum ? TEAL : Color{207, 217, 216};
    surface::fill_pixel_rect(left, card_y, card_width, 238, lite_border);
    surface::fill_pixel_rect(left + 3, card_y + 3, card_width - 6, 232,
                             maximum ? WHITE : Color{237, 250, 247});
    surface::fill_pixel_rect(right, card_y, card_width, 238, max_border);
    surface::fill_pixel_rect(right + 3, card_y + 3, card_width - 6, 232,
                             maximum ? Color{237, 250, 247} : WHITE);

    surface::draw_text(left + 22, card_y + 24, "BARKS LITE", INK, 2);
    surface::draw_text(left + 22, card_y + 74, "LIGHTWEIGHT GUI", MUTED, 1);
    surface::draw_text(left + 22, card_y + 120, "250 MB RAM", INK, 2);
    surface::draw_text(left + 22, card_y + 158, "590 MB STORAGE", INK, 2);
    surface::draw_text(left + 22, card_y + 204, maximum ? "SELECT LITE" : "SELECTED", TEAL, 1);

    surface::draw_text(right + 22, card_y + 24, "BARKS MAXIMUM", INK, 2);
    surface::draw_text(right + 22, card_y + 74, "FULL GUI EXPERIENCE", MUTED, 1);
    surface::draw_text(right + 22, card_y + 120, "1 GB RAM", INK, 2);
    surface::draw_text(right + 22, card_y + 158, "1.7 GB STORAGE", INK, 2);
    surface::draw_text(right + 22, card_y + 204, maximum ? "SELECTED" : "SELECT MAX", TEAL, 1);

    const int button_y = static_cast<int>(height) - 72;
    draw_button(220, button_y, 116, "BACK", false);
    draw_button(static_cast<int>(width) - 176, button_y, 152, "REVIEW", true);
}

void draw_review(std::size_t width, std::size_t height) {
    draw_full_shell(width, height, "04  REVIEW");
    surface::draw_text(232, 84, "REVIEW REQUIREMENTS", INK, 2);
    surface::draw_text(232, 136, maximum ? "BARKS MAXIMUM GUI" : "BARKS LITE GUI", INK, 2);
    surface::draw_text(232, 190, maximum ? "MINIMUM MEMORY     1 GB RAM" : "MINIMUM MEMORY     250 MB RAM", MUTED, 1);
    surface::draw_text(232, 222, maximum ? "MINIMUM STORAGE    1.7 GB STORAGE" : "MINIMUM STORAGE    590 MB STORAGE", MUTED, 1);

    surface::fill_pixel_rect(232, 280, width - 272, 112, {255, 244, 231});
    surface::fill_pixel_rect(232, 280, 5, 112, GOLD);
    surface::draw_text(256, 304, "SEARCH FOR AN IDE INSTALL TARGET", INK, 1);
    surface::draw_text(256, 336, "THE DISK IS ONLY FORMATTED AFTER CONFIRMATION", MUTED, 1);

    const int button_y = static_cast<int>(height) - 72;
    draw_button(220, button_y, 116, "BACK", false);
    draw_button(static_cast<int>(width) - 218, button_y, 194, "CHECK INSTALL", true);
}

std::size_t required_bytes() {
    return maximum ? 1700000000ull : 590000000ull;
}

void format_number(std::uint64_t value, char* output, std::size_t capacity) {
    char reverse[24];
    std::size_t digits = 0;
    do {
        reverse[digits++] = static_cast<char>('0' + value % 10);
        value /= 10;
    } while (value != 0 && digits < sizeof(reverse));
    std::size_t index = 0;
    while (digits > 0 && index + 1 < capacity) output[index++] = reverse[--digits];
    output[index] = '\0';
}

bool target_is_large_enough() {
    const Drivers::ATA::DeviceInfo* target = Drivers::ATA::device_info(target_device);
    return target != nullptr && target->sectors <= 0xFFFFFFFFFFFFFFFFull / 512 &&
           target->sectors * 512 >= required_bytes();
}

void draw_target(std::size_t width, std::size_t height) {
    draw_full_shell(width, height, "05  DISK");
    surface::draw_text(232, 84, "SEARCH FOR A TARGET DISK", INK, 2);
    surface::draw_text(232, 122, "SELECT AN IDE DISK TO FORMAT", MUTED, 1);

    const std::size_t count = Drivers::ATA::device_count();
    for (std::size_t index = 0; index < count && index < 4; ++index) {
        const Drivers::ATA::DeviceInfo* info = Drivers::ATA::device_info(index);
        if (info == nullptr) continue;
        const int row_y = 156 + static_cast<int>(index) * 58;
        surface::fill_pixel_rect(232, row_y, width - 272, 50,
                                 index == target_device ? TEAL : Color{206, 218, 217});
        surface::fill_pixel_rect(235, row_y + 3, width - 278, 44,
                                 index == target_device ? Color{236, 250, 247} : WHITE);
        surface::draw_text(250, row_y + 12, info->model, INK, 1);
        const std::uint64_t capacity_mb = info->sectors * 512 / 1000000;
        char capacity[24];
        format_number(capacity_mb, capacity, sizeof(capacity));
        surface::draw_text(540, row_y + 12, capacity, MUTED, 1);
        surface::draw_text(540 + 6 * 5 * 1, row_y + 12, "MB", MUTED, 1);
        if (index == target_device && target_is_large_enough()) {
            surface::draw_text(static_cast<std::size_t>(width) - 150, row_y + 12,
                               "SELECTED", TEAL, 1);
        }
    }
    if (count == 0) {
        surface::draw_text(250, 174, "NO IDE DISKS FOUND", RED, 1);
    }

    const int button_y = static_cast<int>(height) - 72;
    draw_button(220, button_y, 116, "BACK", false);
    draw_button(static_cast<int>(width) - 218, button_y, 194,
                count == 0 ? "NO DISK" : "CONTINUE", count != 0);
}

void draw_confirm(std::size_t width, std::size_t height) {
    draw_full_shell(width, height, "06  FORMAT");
    surface::draw_text(232, 88, "CONFIRM DISK FORMAT", INK, 2);
    surface::fill_pixel_rect(232, 152, width - 272, 184, {255, 241, 232});
    surface::fill_pixel_rect(232, 152, 6, 184, RED);
    const Drivers::ATA::DeviceInfo* target = Drivers::ATA::device_info(target_device);
    if (target != nullptr) surface::draw_text(260, 180, target->model, INK, 1);
    surface::draw_text(260, 216, maximum ? "BARKS MAXIMUM - 1 GB RAM - 1.7 GB"
                                        : "BARKS LITE - 250 MB RAM - 590 MB", INK, 1);
    surface::draw_text(260, 256, "FORMATTING ERASES THIS VIRTUAL DISK", RED, 1);
    surface::draw_text(260, 286, "ONLY CONTINUE IF THIS IS YOUR NEW VHD", MUTED, 1);

    const int button_y = static_cast<int>(height) - 72;
    draw_button(220, button_y, 116, "BACK", false);
    draw_button(static_cast<int>(width) - 230, button_y, 206,
                target_is_large_enough() ? "FORMAT VHD" : "DISK TOO SMALL",
                target_is_large_enough());
}

void draw_progress(std::size_t width, std::size_t height, std::size_t percent) {
    draw_full_shell(width, height, "06  FORMAT");
    surface::draw_text(232, 100, "FORMATTING BARKS VOLUME", INK, 2);
    surface::draw_text(232, 156, "WRITING FILESYSTEM METADATA", MUTED, 1);
    const std::size_t bar_width = width > 320 ? width - 300 : 20;
    surface::fill_pixel_rect(232, 224, bar_width, 24, {210, 221, 220});
    surface::fill_pixel_rect(232, 224, bar_width * percent / 100, 24, TEAL);
    surface::draw_text(232, 270, percent == 0 ? "STARTING" : "WRITING DISK", INK, 1);
    (void)height;
}

void draw_complete(std::size_t width, std::size_t height) {
    draw_full_shell(width, height, "07  READY");
    surface::draw_text(232, 96, "FILESYSTEM READY", INK, 2);
    surface::fill_pixel_rect(232, 166, width - 272, 138, {232, 248, 242});
    surface::fill_pixel_rect(232, 166, 6, 138, TEAL);
    surface::draw_text(260, 194, "BARKS VOLUME FORMATTED", INK, 1);
    surface::draw_text(260, 228, "FILESYSTEM READY FOR FILES", MUTED, 1);
    surface::draw_text(260, 264, "BOOTABLE OS PAYLOAD NOT COPIED", MUTED, 1);
    surface::draw_text(232, 334, "PRESS ENTER TO RETURN TO DESKTOP", MUTED, 1);
    (void)height;
}

void draw_blocked(std::size_t width, std::size_t height) {
    draw_full_shell(width, height, "05  INSTALL");
    surface::draw_text(232, 92, "INSTALLATION PAUSED", INK, 2);
    surface::fill_pixel_rect(232, 160, width - 272, 190, WHITE);
    surface::fill_pixel_rect(232, 160, 6, 190, RED);
    surface::draw_text(260, 190, blocked_reason, RED, 1);
    surface::draw_text(260, 232, "BARKS PROFILE SELECTED", MUTED, 1);
    surface::draw_text(260, 262, maximum ? "MAXIMUM - 1 GB RAM - 1.7 GB STORAGE"
                                        : "LITE - 250 MB RAM - 590 MB STORAGE", INK, 1);
    surface::draw_text(260, 308, "CHECK THE DISK AND TRY AGAIN", MUTED, 1);
    surface::draw_text(260, 336, "PRESS Q OR USE EXIT TO DESKTOP", MUTED, 1);
    const int button_y = static_cast<int>(height) - 72;
    draw_button(220, button_y, 116, "BACK", false);
    draw_button(static_cast<int>(width) - 230, button_y, 206, "EXIT TO DESKTOP", true);
}

void draw_cursor() {
    for (std::size_t row = 0; row < CURSOR_HEIGHT; ++row) {
        for (std::size_t column = 0; column < CURSOR_WIDTH; ++column) {
            cursor_underlay[row * CURSOR_WIDTH + column] = surface::read_pixel_raw(
                static_cast<std::size_t>(cursor_x) + column,
                static_cast<std::size_t>(cursor_y) + row);
        }
    }
    surface::fill_pixel_rect(cursor_x, cursor_y, CURSOR_WIDTH, CURSOR_HEIGHT, DEEP);
    surface::fill_pixel_rect(cursor_x + 2, cursor_y + 2, 6, 10, WHITE);
    surface::fill_pixel_rect(cursor_x + 4, cursor_y + 8, 3, 2, TEAL);
    cursor_visible = true;
}

void erase_cursor() {
    if (!cursor_visible) return;
    for (std::size_t row = 0; row < CURSOR_HEIGHT; ++row) {
        for (std::size_t column = 0; column < CURSOR_WIDTH; ++column) {
            surface::draw_pixel_raw(static_cast<std::size_t>(cursor_x) + column,
                                    static_cast<std::size_t>(cursor_y) + row,
                                    cursor_underlay[row * CURSOR_WIDTH + column]);
        }
    }
    cursor_visible = false;
}

void draw_page() {
    cursor_visible = false;
    const std::size_t width = surface::pixel_width();
    const std::size_t height = surface::pixel_height();
    if (page == Page::Welcome || page == Page::License) {
        draw_simple_page(width, height);
    } else if (page == Page::Edition) {
        draw_edition(width, height);
    } else if (page == Page::Review) {
        draw_review(width, height);
    } else if (page == Page::Target) {
        draw_target(width, height);
    } else if (page == Page::Confirm) {
        draw_confirm(width, height);
    } else if (page == Page::Formatting) {
        draw_progress(width, height, 10);
    } else if (page == Page::Complete) {
        draw_complete(width, height);
    } else {
        draw_blocked(width, height);
    }
    draw_cursor();
}

bool activate_at_cursor(bool& exit_requested) {
    const std::size_t width = surface::pixel_width();
    const std::size_t height = surface::pixel_height();
    const int button_y = static_cast<int>(height) - (page == Page::Welcome || page == Page::License ? 94 : 72);

    if (page == Page::Welcome) {
        if (cursor_y >= button_y && cursor_x >= static_cast<int>(width) - 180) {
            page = Page::License;
            return true;
        }
    } else if (page == Page::License) {
        const std::size_t panel_width = width > 720 ? 640 : width - 48;
        const int panel_x = static_cast<int>((width - panel_width) / 2);
        const int panel_y = 96;
        if (inside(cursor_x, cursor_y, panel_x + 42, panel_y + 194, 260, 36)) {
            accepted = !accepted;
            return true;
        }
        if (cursor_y >= button_y && cursor_x < 144) {
            page = Page::Welcome;
            return true;
        }
        if (accepted && cursor_y >= button_y && cursor_x >= static_cast<int>(width) - 180) {
            page = Page::Edition;
            return true;
        }
    } else if (page == Page::Edition) {
        const int card_y = 176;
        const int card_width = static_cast<int>((width - 292) / 2);
        const int left = 232;
        const int right = left + card_width + 24;
        if (inside(cursor_x, cursor_y, left, card_y, card_width, 238)) {
            maximum = false;
            return true;
        }
        if (inside(cursor_x, cursor_y, right, card_y, card_width, 238)) {
            maximum = true;
            return true;
        }
        if (cursor_y >= button_y && cursor_x < 340) {
            page = Page::License;
            return true;
        }
        if (cursor_y >= button_y && cursor_x >= static_cast<int>(width) - 176) {
            page = Page::Review;
            return true;
        }
    } else if (page == Page::Review) {
        if (cursor_y >= button_y && cursor_x < 340) {
            page = Page::Edition;
            return true;
        }
        if (cursor_y >= button_y && cursor_x >= static_cast<int>(width) - 218) {
            target_device = 0;
            page = Page::Target;
            return true;
        }
    } else if (page == Page::Target) {
        const std::size_t count = Drivers::ATA::device_count();
        for (std::size_t index = 0; index < count && index < 4; ++index) {
            const int row_y = 156 + static_cast<int>(index) * 58;
            if (inside(cursor_x, cursor_y, 232, row_y, static_cast<int>(width) - 272, 50)) {
                target_device = index;
                return true;
            }
        }
        if (cursor_y >= button_y && cursor_x < 340) {
            page = Page::Review;
            return true;
        }
        if (cursor_y >= button_y && cursor_x >= static_cast<int>(width) - 218) {
            if (count == 0) {
                blocked_reason = "NO IDE DISK FOUND";
                page = Page::Blocked;
            } else if (!target_is_large_enough()) {
                blocked_reason = "TARGET DISK IS TOO SMALL";
                page = Page::Blocked;
            } else {
                page = Page::Confirm;
            }
            return true;
        }
    } else if (page == Page::Confirm) {
        if (cursor_y >= button_y && cursor_x < 340) {
            page = Page::Target;
            return true;
        }
        if (cursor_y >= button_y && cursor_x >= static_cast<int>(width) - 230 &&
            target_is_large_enough()) {
            page = Page::Formatting;
            return true;
        }
    } else if (page == Page::Formatting) {
        return false;
    } else if (page == Page::Complete) {
        if (cursor_y >= static_cast<int>(height) - 72 &&
            cursor_x >= static_cast<int>(width) - 230) {
            exit_requested = true;
        }
        return false;
    } else if (page == Page::Blocked) {
        if (cursor_y >= button_y && cursor_x < 340) {
            page = Page::Target;
            return true;
        }
        if (cursor_y >= button_y && cursor_x >= static_cast<int>(width) - 230) {
            exit_requested = true;
            return false;
        }
    }
    return false;
}

bool handle_key(std::uint8_t scancode) {
    if (scancode == 0x01) {
        if (page == Page::Welcome) return false;
        if (page == Page::License) page = Page::Welcome;
        else if (page == Page::Edition) page = Page::License;
        else if (page == Page::Review) page = Page::Edition;
        else if (page == Page::Target) page = Page::Review;
        else if (page == Page::Confirm) page = Page::Target;
        else if (page == Page::Blocked) page = Page::Target;
        else if (page == Page::Complete) return false;
        else return false;
        return true;
    }
    if (page == Page::License && scancode == 0x39) {
        accepted = !accepted;
        return true;
    }
    if (page == Page::Edition) {
        if (scancode == 0x02) { maximum = false; return true; }
        if (scancode == 0x03) { maximum = true; return true; }
    }
    if (page == Page::Target) {
        if (scancode >= 0x02 && scancode <= 0x05) {
            const std::size_t index = scancode - 0x02;
            if (index < Drivers::ATA::device_count()) {
                target_device = index;
                return true;
            }
        }
        if (scancode == 0x1C) {
            const std::size_t count = Drivers::ATA::device_count();
            if (count == 0) {
                blocked_reason = "NO IDE DISK FOUND";
                page = Page::Blocked;
            } else if (!target_is_large_enough()) {
                blocked_reason = "TARGET DISK IS TOO SMALL";
                page = Page::Blocked;
            } else {
                page = Page::Confirm;
            }
            return true;
        }
    }
    if (page == Page::Confirm && scancode == 0x1C) {
        if (target_is_large_enough()) page = Page::Formatting;
        else {
            blocked_reason = "TARGET DISK IS TOO SMALL";
            page = Page::Blocked;
        }
        return true;
    }
    if (page == Page::Complete && scancode == 0x1C) return false;
    if (scancode != 0x1C) return false;
    if (page == Page::Welcome) page = Page::License;
    else if (page == Page::License && accepted) page = Page::Edition;
    else if (page == Page::Edition) page = Page::Review;
    else if (page == Page::Review) page = Page::Target;
    else return false;
    return true;
}

void format_selected_target() {
    draw_progress(surface::pixel_width(), surface::pixel_height(), 20);
    if (!target_is_large_enough() || !FS::format(target_device)) {
        blocked_reason = "FILESYSTEM FORMAT FAILED";
        page = Page::Blocked;
        draw_page();
        return;
    }

    const char* profile = maximum
        ? "BARKS MAXIMUM\nRAM=1024MB\nSTORAGE=1700MB\n"
        : "BARKS LITE\nRAM=250MB\nSTORAGE=590MB\n";
    if (!FS::create_file("BARKS.CFG", profile)) {
        blocked_reason = "PROFILE FILE WRITE FAILED";
        page = Page::Blocked;
        draw_page();
        return;
    }

    draw_progress(surface::pixel_width(), surface::pixel_height(), 100);
    page = Page::Complete;
    draw_page();
}

} // namespace

void run_graphical() {
    if (!surface::graphics_available()) return;
    page = Page::Welcome;
    accepted = false;
    maximum = false;
    cursor_x = static_cast<int>(surface::pixel_width() / 2);
    cursor_y = static_cast<int>(surface::pixel_height() / 2);
    draw_page();

    bool left_was_down = false;
    while (true) {
        bool redraw = false;
        if (auto scancode = input::try_read_scancode()) {
            if (page == Page::Blocked && *scancode == 0x10) return;
            if (page == Page::Complete && *scancode == 0x1C) return;
            if (handle_key(*scancode)) redraw = true;
        }

        while (auto event = input::try_read_mouse_event()) {
            erase_cursor();
            const int max_x = surface::pixel_width() > CURSOR_WIDTH
                ? static_cast<int>(surface::pixel_width() - CURSOR_WIDTH) : 0;
            const int max_y = surface::pixel_height() > CURSOR_HEIGHT
                ? static_cast<int>(surface::pixel_height() - CURSOR_HEIGHT) : 0;
            const int next_x = cursor_x + (*event).delta_x;
            const int next_y = cursor_y + (*event).delta_y;
            cursor_x = next_x < 0 ? 0 : (next_x > max_x ? max_x : next_x);
            cursor_y = next_y < 0 ? 0 : (next_y > max_y ? max_y : next_y);

            const bool left_down = ((*event).buttons & 0x01) != 0;
            if (left_down && !left_was_down) {
                bool exit_requested = false;
                if (activate_at_cursor(exit_requested)) redraw = true;
                if (exit_requested) return;
            }
            left_was_down = left_down;
            if (!redraw) draw_cursor();
        }

        if (redraw) draw_page();
        if (page == Page::Formatting) format_selected_target();
        __asm__ volatile("pause");
    }
}

} // namespace installer