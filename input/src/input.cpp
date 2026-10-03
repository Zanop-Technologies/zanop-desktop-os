#include "input.hpp"
#include "../../../zanop-kernel/src/drivers/keyboard.hpp"
#include "../../../zanop-kernel/src/drivers/mouse.hpp"

namespace input {

std::uint8_t read_scancode() {
    return Drivers::Keyboard::read_scancode();
}

std::optional<std::uint8_t> try_read_scancode() {
    return Drivers::Keyboard::try_read_scancode();
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