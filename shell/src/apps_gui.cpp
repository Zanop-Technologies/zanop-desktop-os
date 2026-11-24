#include "apps_gui.hpp"
#include "desktop.hpp"
#include "taskbar.hpp"
#include "legacy_shell.hpp"
#include "../../compositor/src/surface.hpp"
#include "../../input/src/input.hpp"
#include "../../../zanop-kernel/src/fs/fs.hpp"

namespace desktop_shell {
namespace {

using surface::Color;

constexpr Color DARK{12, 24, 36};
constexpr Color PANEL{237, 243, 245};
constexpr Color INK{29, 48, 58};
constexpr Color MUTED{103, 126, 136};
constexpr Color TEAL{83, 220, 206};
constexpr Color CORAL{226, 106, 94};

std::uint8_t read_rtc(std::uint8_t reg) {
    __asm__ volatile("outb %0, %1" : : "a"(reg), "Nd"(static_cast<std::uint16_t>(0x70)));
    std::uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(static_cast<std::uint16_t>(0x71)));
    return value;
}

std::uint8_t bcd_to_binary(std::uint8_t value) {
    return static_cast<std::uint8_t>((value & 0x0f) + ((value >> 4) * 10));
}

struct AppFrame {
    std::size_t x;
    std::size_t y;
    std::size_t width;
    std::size_t height;
};

std::size_t text_length(const char* text) {
    std::size_t length = 0;
    while (text[length] != '\0') ++length;
    return length;
}

void clear_text(char* text, std::size_t capacity) {
    for (std::size_t index = 0; index < capacity; ++index) text[index] = '\0';
}

bool append_path(char* output, std::size_t capacity, const char* parent,
                 const char* child) {
    std::size_t length = 0;
    while (parent[length] != '\0' && length + 1 < capacity) {
        output[length] = parent[length];
        ++length;
    }
    if (parent[length] != '\0') return false;
    if (length > 1) {
        if (length + 1 >= capacity) return false;
        output[length++] = '/';
    }
    for (std::size_t index = 0; child[index] != '\0'; ++index) {
        if (length + 1 >= capacity) return false;
        output[length++] = child[index];
    }
    output[length] = '\0';
    return true;
}

void move_to_parent(char* path) {
    std::size_t length = text_length(path);
    if (length <= 1) return;
    while (length > 1 && path[length - 1] != '/') --length;
    path[length == 1 ? 1 : length - 1] = '\0';
}

bool is_system_path(const char* path) {
    const char prefix[] = "/system/";
    if (text_length(path) == 7) {
        for (std::size_t index = 0; index < 7; ++index) {
            if (path[index] != prefix[index]) return false;
        }
        return true;
    }
    for (std::size_t index = 0; index < sizeof(prefix) - 1; ++index) {
        if (path[index] != prefix[index]) return false;
    }
    return true;
}

AppFrame draw_frame(const char* title, const char* subtitle) {
    const std::size_t screen_width = surface::pixel_width();
    const std::size_t screen_height = surface::pixel_height();
    const std::size_t width = screen_width > 700 ? 640
        : (screen_width > 48 ? screen_width - 48 : screen_width);
    const std::size_t height = screen_height > 520 ? 440
        : (screen_height > 112 ? screen_height - 112 : screen_height);
    AppFrame frame{(screen_width - width) / 2, (screen_height - height) / 2, width, height};

    surface::fill_pixel_rect(frame.x + 9, frame.y + 10, frame.width, frame.height, {7, 16, 25});
    surface::fill_pixel_rect(frame.x, frame.y, frame.width, frame.height, PANEL);
    surface::fill_pixel_rect(frame.x, frame.y, frame.width, 58, DARK);
    surface::fill_pixel_rect(frame.x, frame.y + 56, frame.width, 2, TEAL);
    surface::draw_text(frame.x + 24, frame.y + 18, title, {238, 245, 248}, 2);
    surface::draw_text(frame.x + 24, frame.y + 76, subtitle, MUTED, 1);
    surface::fill_pixel_rect(frame.x + frame.width - 36, frame.y + 20, 14, 14, CORAL);
    surface::draw_text(frame.x + 24, frame.y + frame.height - 25,
                       "ESC BACK", MUTED, 1);
    // Every app builds its whole frame through draw_frame(), so flipping
    // here is the single point that makes each app visible. Without it the
    // apps drew into the back buffer and the user saw nothing at all.
    surface::present();
    return frame;
}

void draw_clock() {
    constexpr Color ORANGE{246, 166, 105};
    // Chrome is static, so paint it once. The poll loop below runs hundreds of
    // times a second; rebuilding the whole window every one of those is what
    // made the clock feel slow and stuttery.
    AppFrame frame = draw_frame("CLOCK", "LOCAL HARDWARE TIME");
    surface::fill_pixel_rect(frame.x + 80, frame.y + 350, frame.width - 160, 2,
                             {207, 220, 223});
    surface::draw_text(frame.x + 90, frame.y + 370,
                       "TIME READ FROM CMOS REAL TIME CLOCK", INK, 1);
    surface::present();
    std::uint8_t previous_second = 0xFF;
    while (true) {
        const std::uint8_t hour = bcd_to_binary(read_rtc(0x04));
        const std::uint8_t minute = bcd_to_binary(read_rtc(0x02));
        const std::uint8_t second = bcd_to_binary(read_rtc(0x00));
        if (second != previous_second) {
            char time[9] = {
                static_cast<char>('0' + hour / 10), static_cast<char>('0' + hour % 10), ':',
                static_cast<char>('0' + minute / 10), static_cast<char>('0' + minute % 10), ':',
                static_cast<char>('0' + second / 10), static_cast<char>('0' + second % 10), '\0'
            };
            surface::fill_pixel_rect(frame.x + 32, frame.y + 176,
                                     frame.width - 64, 92, PANEL);
            surface::draw_text(frame.x + 32, frame.y + 190, time, ORANGE, 12);
            previous_second = second;
            surface::present();
        }

        for (unsigned delay = 0; delay < 300000; ++delay) {
            __asm__ volatile("pause");
        }
        auto code = input::try_read_scancode();
        if (code && *code == 0x01) return;
        if (code) input::scancode_to_ascii(*code);
    }
}

void draw_integer(int value, char* output, std::size_t capacity) {
    char reversed[16];
    std::size_t length = 0;
    bool negative = value < 0;
    unsigned magnitude = negative ? static_cast<unsigned>(-value) : static_cast<unsigned>(value);
    do {
        reversed[length++] = static_cast<char>('0' + magnitude % 10);
        magnitude /= 10;
    } while (magnitude != 0 && length < sizeof(reversed));
    if (negative && length < sizeof(reversed)) reversed[length++] = '-';
    std::size_t out = 0;
    while (length > 0 && out + 1 < capacity) output[out++] = reversed[--length];
    output[out] = '\0';
}

bool evaluate(const char* expression, char* result, std::size_t capacity) {
    int left = 0;
    int right = 0;
    char operation = 0;
    std::size_t index = 0;
    if (expression[index] == '\0') return false;
    while (expression[index] >= '0' && expression[index] <= '9') {
        left = left * 10 + expression[index++] - '0';
    }
    operation = expression[index++];
    if (operation != '+' && operation != '-' && operation != '*' && operation != '/') return false;
    if (expression[index] == '\0') return false;
    while (expression[index] >= '0' && expression[index] <= '9') {
        right = right * 10 + expression[index++] - '0';
    }
    if (expression[index] != '\0') return false;
    if (operation == '+') left += right;
    else if (operation == '-') left -= right;
    else if (operation == '*') left *= right;
    else {
        if (right == 0) return false;
        left /= right;
    }
    draw_integer(left, result, capacity);
    return true;
}

void draw_calculator() {
    char expression[32];
    char result[32];
    clear_text(expression, sizeof(expression));
    clear_text(result, sizeof(result));
    bool has_result = false;
    const char* keys[] = {"7", "8", "9", "/", "4", "5", "6", "*",
                          "1", "2", "3", "-", "0", ".", "=", "+"};
    while (true) {
        AppFrame frame = draw_frame("CALCULATOR", "TYPE AN EXPRESSION, THEN PRESS ENTER");
        surface::fill_pixel_rect(frame.x + 32, frame.y + 112, frame.width - 64, 62, {218, 231, 233});
        surface::draw_text(frame.x + 50, frame.y + 132,
                           expression[0] ? expression : "0", INK, 2);
        if (has_result) {
            surface::draw_text(frame.x + frame.width - 150, frame.y + 135, result, TEAL, 2);
        }

        const std::size_t key_width = (frame.width - 96) / 4;
        const std::size_t key_height = 42;
        for (std::size_t index = 0; index < 16; ++index) {
            const std::size_t col = index % 4;
            const std::size_t row = index / 4;
            const std::size_t x = frame.x + 32 + col * (key_width + 8);
            const std::size_t y = frame.y + 198 + row * (key_height + 8);
            const Color key_color = col == 3 ? Color{83, 180, 194} : Color{220, 231, 233};
            surface::fill_pixel_rect(x, y, key_width, key_height, key_color);
            surface::draw_text(x + key_width / 2 - 3, y + 17, keys[index], INK, 1);
        }

        surface::present();
        std::uint8_t code = input::read_scancode();
        if (code == 0x01) return;
        auto character = input::scancode_to_ascii(code);
        if (!character) continue;
        if (*character == '\n') {
            has_result = evaluate(expression, result, sizeof(result));
            if (!has_result) {
                const char* error = "INVALID OR UNDEFINED";
                for (std::size_t i = 0; error[i] != '\0'; ++i) result[i] = error[i];
                result[text_length(error)] = '\0';
                has_result = true;
            }
        } else if (*character == '\b') {
            const std::size_t length = text_length(expression);
            if (length > 0) expression[length - 1] = '\0';
            has_result = false;
        } else if (((*character >= '0' && *character <= '9') ||
                    *character == '+' || *character == '-' || *character == '*' ||
                    *character == '/') && text_length(expression) < sizeof(expression) - 1) {
            const std::size_t length = text_length(expression);
            expression[length] = *character;
            expression[length + 1] = '\0';
            has_result = false;
        }
    }
}

void draw_notes() {
    char note[256];
    clear_text(note, sizeof(note));
    std::size_t length = 0;
    const char* saved_note = FS::read_file("/home/Documents/note.txt");
    if (saved_note == nullptr) saved_note = FS::read_file("/note.txt");
    while (saved_note && saved_note[length] != '\0' && length < sizeof(note) - 1) {
        note[length] = saved_note[length];
        ++length;
    }
    bool saved = false;
    while (true) {
        AppFrame frame = draw_frame("NOTES", "TYPE A NOTE; ESC SAVES TO NOTE.TXT");
        surface::fill_pixel_rect(frame.x + 28, frame.y + 112, frame.width - 56,
                                 frame.height - 164, {250, 249, 239});

        std::size_t row = 0;
        std::size_t column = 0;
        char line[64];
        clear_text(line, sizeof(line));
        for (std::size_t index = 0; index <= length; ++index) {
            if (note[index] == '\n' || note[index] == '\0' || column == sizeof(line) - 1) {
                line[column] = '\0';
                surface::draw_text(frame.x + 46, frame.y + 130 + row * 20, line, INK, 1);
                ++row;
                column = 0;
                line[0] = '\0';
                if (note[index] == '\0' || row > 13) break;
            } else {
                line[column++] = note[index];
            }
        }
        if (!saved && row < 14) {
            surface::fill_pixel_rect(frame.x + 46 + column * 6, frame.y + 130 + row * 20,
                                     2, 10, TEAL);
        }
        if (saved) surface::draw_text(frame.x + 46, frame.y + frame.height - 55,
                                     "SAVED TO NOTE.TXT - PRESS A KEY", TEAL, 1);
        surface::present();

        std::uint8_t code = input::read_scancode();
        if (saved) return;
        if (code == 0x01) {
            note[length] = '\0';
            FS::create_file("/home/Documents/note.txt", note);
            saved = true;
            continue;
        }
        auto character = input::scancode_to_ascii(code);
        if (!character) continue;
        if (*character == '\b') {
            if (length > 0) note[--length] = '\0';
        } else if (*character == '\n') {
            if (length < sizeof(note) - 1) note[length++] = '\n';
        } else if (*character >= 32 && length < sizeof(note) - 1) {
            note[length++] = *character;
        }
    }
}

struct Point { int x; int y; };

void draw_snake() {
    constexpr int columns = 24;
    constexpr int rows = 14;
    constexpr int max_length = columns * rows;
    Point snake[max_length];
    int length = 3;
    Point food{16, 7};
    int direction_x = 1;
    int direction_y = 0;
    unsigned score = 0;
    unsigned step = 0;
    bool game_over = false;
    snake[0] = {columns / 2, rows / 2};
    snake[1] = {columns / 2 - 1, rows / 2};
    snake[2] = {columns / 2 - 2, rows / 2};

    AppFrame frame = draw_frame("SNAKE", "W A S D MOVE - R RESTART - ESC EXIT");
    constexpr std::size_t cell = 18;
    constexpr std::size_t board_width = columns * cell;
    constexpr std::size_t board_height = rows * cell;
    const std::size_t board_x = frame.x + (frame.width - board_width) / 2;
    const std::size_t board_y = frame.y + 112;
    const std::size_t score_x = frame.x + 34;
    const std::size_t score_y = frame.y + 390;

    auto draw_board = [&]() {
        surface::fill_pixel_rect(board_x - 5, board_y - 5, board_width + 10,
                                 board_height + 10, {29, 51, 62});
        surface::fill_pixel_rect(board_x, board_y, board_width, board_height, {17, 34, 43});
        for (int index = 0; index < length; ++index) {
            surface::fill_pixel_rect(board_x + snake[index].x * cell + 2,
                                     board_y + snake[index].y * cell + 2,
                                     cell - 3, cell - 3,
                                     index == 0 ? TEAL : Color{82, 167, 156});
        }
        surface::fill_pixel_rect(board_x + food.x * cell + 4, board_y + food.y * cell + 4,
                                 cell - 7, cell - 7, CORAL);
    };

    auto draw_score = [&]() {
        surface::fill_pixel_rect(score_x, score_y - 4, 160, 22, PANEL);
        char score_label[24] = "SCORE ";
        draw_integer(static_cast<int>(score), score_label + 6, sizeof(score_label) - 6);
        surface::draw_text(score_x, score_y, score_label, INK, 1);
    };

    draw_board();
    draw_score();
    while (true) {

        for (unsigned delay = 0; delay < 700000; ++delay) __asm__ volatile("pause");
        auto code = input::try_read_scancode();
        if (code && *code == 0x01) return;

        if (game_over) {
            if (code) {
                auto character = input::scancode_to_ascii(*code);
                if (character && (*character == 'r' || *character == 'R')) {
                    length = 3;
                    snake[0] = {columns / 2, rows / 2};
                    snake[1] = {columns / 2 - 1, rows / 2};
                    snake[2] = {columns / 2 - 2, rows / 2};
                    food = {16, 7};
                    direction_x = 1;
                    direction_y = 0;
                    score = 0;
                    game_over = false;
                    draw_board();
                    draw_score();
                    surface::present();
                }
            }
            continue;
        }

        if (code) {
            auto character = input::scancode_to_ascii(*code);
            if (character) {
                if ((*character == 'w' || *character == 'W') && direction_y != 1) {
                    direction_x = 0; direction_y = -1;
                } else if ((*character == 's' || *character == 'S') && direction_y != -1) {
                    direction_x = 0; direction_y = 1;
                } else if ((*character == 'a' || *character == 'A') && direction_x != 1) {
                    direction_x = -1; direction_y = 0;
                } else if ((*character == 'd' || *character == 'D') && direction_x != -1) {
                    direction_x = 1; direction_y = 0;
                }
            }
        }

        Point head{(snake[0].x + direction_x + columns) % columns,
                   (snake[0].y + direction_y + rows) % rows};
        for (int index = 0; index < length; ++index) {
            if (snake[index].x == head.x && snake[index].y == head.y) {
                game_over = true;
                break;
            }
        }
        if (game_over) {
            surface::fill_pixel_rect(board_x + 74, board_y + 96, board_width - 148, 64, DARK);
            surface::draw_text(board_x + 111, board_y + 122, "GAME OVER - R TO RESTART",
                               {238, 245, 248}, 1);
            surface::present();
            continue;
        }

        const bool ate = head.x == food.x && head.y == food.y;
        const Point old_tail = snake[length - 1];
        if (!ate) {
            surface::fill_pixel_rect(board_x + old_tail.x * cell + 2,
                                     board_y + old_tail.y * cell + 2,
                                     cell - 3, cell - 3, {17, 34, 43});
        }
        if (ate && length < max_length) {
            ++length;
            ++score;
        }
        surface::fill_pixel_rect(board_x + snake[0].x * cell + 2,
                                 board_y + snake[0].y * cell + 2,
                                 cell - 3, cell - 3, {82, 167, 156});
        for (int index = length - 1; index > 0; --index) snake[index] = snake[index - 1];
        snake[0] = head;
        surface::fill_pixel_rect(board_x + head.x * cell + 2,
                                 board_y + head.y * cell + 2,
                                 cell - 3, cell - 3, TEAL);
        // The board is fully repainted before this point (draw_board() or the
        // incremental erase/shift above), so one flip publishes the move.
        surface::present();
        if (ate) {
            surface::fill_pixel_rect(board_x + food.x * cell + 4,
                                     board_y + food.y * cell + 4,
                                     cell - 7, cell - 7, {17, 34, 43});
        }
        if (ate) {
            ++step;
            food = {(head.x + 7 + static_cast<int>(step)) % columns,
                    (head.y + 5 + static_cast<int>(step * 3)) % rows};
            surface::fill_pixel_rect(board_x + food.x * cell + 4,
                                     board_y + food.y * cell + 4,
                                     cell - 7, cell - 7, CORAL);
        }
        if (ate) {
            draw_score();
            surface::present();
        }
    }
}

void draw_file_manager() {
    std::size_t selected = 0;
    std::size_t first_visible = 0;
    bool previewing = false;
    bool preview_readable = false;
    const char* status_message = nullptr;
    char current_directory[48] = "/";
    char preview_name[48];
    char preview_path[48];
    char preview_content[256];
    std::size_t preview_size = 0;
    clear_text(preview_name, sizeof(preview_name));
    clear_text(preview_path, sizeof(preview_path));
    clear_text(preview_content, sizeof(preview_content));

    while (true) {
        const std::size_t count = FS::directory_entry_count(current_directory);
        if (count == 0) selected = 0;
        else if (selected >= count) selected = count - 1;
        if (selected < first_visible) first_visible = selected;
        if (selected >= first_visible + 8) first_visible = selected - 7;

        AppFrame frame = draw_frame("FILES", previewing
            ? "ESC FILE LIST - ENTER CLOSE PREVIEW" : current_directory);
        surface::fill_pixel_rect(frame.x + 28, frame.y + 112,
                                 frame.width - 56, frame.height - 156,
                                 {250, 249, 239});
        if (!previewing) {
            surface::draw_text(frame.x + 48, frame.y + 132,
                               "ENTER OPEN  D DELETE  ESC UP", MUTED, 1);
            if (status_message != nullptr) {
                surface::draw_text(frame.x + 48, frame.y + frame.height - 52,
                                   status_message, status_message[0] == 'S' ? CORAL : TEAL, 1);
            }
        }

        if (previewing) {
            surface::draw_text(frame.x + 48, frame.y + 128, preview_name, TEAL, 1);
            if (!preview_readable) {
                surface::draw_text(frame.x + 48, frame.y + 166,
                                   "FILE COULD NOT BE READ OR EXCEEDS 255 BYTES", CORAL, 1);
            } else {
                char line[64];
                std::size_t row = 0;
                std::size_t column = 0;
                clear_text(line, sizeof(line));
                for (std::size_t index = 0; index <= preview_size && row < 12; ++index) {
                    const char character = index == preview_size ? '\0' : preview_content[index];
                    if (character == '\n' || character == '\0' || column == sizeof(line) - 1) {
                        line[column] = '\0';
                        surface::draw_text(frame.x + 48, frame.y + 164 + row * 22,
                                           line, INK, 1);
                        ++row;
                        column = 0;
                        line[0] = '\0';
                    } else {
                        line[column++] = character;
                    }
                }
            }
        } else if (count == 0) {
            surface::draw_text(frame.x + 48, frame.y + 144,
                               "EMPTY FOLDER", MUTED, 1);
        } else {
            const std::size_t visible_count = count - first_visible < 8
                ? count - first_visible : 8;
            for (std::size_t row = 0; row < visible_count; ++row) {
                const std::size_t entry_index = first_visible + row;
                FS::DirectoryEntry entry{};
                if (!FS::directory_entry(current_directory, entry_index, entry)) continue;
                const std::size_t row_y = frame.y + 158 + row * 32;
                if (entry_index == selected) {
                    surface::fill_pixel_rect(frame.x + 40, row_y - 4,
                                             frame.width - 80, 26, {217, 238, 234});
                }
                surface::draw_text(frame.x + 54, row_y, entry.name,
                                   entry_index == selected ? INK : MUTED, 1);
                surface::draw_text(frame.x + frame.width - 136, row_y,
                                   entry.is_directory ? "FOLDER" : "FILE",
                                   entry.is_directory ? TEAL : MUTED, 1);
            }
        }

        surface::present();
        const std::uint8_t scancode = input::read_scancode();
        if (scancode == 0x01) {
            if (previewing) previewing = false;
            else if (current_directory[0] == '/' && current_directory[1] == '\0') return;
            else move_to_parent(current_directory);
            selected = 0;
            first_visible = 0;
            continue;
        }
        auto character = input::scancode_to_ascii(scancode);
        if (!character) continue;
        if (previewing) {
            if (*character == '\n' || *character == '\b') previewing = false;
            continue;
        }
        if (*character == '\b') {
            if (!(current_directory[0] == '/' && current_directory[1] == '\0')) {
                move_to_parent(current_directory);
                selected = 0;
                first_visible = 0;
            }
            continue;
        }
        if (*character == 'd' && count > 0) {
            FS::DirectoryEntry entry{};
            if (!FS::directory_entry(current_directory, selected, entry)) continue;
            char entry_path[48];
            if (!append_path(entry_path, sizeof(entry_path), current_directory, entry.name)) continue;
            const bool removed = entry.is_directory
                ? FS::remove_directory(entry_path) : FS::remove_file(entry_path);
            if (removed) {
                status_message = "ENTRY DELETED";
                if (selected > 0) --selected;
            } else if (is_system_path(entry_path)) {
                status_message = "SYSTEM FOLDER PROTECTED";
            } else {
                status_message = "FOLDER MUST BE EMPTY";
            }
            continue;
        }
        if (*character == 'j' || *character == 's') {
            if (count > 0 && selected + 1 < count) ++selected;
        } else if (*character == 'k' || *character == 'w') {
            if (selected > 0) --selected;
        } else if (*character == '\n' && count > 0) {
            FS::DirectoryEntry entry{};
            if (!FS::directory_entry(current_directory, selected, entry)) continue;
            char entry_path[48];
            if (!append_path(entry_path, sizeof(entry_path), current_directory, entry.name)) continue;
            if (entry.is_directory) {
                const std::size_t path_length = text_length(entry_path);
                for (std::size_t index = 0; index < path_length; ++index) {
                    current_directory[index] = entry_path[index];
                }
                current_directory[path_length] = '\0';
                selected = 0;
                first_visible = 0;
                status_message = nullptr;
                continue;
            }
            for (std::size_t index = 0; entry.name[index] != '\0'; ++index) {
                preview_name[index] = entry.name[index];
            }
            preview_name[text_length(entry.name)] = '\0';
            for (std::size_t index = 0; entry_path[index] != '\0'; ++index) {
                preview_path[index] = entry_path[index];
            }
            preview_path[text_length(entry_path)] = '\0';
            preview_readable = FS::read_file(preview_path,
                reinterpret_cast<std::uint8_t*>(preview_content),
                sizeof(preview_content) - 1, preview_size);
            if (preview_readable) preview_content[preview_size] = '\0';
            previewing = true;
        }
    }
}

void draw_settings() {
    while (true) {
        AppFrame frame = draw_frame("SETTINGS", "DESKTOP APPEARANCE - THIS SESSION");
        const bool warm = warm_theme_enabled();
        surface::fill_pixel_rect(frame.x + 32, frame.y + 132,
                                 frame.width - 64, 64,
                                 warm ? Color{102, 65, 63} : Color{18, 86, 91});
        surface::draw_text(frame.x + 54, frame.y + 156,
                           warm ? "CURRENT THEME: EMBER" : "CURRENT THEME: OCEAN",
                           {238, 245, 248}, 1);
        surface::draw_text(frame.x + 54, frame.y + 236,
                           "1  OCEAN", INK, 2);
        surface::draw_text(frame.x + 54, frame.y + 286,
                           "2  EMBER", INK, 2);
        surface::draw_text(frame.x + 54, frame.y + 356,
                           "THE CHANGE APPLIES WHEN YOU RETURN TO THE DESKTOP", MUTED, 1);
        surface::present();

        const std::uint8_t scancode = input::read_scancode();
        if (scancode == 0x01) return;
        auto character = input::scancode_to_ascii(scancode);
        if (!character) continue;
        if (*character == '1') set_warm_theme(false);
        else if (*character == '2') set_warm_theme(true);
    }
}

} // namespace

// -- TERMINAL -------------------------------------------------------------
// Graphical front end for the ECMXS shell (legacy_shell). Runs the same
// command engine as the text-mode terminal, but inside an app frame: each
// command is one line of input, its output is appended to a scrolling
// buffer, and the prompt is "<computer>@shell~$".
constexpr std::size_t TERM_LINES = 14;
constexpr std::size_t TERM_COLS = 74;
char term_lines[TERM_LINES][TERM_COLS];
std::size_t term_head = 0;   // index of the next line to write
std::size_t term_used = 0;   // lines currently held (up to TERM_LINES)
bool term_app_open = false;  // lets Ctrl+Alt+T in the desktop loop know

void term_push_line(const char* text) {
    // A command's output can contain embedded newlines (e.g. cat of a file
    // body); split it so every visual row is one buffer line.
    const char* start = text;
    while (*start) {
        const char* end = start;
        while (*end && *end != '\n') ++end;
        char row[TERM_COLS];
        std::size_t n = 0;
        for (const char* p = start; p != end && n + 1 < TERM_COLS; ++p) row[n++] = *p;
        row[n] = '\0';
        for (std::size_t i = 0; i <= n; ++i) term_lines[term_head][i] = row[i];
        term_head = (term_head + 1) % TERM_LINES;
        if (term_used < TERM_LINES) ++term_used;
        if (*end == '\n') ++end;
        start = end;
    }
}
void term_clear() {
    for (std::size_t r = 0; r < TERM_LINES; ++r)
        for (std::size_t c = 0; c < TERM_COLS; ++c) term_lines[r][c] = '\0';
    term_head = 0;
    term_used = 0;
}
void term_draw(const AppFrame& frame, const char* input_text,
               const char* prompt) {
    for (std::size_t r = 0; r < TERM_LINES; ++r) {
        const std::size_t row = (term_head + TERM_LINES - term_used + r) % TERM_LINES;
        surface::fill_pixel_rect(frame.x + 20, frame.y + 100 + r * 20,
                                 frame.width - 40, 18, PANEL);
        if (r < term_used) {
            surface::draw_text(frame.x + 20, frame.y + 100 + r * 20,
                               term_lines[row], INK, 1);
        }
    }
    surface::fill_pixel_rect(frame.x + 16, frame.y + frame.height - 66,
                             frame.width - 32, 34, DARK);
    surface::draw_text(frame.x + 24, frame.y + frame.height - 56, prompt, TEAL, 1);
    surface::draw_text(frame.x + 24 + surface::text_advance(1) * text_length(prompt),
                       frame.y + frame.height - 56, input_text, {238, 245, 248}, 1);
    surface::present();
}
void draw_terminal() {
    term_app_open = true;
    AppFrame frame = draw_frame("TERMINAL", "ECMXS - TYPE HLP FOR COMMANDS");
    term_clear();
    term_push_line("Zanop ECMXS terminal (type hlp)");
    legacy_shell::init();
    legacy_shell::set_emit(term_push_line);

    char input_text[TERM_COLS] = {};
    std::size_t input_len = 0;
    char prompt[16];
    auto make_prompt = [&]() {
        std::size_t p = 0;
        const char* name = legacy_shell::computer_name();
        while (*name && p < sizeof(prompt) - 1) prompt[p++] = *name++;
        const char* mid = "@shell~$ ";
        for (std::size_t i = 0; mid[i] && p < sizeof(prompt) - 1; ++i) prompt[p++] = mid[i];
        prompt[p] = '\0';
    };
    make_prompt();
    term_draw(frame, "", prompt);

    while (true) {
        const std::uint8_t scancode = input::read_scancode();
        if (scancode == 0x01) break; // ESC
        auto character = input::scancode_to_ascii(scancode);
        if (!character) continue;
        if (*character == '\b' || *character == 127) {
            if (input_len) {
                --input_len;
                input_text[input_len] = '\0';
                term_draw(frame, input_text, prompt);
            }
            continue;
        }
        if (*character == '\n' || *character == '\r') {
            if (input_len) {
                char echoed[TERM_COLS * 2];
                std::size_t e = 0;
                for (std::size_t i = 0; prompt[i] && e < sizeof(echoed) - 1; ++i) echoed[e++] = prompt[i];
                for (std::size_t i = 0; input_text[i] && e < sizeof(echoed) - 1; ++i) echoed[e++] = input_text[i];
                echoed[e] = '\0';
                term_push_line(echoed);
                legacy_shell::run_command(input_text);
                for (std::size_t i = 0; i <= input_len; ++i) input_text[i] = '\0';
                input_len = 0;
            }
            make_prompt();
            term_draw(frame, input_text, prompt);
            continue;
        }
        if (*character >= 32 && *character <= 126 &&
            input_len + 1 < sizeof(input_text)) {
            input_text[input_len++] = *character;
            input_text[input_len] = '\0';
            term_draw(frame, input_text, prompt);
        }
    }
    legacy_shell::set_emit(nullptr);
    term_app_open = false;
}

void run_graphical_app(const char* name) {
    if (name[0] == 'c' && name[1] == 'l') draw_clock();
    else if (name[0] == 'c') draw_calculator();
    else if (name[0] == 'n') draw_notes();
    else if (name[0] == 'f') draw_file_manager();
    else if (name[0] == 's' && name[1] == 'e') draw_settings();
    else if (name[0] == 's') draw_snake();
    else if (name[0] == 't') draw_terminal();
}

} // namespace desktop_shell