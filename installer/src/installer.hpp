#pragma once

// The real (non-mockup) Windows 1.01-inspired installer, adapted for
// text-mode: bordered windows via compositor::Window, keyboard-only
// navigation (Enter = Next, Esc = Back/Cancel), same step sequence as
// the earlier HTML mockup -- Welcome, License, Setup Type, User Info,
// Installing (progress), Finish.

namespace installer {
void run();
}
