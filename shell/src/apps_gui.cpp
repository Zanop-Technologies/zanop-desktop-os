#include "apps_gui.hpp"
#include "desktop.hpp"
#include "taskbar.hpp"
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

AppFrame draw_frame(const char* title, const char* subtitle) {
    draw_background();
    draw_top_bar();
    draw_bottom_bar();

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
    return frame;
}

void draw_clock() {
    constexpr Color ORANGE{246, 166, 105};
    unsigned tick = 0;
    while (true) {
        AppFrame frame = draw_frame("CLOCK", "LOCAL HARDWARE TIME");
        const std::uint8_t hour = bcd_to_binary(read_rtc(0x04));
        const std::uint8_t minute = bcd_to_binary(read_rtc(0x02));
        const std::uint8_t second = bcd_to_binary(read_rtc(0x00));
        char time[9] = {
            static_cast<char>('0' + hour / 10), static_cast<char>('0' + hour % 10), ':',
            static_cast<char>('0' + minute / 10), static_cast<char>('0' + minute % 10), ':',
            static_cast<char>('0' + second / 10), static_cast<char>('0' + second % 10), '\0'
        };
        const std::size_t clock_width = 8 * 6 * 12;
        surface::draw_text(frame.x + (frame.width - clock_width) / 2,
                           frame.y + 190, time, ORANGE, 12);
        surface::fill_pixel_rect(frame.x + 80, frame.y + 350, frame.width - 160, 2,
                                 {207, 220, 223});
        surface::draw_text(frame.x + 90, frame.y + 370,
                           "TIME READ FROM CMOS REAL TIME CLOCK", INK, 1);

        for (unsigned delay = 0; delay < 300000; ++delay) {
            __asm__ volatile("pause");
        }
        auto code = input::try_read_scancode();
        if (code && *code == 0x01) return;
        if (code) input::scancode_to_ascii(*code);
        if (++tick == 4) tick = 0;
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
    const char* saved_note = FS::read_file("note.txt");
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

        std::uint8_t code = input::read_scancode();
        if (saved) return;
        if (code == 0x01) {
            note[length] = '\0';
            FS::create_file("note.txt", note);
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

    while (true) {
        AppFrame frame = draw_frame("SNAKE", "W A S D MOVE - R RESTART - ESC EXIT");
        const std::size_t cell = 18;
        const std::size_t board_width = columns * cell;
        const std::size_t board_height = rows * cell;
        const std::size_t board_x = frame.x + (frame.width - board_width) / 2;
        const std::size_t board_y = frame.y + 112;
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
        char score_label[24] = "SCORE ";
        draw_integer(static_cast<int>(score), score_label + 6, sizeof(score_label) - 6);
        surface::draw_text(frame.x + 34, frame.y + 390, score_label, INK, 1);
        if (game_over) {
            surface::fill_pixel_rect(board_x + 74, board_y + 96, board_width - 148, 64, DARK);
            surface::draw_text(board_x + 111, board_y + 122, "GAME OVER - R TO RESTART",
                               {238, 245, 248}, 1);
        }

        for (unsigned delay = 0; delay < 700000; ++delay) __asm__ volatile("pause");
        auto code = input::try_read_scancode();
        if (code && *code == 0x01) return;
        if (code) {
            auto character = input::scancode_to_ascii(*code);
            if (character && (*character == 'r' || *character == 'R') && game_over) {
                length = 3;
                snake[0] = {columns / 2, rows / 2};
                snake[1] = {columns / 2 - 1, rows / 2};
                snake[2] = {columns / 2 - 2, rows / 2};
                food = {16, 7};
                direction_x = 1;
                direction_y = 0;
                score = 0;
                game_over = false;
            } else if (character && !game_over) {
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
        if (game_over) continue;

        Point head{(snake[0].x + direction_x + columns) % columns,
                   (snake[0].y + direction_y + rows) % rows};
        for (int index = 0; index < length; ++index) {
            if (snake[index].x == head.x && snake[index].y == head.y) game_over = true;
        }
        if (game_over) continue;
        const bool ate = head.x == food.x && head.y == food.y;
        if (ate && length < max_length) {
            ++length;
            ++score;
        }
        for (int index = length - 1; index > 0; --index) snake[index] = snake[index - 1];
        snake[0] = head;
        if (ate) {
            ++step;
            food = {(head.x + 7 + static_cast<int>(step)) % columns,
                    (head.y + 5 + static_cast<int>(step * 3)) % rows};
        }
    }
}

void draw_file_manager() {
    std::size_t selected = 0;
    std::size_t first_visible = 0;
    bool previewing = false;
    bool preview_readable = false;
    char preview_name[48];
    char preview_content[256];
    std::size_t preview_size = 0;
    clear_text(preview_name, sizeof(preview_name));
    clear_text(preview_content, sizeof(preview_content));

    while (true) {
        const std::size_t count = FS::file_count();
        if (count == 0) selected = 0;
        else if (selected >= count) selected = count - 1;
        if (selected < first_visible) first_visible = selected;
        if (selected >= first_visible + 10) first_visible = selected - 9;

        AppFrame frame = draw_frame("FILES", previewing
            ? "ESC FILE LIST - ENTER CLOSE PREVIEW"
            : "J/K OR W/S SELECT - ENTER PREVIEW");
        surface::fill_pixel_rect(frame.x + 28, frame.y + 112,
                                 frame.width - 56, frame.height - 156,
                                 {250, 249, 239});

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
                               "NO FILES - SAVE A NOTE TO CREATE ONE", MUTED, 1);
        } else {
            const std::size_t visible_count = count - first_visible < 10
                ? count - first_visible : 10;
            for (std::size_t row = 0; row < visible_count; ++row) {
                const std::size_t file_index = first_visible + row;
                const char* name = FS::file_name(file_index);
                const std::size_t row_y = frame.y + 124 + row * 30;
                if (file_index == selected) {
                    surface::fill_pixel_rect(frame.x + 40, row_y - 4,
                                             frame.width - 80, 26, {217, 238, 234});
                }
                if (name != nullptr) {
                    surface::draw_text(frame.x + 54, row_y, name,
                                       file_index == selected ? INK : MUTED, 1);
                }
            }
        }

        const std::uint8_t scancode = input::read_scancode();
        if (scancode == 0x01) {
            if (previewing) previewing = false;
            else return;
            continue;
        }
        auto character = input::scancode_to_ascii(scancode);
        if (!character) continue;
        if (previewing) {
            if (*character == '\n') previewing = false;
            continue;
        }
        if (*character == 'j' || *character == 's') {
            if (count > 0 && selected + 1 < count) ++selected;
        } else if (*character == 'k' || *character == 'w') {
            if (selected > 0) --selected;
        } else if (*character == '\n' && count > 0) {
            const char* name = FS::file_name(selected);
            if (name == nullptr) continue;
            std::size_t index = 0;
            while (name[index] != '\0' && index + 1 < sizeof(preview_name)) {
                preview_name[index] = name[index];
                ++index;
            }
            preview_name[index] = '\0';
            preview_readable = FS::read_file(name,
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

        const std::uint8_t scancode = input::read_scancode();
        if (scancode == 0x01) return;
        auto character = input::scancode_to_ascii(scancode);
        if (!character) continue;
        if (*character == '1') set_warm_theme(false);
        else if (*character == '2') set_warm_theme(true);
    }
}

} // namespace

void run_graphical_app(const char* name) {
    if (name[0] == 'c' && name[1] == 'l') draw_clock();
    else if (name[0] == 'c') draw_calculator();
    else if (name[0] == 'n') draw_notes();
    else if (name[0] == 'f') draw_file_manager();
    else if (name[0] == 's' && name[1] == 'e') draw_settings();
    else if (name[0] == 's') draw_snake();
}

} // namespace desktop_shell