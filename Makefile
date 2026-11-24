# Zanop Desktop OS — Build "Alphine" (C++)
# Combines zanop-kernel's sources with Desktop OS's compositor/shell/
# input/boot/login sources into one bootable kernel+GUI binary, since
# desktop_shell::run() (or boot_screen::run() / login_screen::run(),
# once wired into a real boot sequence) is meant to replace the
# kernel's plain-text shell::run() call in main.cpp.
#
# ASSUMES:
#   - zanop-kernel and zanop-desktop-os sit as sibling folders
#   - zanop-kernel/src/main.cpp has been edited to call into whichever
#     Desktop OS entry point you want booted first (desktop_shell::run(),
#     boot_screen::run(), login_screen::run(), etc.) instead of
#     shell::run() -- this Makefile does not decide that, main.cpp does.
#   - ../limine (the Limine bootloader repo, cloned + built earlier)
#     sits alongside both of the above.
#
# FONT DEPENDENCY -- generated headers come out of scripts and TTFs in assets/fonts/:
#
#   compositor/src/embedded_fonts.hpp     (8x12, Space Mono + other faces)
#       -> assets/fonts/build_fonts.py, read by surface.cpp via
#          embedded_fonts.hpp. Needed by compositor/, shell/, installer.
#   installer/src/space_mono_font.hpp     (8x16, VGA text mode)
#       -> installer/tools/ttf_to_vga_font.py, read by vga_font::load()
#          for the text-mode boot/installer path.
#
# All of these are checked-in generated files, so a clean clone still
# builds; the rules below only regenerate when a TTF actually changes.
# To rebuild them by hand:
#   python3 assets/fonts/build_fonts.py
#   python3 installer/tools/ttf_to_vga_font.py assets/fonts/MONO.ttf \
#       installer/src/space_mono_font.hpp
#
# NOTE: neither generator may emit `#include <cstdint>` -- that header
# does not exist for -target x86_64-unknown-none in a freestanding build.
# Plain `unsigned char` needs no header and is what they emit.

CXX      := clang++
AS       := nasm
LD       := ld
ICON_PYTHON ?= python3

# --- generated font headers ---
FONT_ASSETS := assets/fonts/Ubuntu-Regular.ttf assets/fonts/UbuntuMono-Regular.ttf \
	assets/fonts/MONO.ttf assets/fonts/WOBBLYDOODLE.ttf
GLYPH_HEADER := compositor/src/embedded_fonts.hpp
VGA_FONT_HEADER := installer/src/space_mono_font.hpp

KERNEL_DIR := ../zanop-kernel
LIMINE_DIR := ../Limine
LIMINE_ASSET_DIR ?= $(if $(wildcard $(LIMINE_DIR)/limine-bios.sys),$(LIMINE_DIR),build/iso/boot/limine)
LIMINE_INSTALLER ?= $(shell command -v limine 2>/dev/null)
USE_GRUB ?= 0

TARGET   := x86_64-unknown-none
CXXFLAGS := -target $(TARGET) -mcmodel=kernel -ffreestanding -fno-exceptions -fno-rtti \
            -fno-stack-protector -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
            -Wall -Wextra -std=c++20 -I$(KERNEL_DIR)/src -I$(KERNEL_DIR)/src/shell -I.

ASFLAGS  := -f elf64
LDFLAGS  := -T $(KERNEL_DIR)/src/arch/x86_64/linker.ld -nostdlib --gc-sections

BUILD_DIR := build

# Kernel sources (everything except its own main.cpp -- we don't want
# two competing kernel_main definitions; main.cpp is included
# separately below since it's the one file that needs whichever
# Desktop OS entry-point edit applied). This `find` is recursive, so
# new kernel-side folders like src/framebuffer/ are picked up
# automatically -- no Makefile change needed when kernel-side files
# are added, only when DESKTOP-side folders are added (see below).
#
# compositor_surface.cpp is EXCLUDED deliberately: it is a stale
# copy of zanop-desktop-os/compositor/src/surface.cpp (it even
# #includes that file's header). It defines the same `surface::` and
# `memcpy` symbols as the real compositor/src/surface.cpp, so linking
# both fails with multiple-definition errors. The desktop-side file
# is the live one -- it is the copy carrying the new fonts.
KERNEL_SRC_CPP := $(shell find $(KERNEL_DIR)/src -name '*.cpp' ! -name 'main.cpp' ! -name 'compositor_surface.cpp' ! -path '*/shell/shell.cpp' ! -path '*/memory/mod.cpp')
KERNEL_SRC_MAIN := $(KERNEL_DIR)/src/main.cpp
KERNEL_SRC_ASM  := $(KERNEL_DIR)/src/arch/x86_64/boot.asm

# Desktop OS sources. NOTE: unlike KERNEL_SRC_CPP above, this list is
# explicit per-folder, not a single blanket `find .` -- that's
# deliberate, so a new top-level folder here doesn't silently get
# swept in before its dependencies (like the font header) are ready.
# Add new folders to this list by hand as they become buildable.
DESKTOP_SRC_CPP := $(shell find compositor shell input loader boot login -name '*.cpp' 2>/dev/null) installer/src/gui.cpp
ICON_HEADER := shell/src/embedded_icons.hpp
ICON_ASSETS := assets/icons/clock.svg assets/icons/calculator.svg \
	assets/icons/snake.svg assets/icons/notes.svg assets/icons/file-explorer.svg \
	assets/icons/settings.svg assets/icons/launcher.svg
# The graphical installer is included directly; the older text installer
# and its optional generated font are deliberately not part of this build.

ALL_SRC_CPP := $(KERNEL_SRC_CPP) $(KERNEL_SRC_MAIN) $(DESKTOP_SRC_CPP)

# Flatten object output paths to avoid collisions between the two repos.
KERNEL_OBJ_CPP := $(patsubst $(KERNEL_DIR)/src/%.cpp,$(BUILD_DIR)/kernel/%.o,$(KERNEL_SRC_CPP) $(KERNEL_SRC_MAIN))
DESKTOP_OBJ_CPP := $(patsubst %.cpp,$(BUILD_DIR)/desktop/%.o,$(DESKTOP_SRC_CPP))
OBJ_CPP := $(KERNEL_OBJ_CPP) $(DESKTOP_OBJ_CPP)
OBJ_ASM := $(BUILD_DIR)/boot.o

KERNEL_BIN := $(BUILD_DIR)/zanop-desktop-os.bin
PAYLOAD_ISO_ROOT := $(BUILD_DIR)/payload/iso
PAYLOAD_IMAGE := $(BUILD_DIR)/payload/zanop-os.img

# Number each build after the existing ISO files in the destination folder.
# Override SD_ROOT when the card is mounted somewhere else.
ISO_DIR       := $(BUILD_DIR)/iso
SD_ROOT     ?= /run/media/wraine/3866-3334
ISO_FOLDER    := $(SD_ROOT)/zanop-os-isos
ISO_COUNT     := $(shell if test -d "$(ISO_FOLDER)"; then find "$(ISO_FOLDER)" -maxdepth 1 -type f \( -iname '*.iso' \) | wc -l; else echo 0; fi)
BUILD_NUM    ?= $(shell expr $(ISO_COUNT) + 1)
ISO_OUT       := $(ISO_FOLDER)/$(BUILD_NUM).zanop-desktop-os.iso

.PHONY: all iso release burn clean

all: $(KERNEL_BIN)

$(KERNEL_BIN): $(OBJ_CPP) $(OBJ_ASM)
	$(LD) $(LDFLAGS) -o $@ $(OBJ_ASM) $(OBJ_CPP)

# Keep source subdirectories in object paths so same-named files from
# different modules cannot override one another's build recipes.
$(BUILD_DIR)/kernel/%.o: $(KERNEL_DIR)/src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# This legacy interactive shell moved to the desktop side as
# shell/src/legacy_shell.cpp (imported from zanop-kernel/src/shell);
# the kernel's own copy stays excluded from the desktop image.
$(BUILD_DIR)/kernel/shell/shell.o: ; @:

$(BUILD_DIR)/desktop/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(ICON_HEADER): assets/icons/build_icons.py assets/icons/requirements.txt $(ICON_ASSETS)
	$(ICON_PYTHON) assets/icons/build_icons.py

$(GLYPH_HEADER): assets/fonts/build_fonts.py $(FONT_ASSETS)
	$(ICON_PYTHON) assets/fonts/build_fonts.py

$(VGA_FONT_HEADER): installer/tools/ttf_to_vga_font.py assets/fonts/MONO.ttf
	$(ICON_PYTHON) installer/tools/ttf_to_vga_font.py assets/fonts/MONO.ttf $@

# Recompile the compositor whenever its generated glyph data changes.
$(BUILD_DIR)/desktop/compositor/src/surface.o: $(GLYPH_HEADER)
$(BUILD_DIR)/kernel/installer_installer.o: | $(VGA_FONT_HEADER)

$(BUILD_DIR)/desktop/shell/src/desktop.o $(BUILD_DIR)/desktop/shell/src/taskbar.o: $(ICON_HEADER)

$(BUILD_DIR)/boot.o: $(KERNEL_DIR)/src/arch/x86_64/boot.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) $(ASFLAGS) $< -o $@

# Build a compact BIOS GRUB payload for the installer to copy to ATA.
$(PAYLOAD_IMAGE): $(KERNEL_BIN) Makefile
	@mkdir -p $(PAYLOAD_ISO_ROOT)/boot/grub
	cp $(KERNEL_BIN) $(PAYLOAD_ISO_ROOT)/boot/kernel.bin
	printf 'set timeout=0\nset default=0\nmenuentry "Zanop Desktop OS" {\n    multiboot2 /boot/kernel.bin\n    boot\n}\n' > $(PAYLOAD_ISO_ROOT)/boot/grub/grub.cfg
	grub-mkimage -O i386-pc -o $@ -c $(PAYLOAD_ISO_ROOT)/boot/grub/grub.cfg \
	-C xz -p '(hd0)/boot/grub' biosdisk iso9660 multiboot2 normal configfile

# --- ISO packaging ---
# CHANGED from the earlier version: no more `video_mode: text` line.
# That was forcing Limine into legacy text mode, which directly
# conflicts with the framebuffer request now in boot.asm. Omitting the
# key lets Limine pick a graphics mode by default (matches the actual
# observed default behavior from earlier boot testing) -- this does
# NOT hardcode a specific alternate video_mode value, since guessing
# an unverified config key was exactly what caused wasted rebuild
# cycles earlier in this project (see FIXES.md).
iso: $(KERNEL_BIN) $(PAYLOAD_IMAGE)
	@rm -rf $(ISO_DIR)
	@mkdir -p $(ISO_DIR)/boot/limine
	@mkdir -p $(dir $(ISO_OUT))
	cp $(KERNEL_BIN) $(ISO_DIR)/boot/kernel.bin
	cp $(PAYLOAD_IMAGE) $(ISO_DIR)/boot/zanop-payload.img
	@if test "$(USE_GRUB)" = "1"; then \
	   mkdir -p $(ISO_DIR)/boot/grub; \
	   printf 'set timeout=0\nset default=0\nmenuentry "Zanop Desktop OS" {\n    multiboot2 /boot/kernel.bin\n    module2 /boot/zanop-payload.img ZANOP_PAYLOAD\n    boot\n}\n' > $(ISO_DIR)/boot/grub/grub.cfg; \
	   if command -v xorriso >/dev/null 2>&1; then \
	      grub-mkrescue -o "$(ISO_OUT)" $(ISO_DIR) || exit $$?; \
	   elif command -v genisoimage >/dev/null 2>&1 && command -v grub-mkimage >/dev/null 2>&1; then \
	      grub-mkimage -O i386-pc-eltorito -o $(ISO_DIR)/boot/grub/eltorito.img \
	         -c $(ISO_DIR)/boot/grub/grub.cfg -C xz -p '(cd)/boot/grub' \
	         biosdisk iso9660 multiboot2 normal configfile || exit $$?; \
	      genisoimage -R -r -J -b boot/grub/eltorito.img -no-emul-boot \
	         -boot-load-size 4 -boot-info-table -c boot/grub/boot.cat \
	         -o "$(ISO_OUT)" $(ISO_DIR) || exit $$?; \
	   else \
	      echo "ERROR: GRUB CD packaging requires xorriso or genisoimage plus grub-mkimage."; exit 1; \
	   fi; \
	elif test -f "$(LIMINE_ASSET_DIR)/limine-bios.sys" && \
	   test -f "$(LIMINE_ASSET_DIR)/limine-bios-cd.bin" && \
	   test -f "$(LIMINE_ASSET_DIR)/limine-uefi-cd.bin"; then \
	   if test "$(LIMINE_ASSET_DIR)" != "$(ISO_DIR)/boot/limine"; then \
	      cp "$(LIMINE_ASSET_DIR)/limine-bios.sys" "$(LIMINE_ASSET_DIR)/limine-bios-cd.bin" \
	         "$(LIMINE_ASSET_DIR)/limine-uefi-cd.bin" $(ISO_DIR)/boot/limine/; \
	   fi; \
	   printf 'timeout: 0\n\n/Zanop Desktop OS\n    protocol: multiboot2\n    kernel_path: boot():/boot/kernel.bin\n    module_path: boot():/boot/zanop-payload.img\n    module_string: ZANOP_PAYLOAD\n' \
	      > $(ISO_DIR)/boot/limine/limine.conf; \
	   if ! command -v xorriso >/dev/null 2>&1; then \
	      echo "ERROR: xorriso is required for a Limine BIOS/UEFI ISO; refusing a non-bootable fallback."; exit 1; \
	   fi; \
	   xorriso -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin \
	      -no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus \
	      -apm-block-size 2048 \
	      --efi-boot boot/limine/limine-uefi-cd.bin \
	      -efi-boot-part --efi-boot-image --protective-msdos-label \
	      $(ISO_DIR) -o "$(ISO_OUT)" || exit $$?; \
	   if test -n "$(LIMINE_INSTALLER)" && test -x "$(LIMINE_INSTALLER)"; then \
	      "$(LIMINE_INSTALLER)" bios-install "$(ISO_OUT)" || exit $$?; \
	   else \
	      echo "ERROR: Limine BIOS installer is required to make the hybrid ISO BIOS-bootable."; exit 1; \
	   fi; \
	else \
	   echo "ERROR: Limine boot assets are missing; set USE_GRUB=1 to explicitly use GRUB."; exit 1; \
	fi
	@echo "ISO written to $(ISO_OUT)"

# --- Release: same as iso, just the explicit name for "this is the
# artifact that gets published / burned to CD-R" ---
release: iso
	@echo "Release ISO ready at $(ISO_OUT)"
	@echo "To burn to a CD-R, run: make burn DEVICE=/dev/sr0"
	@echo "(check your actual optical drive device first -- see the burn target's own warning)"

# --- Burning to physical media ---
# DELIBERATELY not run automatically by `release` -- writing to the
# wrong block device destroys data on whatever's actually there, and
# DEVICE has no safe default to assume. You MUST pass the real device
# explicitly: make burn DEVICE=/dev/sr0
DEVICE ?=

burn: $(ISO_OUT)
ifeq ($(DEVICE),)
	@echo "ERROR: no DEVICE given."
	@echo "Find your optical drive with: lsblk -o NAME,MODEL,TYPE | grep rom"
	@echo "Then run: make burn DEVICE=/dev/sr0   (use your real device path)"
	@exit 1
else
	@echo "About to burn $(ISO_OUT) to $(DEVICE)."
	@echo "This will ERASE the disc if it's rewritable, or fail if it's not blank."
	@echo "Press Ctrl+C now if $(DEVICE) is not correct."
	@sleep 5
	wodim -v dev=$(DEVICE) $(ISO_OUT)
endif

clean:
	rm -rf $(BUILD_DIR)
