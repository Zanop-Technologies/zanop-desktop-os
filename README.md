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

- Only the Barks Maximum profile is offered. It requires 1.5 GB of usable RAM and 5 GB of disk capacity; 8 GB is recommended.
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

Create or use an x86_64 VM, attach `zanop-desktop-os.iso` as its optical
disk, and boot from the optical drive. The kernel requests an optional
800x600x32 framebuffer; the desktop uses the actual mode returned by
the bootloader. The host must have the VirtualBox `vboxdrv` module
loaded before the VM can start.

## Current Limits

- Input is keyboard-only; there is no mouse driver yet.
- Notes are saved as `note.txt` on a mounted, formatted ATA filesystem; without one, they stay in RAM.
- The installer writes a bootable hybrid disk image to the selected ATA disk. This overwrites the entire disk; use an empty virtual disk or a disk whose contents can be erased.
- Snake uses WASD because extended arrow-key scancodes are not decoded.
- The desktop currently supports one focused app window at a time.
