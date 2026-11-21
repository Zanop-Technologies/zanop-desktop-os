#pragma once
// Relative to zanop-kernel, which sits as a sibling of this repo. Avoid an
// absolute path here: the old hardcoded one broke the moment the checkout
// moved to a different drive.
#include "../../../zanop-kernel/src/types.hpp"

// Loads a custom 8x16 bitmap font into VGA text-mode hardware,
// replacing the default BIOS/CP437 glyph table. Standard technique
// (see OSDev wiki "VGA Fonts") -- reprograms the Sequencer and
// Graphics Controller registers to access font plane 2, writes the
// glyph data, then restores normal text-mode register state.
//
// NOT YET BUILD/BOOT TESTED -- register sequence written from the
// well-established technique, but like everything else in this
// project tonight, treat as "should be correct" not "confirmed
// correct" until it's actually run.

namespace vga_font {

// 256 glyphs x 16 bytes each (one byte per scanline, 8 pixels wide).
using FontData = const std::uint8_t (&)[256][16];

void load(FontData font);

} // namespace vga_font
