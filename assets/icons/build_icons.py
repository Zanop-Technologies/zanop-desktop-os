from io import BytesIO
from pathlib import Path

import cairosvg
from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
ICON_DIR = ROOT / "assets" / "icons"
OUTPUT = ROOT / "shell" / "src" / "embedded_icons.hpp"
ICON_SIZE = 40
ICONS = (
    ("clock", "ICON_CLOCK_RGB"),
    ("calculator", "ICON_CALCULATOR_RGB"),
    ("snake", "ICON_SNAKE_RGB"),
    ("notes", "ICON_NOTES_RGB"),
    ("file-explorer", "ICON_FILE_EXPLORER_RGB"),
    ("launcher", "ICON_LAUNCHER_RGB"),
    ("settings", "ICON_SETUP_RGB"),
)


def render_icon(filename):
    png = cairosvg.svg2png(
        url=str(ICON_DIR / f"{filename}.svg"),
        output_width=ICON_SIZE,
        output_height=ICON_SIZE,
    )
    image = Image.open(BytesIO(png)).convert("RGBA")
    background = Image.new("RGBA", image.size, (12, 24, 36, 255))
    background.alpha_composite(image)
    return background.convert("RGB").tobytes()


def format_array(name, pixels):
    lines = []
    for offset in range(0, len(pixels), 18):
        chunk = pixels[offset:offset + 18]
        lines.append("    " + ", ".join(f"0x{value:02x}" for value in chunk))
    return f"inline constexpr unsigned char {name}[] = {{\n" + ",\n".join(lines) + "\n};"


def main():
    declarations = [format_array(symbol, render_icon(filename)) for filename, symbol in ICONS]
    header = "#pragma once\n\n"
    header += f"inline constexpr unsigned int ICON_SIZE = {ICON_SIZE};\n\n"
    OUTPUT.write_text(header + "\n\n".join(declarations) + "\n", encoding="ascii")


if __name__ == "__main__":
    main()