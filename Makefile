# Zanop Desktop OS — Build "Chicago" (C++)
# Combines zanop-kernel's sources with Desktop OS's compositor/shell/
# input sources into one bootable kernel+GUI binary, since
# desktop_shell::run() is meant to replace the kernel's plain-text
# shell::run() call in main.cpp.
#
# ASSUMES:
#   - zanop-kernel and zanop-desktop-os sit as sibling folders
#   - zanop-kernel/src/main.cpp has been edited to call
#     desktop_shell::run() instead of shell::run() (see integration
#     note in README) -- otherwise this will link fine but boot into
#     the plain-text shell, not the GUI, since main.cpp decides that.
#   - ../limine (the Limine bootloader repo, cloned + built earlier)
#     sits alongside both of the above.
#   - installer/ is DELIBERATELY EXCLUDED from this build for now --
#     it depends on installer/src/space_mono_font.hpp, which only
#     exists after running installer/tools/ttf_to_vga_font.py against
#     a real Space Mono .ttf. Including it before that file exists
#     would guarantee a build failure. Add it back to SRC_CPP once
#     that header is generated.

CXX      := clang++
AS       := nasm
LD       := ld.lld

KERNEL_DIR := ../zanop-os
LIMINE_DIR := ../limine

TARGET   := x86_64-unknown-none
CXXFLAGS := -target $(TARGET) -ffreestanding -fno-exceptions -fno-rtti \
            -fno-stack-protector -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
            -Wall -Wextra -std=c++20 -I$(KERNEL_DIR)/src -I.

ASFLAGS  := -f elf64
LDFLAGS  := -T $(KERNEL_DIR)/src/arch/x86_64/linker.ld -nostdlib --gc-sections

BUILD_DIR := build

# Kernel sources (everything except its own main.cpp -- we don't want
# two competing kernel_main definitions; main.cpp is included
# separately below since it's the one file that needs the shell::run()
# -> desktop_shell::run() edit applied).
KERNEL_SRC_CPP := $(shell find $(KERNEL_DIR)/src -name '*.cpp' ! -name 'main.cpp')
KERNEL_SRC_MAIN := $(KERNEL_DIR)/src/main.cpp
KERNEL_SRC_ASM  := $(KERNEL_DIR)/src/arch/x86_64/boot.asm \

# Desktop OS sources (installer/ excluded, see note above).
#DESKTOP_SRC_CPP := $(shell find compositor shell input loader -name '*.cpp' 2>/dev/null)

ALL_SRC_CPP := $(KERNEL_SRC_CPP) $(KERNEL_SRC_MAIN)

# Flatten object output paths to avoid collisions between the two repos.
OBJ_CPP := $(addprefix $(BUILD_DIR)/, $(addsuffix .o, $(notdir $(basename $(ALL_SRC_CPP)))))
OBJ_ASM := $(BUILD_DIR)/boot.o 

KERNEL_BIN := $(BUILD_DIR)/zanop-desktop-os.bin

# Where the final ISO ends up -- shared storage, so it's easy to grab
# from outside this proot/Termux environment.
DOWNLOADS_DIR := /sdcard/Download
ISO_DIR       := $(BUILD_DIR)/iso
ISO_OUT       := $(DOWNLOADS_DIR)/zanop-desktop-os.iso

.PHONY: all iso clean

all: $(KERNEL_BIN)

$(KERNEL_BIN): $(OBJ_CPP) $(OBJ_ASM)
	$(LD) $(LDFLAGS) -o $@ $(OBJ_ASM) $(OBJ_CPP)

# Pattern rules compiling each source into build/<basename>.o,
# regardless of which repo/subfolder it actually lives in.
define CXX_RULE
$(BUILD_DIR)/$(notdir $(basename $(1))).o: $(1)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $$< -o $$@
endef
$(foreach src,$(ALL_SRC_CPP),$(eval $(call CXX_RULE,$(src))))

$(BUILD_DIR)/boot.o: $(KERNEL_DIR)/src/arch/x86_64/boot.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) $(ASFLAGS) $< -o $@

	@mkdir -p $(BUILD_DIR)
	$(AS) $(ASFLAGS) $< -o $@

iso: $(KERNEL_BIN)
	@mkdir -p $(ISO_DIR)/boot/limine
	@mkdir -p $(DOWNLOADS_DIR)
	cp $(KERNEL_BIN) $(ISO_DIR)/boot/kernel.bin
	cp $(LIMINE_DIR)/limine-bios.sys $(LIMINE_DIR)/limine-bios-cd.bin \
	   $(LIMINE_DIR)/limine-uefi-cd.bin $(ISO_DIR)/boot/limine/
	printf 'timeout: 0\n\n/Zanop Desktop OS\n    protocol: multiboot2\n    kernel_path: boot():/boot/kernel.bin\n    video_mode: text\n' \
	   > $(ISO_DIR)/boot/limine/limine.conf
	xorriso -as mkisofs -b boot/limine/limine-bios-cd.bin \
	   -no-emul-boot -boot-load-size 4 -boot-info-table \
	   --efi-boot boot/limine/limine-uefi-cd.bin \
	   -efi-boot-part --efi-boot-image --protective-msdos-label \
	   $(ISO_DIR) -o $(ISO_OUT)
	$(LIMINE_DIR)/limine bios-install $(ISO_OUT)
	@echo "ISO written to $(ISO_OUT)"

clean:
	rm -rf $(BUILD_DIR)
