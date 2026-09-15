#!/usr/bin/env python3
"""
Converts a monospace TTF (e.g. Space Mono) into an 8x16 VGA bitmap
font header (space_mono_font.hpp), for use with vga_font::load().

Requires: pip install Pillow

Usage:
    python3 ttf_to_vga_font.py SpaceMono-Regular.ttf space_mono_font.hpp
"""
import sys
from PIL import Image, ImageDraw, ImageFont

GLYPH_W = 8
GLYPH_H = 16

def rasterize_glyph(font, codepoint):
    img = Image.new("1", (GLYPH_W, GLYPH_H), 0)
    draw = ImageDraw.Draw(img)
    try:
        ch = chr(codepoint)
        draw.text((0, 0), ch, font=font, fill=1)
    except Exception:
        pass  # leave blank for glyphs the font doesn't have
    rows = []
    for y in range(GLYPH_H):
        byte = 0
        for x in range(GLYPH_W):
            if img.getpixel((x, y)):
                byte |= (1 << (7 - x))
        rows.append(byte)
    return rows

def main():
    if len(sys.argv) != 3:
        print(__doc__)
        sys.exit(1)

    ttf_path, out_path = sys.argv[1], sys.argv[2]
    # Size chosen to roughly fill an 8x16 cell; adjust if glyphs look
    # too cramped or too large once you actually see it rendered.
    font = ImageFont.truetype(ttf_path, 14)

    with open(out_path, "w") as f:
        f.write("#pragma once\n")
        f.write("#include <cstdint>\n\n")
        f.write("// Auto-generated from a real TTF by ttf_to_vga_font.py.\n")
        f.write("// Do not hand-edit -- regenerate from the source font instead.\n\n")
        f.write("namespace vga_font {\n\n")
        f.write("inline const std::uint8_t SPACE_MONO_FONT[256][16] = {\n")

        for cp in range(256):
            rows = rasterize_glyph(font, cp)
            row_str = ", ".join(f"0x{b:02X}" for b in rows)
            f.write(f"    {{{row_str}}}, // glyph {cp}\n")

        f.write("};\n\n")
        f.write("} // namespace vga_font\n")

    print(f"Wrote {out_path}")

if __name__ == "__main__":
    main()
