#pragma once
// Text shell for Zanop Desktop OS.
//
// Imported from zanop-kernel/src/shell and reworked to speak ECMXS
// (zanop-ecxms, see COMMANDLIST.md there) as its only command set, with
// the POSIX-style commands kept as translations onto their ECMXS
// equivalents (e.g. `ls` -> `con`, `mkdir` -> `cre dir`, `ps` -> `sy tsk`).
// The zanop-ecxms reference implementation uses std::string and
// std::filesystem, which cannot exist in a freestanding kernel, so this
// is a char-buffer port mapping everything onto the kernel's FS::
// primitives.
//
// It is usable two ways:
//   - legacy_shell::run()  -- blocking text terminal (VGA fallback path);
//   - legacy_shell::init() + set_emit()/run_command() -- one-shot command
//     execution with output routed to any sink, used by the graphical
//     Terminal app in shell/src/apps_gui.cpp.
namespace legacy_shell {
// Receives one logical output line per call (multi-line text such as a
// `cat` body may contain embedded newlines). Implementations that keep a
// line list should split on '\n'.
using EmitFn = void (*)(const char* text);

void init();  // Process/Signals/Terminal bring-up; safe to call once early
void run();   // blocking interactive text terminal

// Route command output. Pass nullptr to restore the default (text
// terminal) sink.
void set_emit(EmitFn emit);

// Execute one input line (no prompt, no loop) using the current emit.
// Prints "unknown command; type hlp" for unrecognized input.
void run_command(const char* line);

// Host name shown in the "<name>@shell~$" prompt.
const char* computer_name();
} // namespace legacy_shell
