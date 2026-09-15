#include "input.hpp"
#include "../../../zanop-kernel/src/drivers/keyboard.hpp"
// NOTE: relative path assumes the sibling-folder layout (zanop-kernel,
// zanop-desktop-os as siblings) -- same assumption as every other
// cross-repo include in this project. Adjust if your layout differs.
//
// NOTE: this assumes Drivers::Keyboard:: is the real current
// namespace -- unverified, see the header's comment above. If the
// real keyboard.hpp uses a different namespace or signatures, only
// the four lines below need to change.

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

} // namespace input