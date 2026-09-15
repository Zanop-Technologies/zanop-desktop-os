# .sft Installer

This document covers building and installing `.sft` files — the app
executable format used by Zanop Desktop OS (see `docs/sft-format.md`
for the actual binary header spec).

## Status

Not yet built. This is a planning document — nothing described below
exists yet.

## Planned language support for building `.sft` files

The goal is for `.sft` files to be producible from more than one
toolchain, rather than locking app development to a single language.
Planned support, in the order it'll likely be built:

- **C/C++** — the natural first target, since the kernel and Desktop
  OS shell are themselves written in C++. Likely the reference
  implementation others get checked against.
- **Java**
- **Python**
- **JavaScript**
- **Scrifsupt** — a custom language being built specifically for
  Zanop OS app development. Not started yet.

Each of these will need its own compiler/packer path that ultimately
produces a valid `.sft` file (magic bytes, version, entry point
offset, code size, app name, then raw code) — see `sft-tools/` for
the existing C/C++-oriented packer (`sft-pack`).

## Installer

Not designed yet. Once `.sft` building exists for at least one
language, this section will cover how a built `.sft` file actually
gets installed into a running Zanop Desktop OS system (where it's
copied to, how the shell/launcher discovers it, etc.).