#pragma once

#include "../../../zanop-kernel/src/types.hpp"

namespace installer {
void set_system_memory(std::uint64_t bytes);
void set_boot_payload(const std::uint8_t* data, std::size_t size);
void run_graphical();
}
