#include "sft.hpp"
#include "../../../zanop-kernel/src/drivers/vga.hpp"
// NOTE: relative path assumes the sibling-folder layout, same as
// every other cross-repo include in this project.

// DESIGN NOTE: this is NOT a separate bootable binary with its own
// entry point -- the kernel's boot.asm already handles that, and
// duplicating a second freestanding bootstrap here would be
// redundant/wrong. Instead this provides load_and_run(), meant to be
// called from the Desktop OS shell (e.g. a future "run <file.sft>"
// command) once app loading is wired in.

namespace loader {

// Parses a .sft file's bytes and, if valid, WOULD jump to its entry
// point -- except that's not actually safe yet, same caveat as the
// original Rust version of this function: jumping to `code` requires
// the memory it lives in to be marked executable, and the kernel's
// paging setup doesn't yet distinguish executable vs. non-executable
// pages (no NX bit handling anywhere), and there's no relocation or
// memory-protection logic here at all. This prints what it WOULD do
// and stops there, rather than actually executing untrusted/even
// trusted .sft code prematurely.
bool load_and_run(const std::uint8_t* data, std::size_t len) {
    sft::SftFile file;
    sft::SftError err;

    if (!sft::parse(data, len, file, err)) {
        VGA::print("Failed to parse .sft file.\n");
        return false;
    }

    VGA::print("Parsed .sft: ");
    for (std::size_t i = 0; i < file.header.app_name_len; ++i) {
        char c[2] = {file.header.app_name[i], '\0'};
        VGA::print(c);
    }
    VGA::print("\n");
    VGA::print("(NOT executing -- memory protection groundwork doesn't exist yet.)\n");

    // SAFETY: intentionally not computing/jumping to an entry pointer
    // here -- see the doc comment above. This function's job right
    // now is to prove parsing works, not to run arbitrary code.
    (void)file;

    return true;
}

} // namespace loader