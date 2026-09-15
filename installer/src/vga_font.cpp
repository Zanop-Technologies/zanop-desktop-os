#include "vga_font.hpp"

namespace vga_font {
namespace {

inline void outb(std::uint16_t port, std::uint8_t val) {
    asm volatile("out %0, %1" : : "a"(val), "Nd"(port));
}

constexpr std::uint16_t SEQ_INDEX = 0x3C4;
constexpr std::uint16_t SEQ_DATA  = 0x3C5;
constexpr std::uint16_t GC_INDEX  = 0x3CE;
constexpr std::uint16_t GC_DATA   = 0x3CF;
constexpr std::uintptr_t FONT_MEM = 0xA0000; // accessible while in font-load mode

}

void load(FontData font) {
    // --- switch into font-access mode ---
    outb(SEQ_INDEX, 0x00); outb(SEQ_DATA, 0x01); // reset sequencer
    outb(SEQ_INDEX, 0x02); outb(SEQ_DATA, 0x04); // enable write to plane 2 only
    outb(SEQ_INDEX, 0x04); outb(SEQ_DATA, 0x07); // sequential addressing, disable chain-4
    outb(SEQ_INDEX, 0x00); outb(SEQ_DATA, 0x03); // restart sequencer

    outb(GC_INDEX, 0x04); outb(GC_DATA, 0x02);   // read plane 2
    outb(GC_INDEX, 0x05); outb(GC_DATA, 0x00);   // disable odd/even, write mode 0
    outb(GC_INDEX, 0x06); outb(GC_DATA, 0x00);   // map font memory at 0xA0000

    // --- write glyph data (each glyph gets a 32-byte stride; a
    // 16-row font only uses the first 16 of those 32 bytes) ---
    volatile std::uint8_t* dst = reinterpret_cast<volatile std::uint8_t*>(FONT_MEM);
    for (int glyph = 0; glyph < 256; ++glyph) {
        for (int row = 0; row < 16; ++row) {
            dst[glyph * 32 + row] = font[glyph][row];
        }
    }

    // --- restore normal text-mode register state ---
    outb(SEQ_INDEX, 0x00); outb(SEQ_DATA, 0x01);
    outb(SEQ_INDEX, 0x02); outb(SEQ_DATA, 0x03); // re-enable planes 0+1 (text mode)
    outb(SEQ_INDEX, 0x04); outb(SEQ_DATA, 0x03); // restore chain-4 addressing
    outb(SEQ_INDEX, 0x00); outb(SEQ_DATA, 0x03);

    outb(GC_INDEX, 0x04); outb(GC_DATA, 0x00);
    outb(GC_INDEX, 0x05); outb(GC_DATA, 0x10);   // restore odd/even addressing
    outb(GC_INDEX, 0x06); outb(GC_DATA, 0x0E);   // map text-mode memory back to 0xB8000
}

} // namespace vga_font
