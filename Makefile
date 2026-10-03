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
# FONT DEPENDENCY -- installer/, boot/, and login/ ALL depend on
# space_mono_font.hpp, which only exists after running
# installer/tools/ttf_to_vga_font.py against a real Space Mono .ttf.
# If that header doesn't exist yet, building any of these three
# folders will fail. Generate it first:
#   python3 installer/tools/ttf_to_vga_font.py SpaceMono-Regular.ttf \
#       installer/src/space_mono_font.hpp
# (boot/ and login/ both #include it via a relative path into
# installer/src/ -- see their source files. If you'd rather each folder
# have its own copy, that's a small path change in boot_screen.cpp and
# login_screen.cpp, not a Makefile change.)

CXX      := clang++
AS       := nasm
LD       := ld
ICON_PYTHON ?= python3

KERNEL_DIR := ../zanop-kernel
LIMINE_DIR := ../Limine
LIMINE_ASSET_DIR ?= $(if $(wildcard $(LIMINE_DIR)/limine-bios.sys),$(LIMINE_DIR),build/iso/boot/limine)
LIMINE_INSTALLER ?= $(if $(wildcard $(LIMINE_DIR)/limine),$(LIMINE_DIR)/limine,$(shell command -v limine 2>/dev/null))

TARGET   := x86_64-unknown-none
CXXFLAGS := -target $(TARGET) -mcmodel=kernel -ffreestanding -fno-exceptions -fno-rtti \
            -fno-stack-protector -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
            -Wall -Wextra -std=c++20 -I$(KERNEL_DIR)/src -I.

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
KERNEL_SRC_CPP := $(shell find $(KERNEL_DIR)/src -name '*.cpp' ! -name 'main.cpp' ! -path '*/memory/mod.cpp')
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

# Where the final ISO ends up.
DOWNLOADS_DIR := $(HOME)/Downloads
ISO_DIR       := $(BUILD_DIR)/iso
ISO_OUT       := $(DOWNLOADS_DIR)/zanop-desktop-os.iso

.PHONY: all iso release burn clean

all: $(KERNEL_BIN)

$(KERNEL_BIN): $(OBJ_CPP) $(OBJ_ASM)
	$(LD) $(LDFLAGS) -o $@ $(OBJ_ASM) $(OBJ_CPP)

# Keep source subdirectories in object paths so same-named files from
# different modules cannot override one another's build recipes.
$(BUILD_DIR)/kernel/%.o: $(KERNEL_DIR)/src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/desktop/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(ICON_HEADER): assets/icons/build_icons.py assets/icons/requirements.txt $(ICON_ASSETS)
	$(ICON_PYTHON) assets/icons/build_icons.py

$(BUILD_DIR)/desktop/shell/src/desktop.o $(BUILD_DIR)/desktop/shell/src/taskbar.o: $(ICON_HEADER)

$(BUILD_DIR)/boot.o: $(KERNEL_DIR)/src/arch/x86_64/boot.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) $(ASFLAGS) $< -o $@

# Build an ISO-hybrid disk image that can be copied verbatim to an ATA disk.
$(PAYLOAD_IMAGE): $(KERNEL_BIN) Makefile
	@mkdir -p $(PAYLOAD_ISO_ROOT)/boot/grub
	cp $(KERNEL_BIN) $(PAYLOAD_ISO_ROOT)/boot/kernel.bin
	printf 'set timeout=0\nset default=0\nmenuentry "Zanop Desktop OS" {\n    multiboot2 /boot/kernel.bin\n    boot\n}\n' > $(PAYLOAD_ISO_ROOT)/boot/grub/grub.cfg
	grub-mkrescue -o $@ $(PAYLOAD_ISO_ROOT)

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
	@mkdir -p $(ISO_DIR)/boot/limine
	@mkdir -p $(DOWNLOADS_DIR)
	cp $(KERNEL_BIN) $(ISO_DIR)/boot/kernel.bin
	cp $(PAYLOAD_IMAGE) $(ISO_DIR)/boot/zanop-payload.img
	@if test -f "$(LIMINE_ASSET_DIR)/limine-bios.sys" && \
	   test -f "$(LIMINE_ASSET_DIR)/limine-bios-cd.bin" && \
	   test -f "$(LIMINE_ASSET_DIR)/limine-uefi-cd.bin"; then \
	   if test "$(LIMINE_ASSET_DIR)" != "$(ISO_DIR)/boot/limine"; then \
	      cp "$(LIMINE_ASSET_DIR)/limine-bios.sys" "$(LIMINE_ASSET_DIR)/limine-bios-cd.bin" \
	         "$(LIMINE_ASSET_DIR)/limine-uefi-cd.bin" $(ISO_DIR)/boot/limine/; \
	   fi; \
	   printf 'timeout: 0\n\n/Zanop Desktop OS\n    protocol: multiboot2\n    kernel_path: boot():/boot/kernel.bin\n    module_path: boot():/boot/zanop-payload.img\n    module_string: ZANOP_PAYLOAD\n' \
	      > $(ISO_DIR)/boot/limine/limine.conf; \
	   xorriso -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin \
	      -no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus \
	      -apm-block-size 2048 \
	      --efi-boot boot/limine/limine-uefi-cd.bin \
	      -efi-boot-part --efi-boot-image --protective-msdos-label \
	      $(ISO_DIR) -o "$(ISO_OUT)"; \
	   if test -n "$(LIMINE_INSTALLER)" && test -x "$(LIMINE_INSTALLER)"; then \
	      "$(LIMINE_INSTALLER)" bios-install "$(ISO_OUT)"; \
	   else \
	      echo "Limine BIOS installer not found; using the Limine CD boot images as-is."; \
	   fi; \
	else \
	   mkdir -p $(ISO_DIR)/boot/grub; \
	   printf 'set timeout=0\nset default=0\nmenuentry "Zanop Desktop OS" {\n    multiboot2 /boot/kernel.bin\n    module2 /boot/zanop-payload.img ZANOP_PAYLOAD\n    boot\n}\n' \
	      > $(ISO_DIR)/boot/grub/grub.cfg; \
	   grub-mkrescue -o "$(ISO_OUT)" $(ISO_DIR); \
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
