# Zanop Desktop OS

The desktop uses the Multiboot2 RGB framebuffer when available and
falls back to VGA text mode otherwise. In graphics mode, the desktop,
launcher, windows, and built-in apps all render as pixels.

## Desktop and Apps

  to return or close.
  in-memory filesystem when you press Esc.
- Files lists stored files and previews readable text files; use `J`/`K`
  or `W`/`S` to select and Enter to preview.
- Settings switches the desktop background between Ocean and Ember for
  the current session.
  restart, and Esc to return.

## Fonts
Four real TTFs are embedded for graphics mode. **Space Mono** is the default
font throughout the desktop, apps and graphical installer. **Ubuntu Mono** is
reserved for a future Terminal; Ubuntu and Wobblydoodle remain alternate faces.
The fonts in `assets/fonts/` are rasterized into baseline-aligned 8x12 bitmaps
with fixed character spacing. The taller glyphs and preserved thin strokes
improve readability at the UI's existing explicit text scales.

The faces are selected through `surface::Font`, whose `DEFAULT_FONT` is
`Font::Mono` (Space Mono):
```cpp
enum class Font { Ui, Terminal, Mono, Wobbly };
```
Both headers are generated and checked in. Regenerate after changing a TTF:
```sh
python3 assets/fonts/build_fonts.py
python3 installer/tools/ttf_to_vga_font.py assets/fonts/MONO.ttf \
    installer/src/space_mono_font.hpp
```
The first produces `compositor/src/embedded_fonts.hpp` (8x12, graphics mode).
The second produces `installer/src/space_mono_font.hpp` (8x16) for the VGA
text-mode boot and installer path.

## Build
- Barks Lite requires 250 MB of RAM and 590 MB of storage. Barks Maximum requires 1 GB of RAM and 1.7 GB of storage.
- A terminal or command shell is not included yet.
```

Create a bootable ISO with:

```sh
make iso
```

`make iso` writes a numbered ISO to the SD card's `zanop-os-isos` folder.
The number is automatically one greater than the count of `.iso` files already
there; override `SD_ROOT` if the card is mounted elsewhere. You can set
`BUILD_NUM` explicitly to override the automatic number.
Limine boot assets are taken from `../Limine` when present, or from
`build/iso/boot/limine`; otherwise the Makefile uses GRUB tooling. The build
uses `xorriso` or `genisoimage` for ISO packaging and embeds a compact GRUB
BIOS image as the installable payload. It boots the freestanding Zanop desktop
shell. Although
the sibling kernel repository identifies modified LXQt and Budgie as supported
desktop directions, this ISO currently has no LXQt runtime integration or
Linux userspace, so this build cannot boot into LXQt yet.

## Double Buffering
All pixel drawing goes to a back buffer; `surface::present()` copies the
finished frame to the display. The desktop redraws whole frames and
presents once, and mouse motion presents after the pointer is redrawn, so
partial frames are never visible. The buffer is a 16 MB `.bss` array (NOLOAD,
so it costs nothing in the ISO) and is skipped automatically if the
bootloader grants a framebuffer larger than that.

## Installed State
The installer writes `/system/BARKS.CFG` on success. When that file is
present the desktop drops the Setup/install shortcut -- it, the Start menu
entry, and the click hit-test all read one shared tile table from
`desktop_tiles()`, so they cannot disagree -- and the first boot afterwards
shows a one-time notice that the install shortcut may be removed. The
notice is dismissed by any keypress and does not return on later boots.

Create an x86_64 VM, attach `zanop-desktop-os.iso` as its optical disk,
and boot from the optical drive. The installer can format a selected
ATA target after explicit confirmation; formatting erases that target.
Use a disposable virtual disk for testing.

## Current Limits

- PS/2 mouse input supports pointer movement and desktop clicks.
- Notes persist on a formatted ATA filesystem; without one, they stay in RAM.
- Snake uses WASD because extended arrow-key scancodes are not decoded.
- The desktop currently supports one focused app window at a time.

## System Folder

```text
/
├── system/
│   ├── commands
│   ├── programs
│   ├── icons
│   ├── fonts
│   └── engine
├── temp
├── mnt
├── external
└── home/
  ├── Desktop
  ├── Documents
  ├── Videos
  └── Music
```

The System folder is protected from deletion.

## Next Milestone

Continue validating the installer and persistent filesystem against a
disposable VHD before using any physical disk target.
