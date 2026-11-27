#include "input.hpp"
#include "../../../zanop-kernel/src/drivers/keyboard.hpp"
#include "../../../zanop-kernel/src/drivers/mouse.hpp"

namespace input {

namespace {
MouseSink mouse_sink = nullptr;
int click_x = 0;
int click_y = 0;

bool poll_mouse() {
    return mouse_sink != nullptr && mouse_sink(click_x, click_y);
}
} // namespace

void set_mouse_sink(MouseSink sink) { mouse_sink = sink; }
int last_click_x() { return click_x; }
int last_click_y() { return click_y; }

std::uint8_t read_scancode() {
    while (true) {
        if (auto code = Drivers::Keyboard::try_read_scancode()) return *code;
        if (poll_mouse()) return SC_MOUSE_CLICK;
        __asm__ volatile("hlt"); // any interrupt (keyboard, mouse, timer) wakes us
    }
}

std::optional<std::uint8_t> try_read_scancode() {
    if (auto code = Drivers::Keyboard::try_read_scancode()) return code;
    if (poll_mouse()) return SC_MOUSE_CLICK;
    return std::nullopt;
}

std::optional<char> scancode_to_ascii(std::uint8_t code) {
    return Drivers::Keyboard::scancode_to_ascii(code);
}

std::optional<ArrowKey> read_arrow_key() {
    return std::nullopt;
}

std::optional<MouseEvent> try_read_mouse_event() {
    auto event = Drivers::Mouse::try_read_event();
    if (!event) return std::nullopt;
    return MouseEvent{(*event).delta_x, (*event).delta_y, (*event).buttons};
}

bool mouse_available() {
    return Drivers::Mouse::is_available();
}

std::uint8_t mouse_init_error() {
    return Drivers::Mouse::init_error();
}

} // namespace input