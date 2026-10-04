# Zanop Desktop OS — Build "Chicago" (September 2026)

The first draft of the actual GUI layer running on top of the Zanop
Kernel. "Basic" on purpose: the kernel only has VGA text-mode output
right now (no pixel framebuffer driver yet), so this is a **text-mode
windowing desktop** — bordered windows drawn with line-art characters,
a taskbar, and a numbered text launcher. No icons, because there's no
graphics to draw them with yet. Real icon/pixel support is planned for
a later kernel version with proper GUI support.

## What's in this build

- **Desktop background** — a light dither pattern fill (the closest
  thing to "wallpaper" in text mode)
- **Top bar** — build name + a live clock (reads the kernel's CMOS RTC)
- **Bottom bar** — a `[ Start ]` label and the title of the focused window
- **Start Menu** — press `S` to open it. A plain numbered list of the
  kernel's built-in apps (Calculator, Notes, Clock, Snake) — press the
  number shown to launch, `Esc` to close the menu without launching
  anything
- **Windows** — bordered boxes with a title bar, drawn via the
  compositor. Currently only the Start Menu is rendered as a real
  window; launched apps take over the full screen for now (see
  "Known limitations" below)

## Repo layout

```
zanop-desktop-os/
├── compositor/     # text-mode window manager (surface.rs, window.rs, mod.rs)
├── shell/          # desktop.rs (background), taskbar.rs, launcher.rs, mod.rs
├── input/          # thin wrapper around the kernel's keyboard driver
├── loader/         # unchanged from the earlier .sft draft
├── sft-tools/       # unchanged from the earlier .sft draft
└── kernel-patch/    # NOT part of the workspace — apply this to zanop-kernel first
```

## Required: apply the kernel patch first

This build depends on `zanop-kernel` as a **library**, not just a
binary, so it can reuse the kernel's already-fixed VGA writer and
interrupt-driven keyboard input instead of duplicating that code (and
duplicating its bugs). The kernel repo doesn't expose itself as a
library yet, so:

1. Open `kernel-patch/APPLY_PATCH.txt` and follow its 4 steps in your
   `zanop-kernel` repo (adds `src/lib.rs`, a `[lib]` section in
   `Cargo.toml`, and makes a few modules `pub`)
2. Open `kernel-patch/VGA_PATCH.txt` and add the `put_at()` method it
   describes to `zanop-kernel/src/drivers/vga.rs` — the compositor
   needs this to draw windows without disturbing the shell's own
   cursor position

Both patches are additive only — they don't change any existing
kernel behavior from v0.0.4.

## Required: wire the kernel to boot into this shell

In `zanop-kernel/src/main.rs`, change the line that currently calls
the plain-text shell:

```rust
// before:
shell::run()

// after:
zanop_desktop_shell::run()
```

You'll also need to add `zanop-desktop-shell` as a dependency in the
kernel's `Cargo.toml` `[[bin]]` build — this creates a small circular
relationship (kernel binary depends on a crate that depends on the
kernel library), which Cargo handles fine, but is worth knowing about
if it looks unusual.

## Folder path assumption

`Cargo.toml`'s `zanop-kernel` dependency uses `path = "../zanop-kernel"`,
which assumes both repos sit as sibling folders (e.g. both under
`~/Projects/`). Adjust that path if your layout differs.

## Building sft-tools (needs a target override)

`.cargo/config.toml` at the workspace root defaults every crate to the
freestanding kernel target — correct for everything except
`sft-tools`, which is a normal host binary (runs on your dev machine
to package `.sft` files, not on the kernel itself). Build it with an
explicit override to your actual host target, e.g.:

```bash
cargo build -p sft-tools --target x86_64-unknown-linux-gnu
```

(swap the target triple for whatever matches your machine — check
with `rustc -vV` if unsure).

## Known limitations (honest, not hedging)

- **No compiled/boot test yet** — same sandbox limitation as every
  kernel build so far: no Rust/NASM/QEMU toolchain here, so this is
  correct by inspection, not by an actual successful build.
- **Launched apps take over the whole screen** rather than living in
  their own window — Calculator/Notes/Clock/Snake still print through
  the shared VGA writer directly, same as they did before this GUI
  layer existed. A real "windowed app host" that redirects an app's
  output into a bounded window region is the natural next step, but
  it's a real chunk of work (needs a virtual/clipped writer), so it's
  deliberately not attempted in this first draft.
- **No arrow-key navigation** — arrow keys send an `0xE0`-prefixed
  extended scancode that nothing in the kernel's keyboard driver
  decodes yet (see `input/src/mod.rs`'s `read_arrow_key()` stub, which
  always returns `None` on purpose rather than pretending to work).
  Number-key selection in the Start Menu is the workaround for now.
- **No mouse support** — there's no mouse driver in the kernel at all
  yet, so everything here is keyboard-only by necessity, not choice.
- **Single-window focus model** — the compositor is a simple stack
  (last opened = focused), no click-to-focus, no window dragging, no
  overlapping window management beyond draw order.

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
System Folder cannot be deleted no matter what the command is.

## Next Milestone

Get the kernel patches applied and an actual boot test done, then
tackle either: (a) a real windowed app host so launched apps render
inside a bounded region instead of taking the full screen, or (b)
0xE0 extended scancode handling to unlock arrow-key navigation —
whichever matters more for how you want the Start Menu to feel.
