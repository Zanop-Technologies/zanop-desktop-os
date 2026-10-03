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

## Build

- Barks Lite requires 250 MB of RAM and 590 MB of storage. Barks Maximum requires 1 GB of RAM and 1.7 GB of storage.
- A terminal or command shell is not included yet.
```

Create a bootable ISO with:

```sh
make iso
```

The ISO is written to `~/Downloads/zanop-desktop-os.iso`. Limine boot
assets are taken from `../Limine` when present, or from
`build/iso/boot/limine`; otherwise the Makefile uses GRUB tooling. The
build also requires `grub-mkrescue` to create the hybrid disk image
embedded in the installer ISO.

## VirtualBox

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
