#pragma once
namespace desktop_shell {
void run(); // meant to be called from the kernel's main.cpp instead
            // of the plain-text shell::run()
}