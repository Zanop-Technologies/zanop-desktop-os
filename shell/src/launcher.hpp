#pragma once
#include "../../compositor/src/window.hpp"

namespace desktop_shell {
compositor::Window make_start_menu();
// Maps a typed digit ('1'-'9') to an app name, or nullptr if out of range.
const char* app_for_key(char key);
}