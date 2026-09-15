// sft-pack -- packages a raw compiled binary into a .sft file.
// This is a normal std binary (runs on your dev machine, not on the
// kernel) -- full standard library available, unlike everything else
// in this project. Build it separately from the freestanding kernel
// targets, with your system's normal compiler (no -ffreestanding,
// no -nostdlib).
//
// Usage: sft-pack <input-binary> -o <output.sft> [--entry <offset>] [--name <app-name>]

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

void write_u16(std::vector<std::uint8_t>& out, std::uint16_t v) {
    out.push_back(static_cast<std::uint8_t>(v & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
}
void write_u32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
}
void write_u64(std::vector<std::uint8_t>& out, std::uint64_t v) {
    for (int i = 0; i < 8; ++i) out.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
}

void print_usage() {
    std::cerr << "Usage: sft-pack <input-binary> -o <output.sft> "
                 "[--entry <offset>] [--name <app-name>]\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    std::string input_path = argv[1];
    std::string output_path = "out.sft";
    std::uint64_t entry_offset = 0;
    std::string app_name = "app";

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-o") {
            if (++i >= argc) { print_usage(); return 1; }
            output_path = argv[i];
        } else if (arg == "--entry") {
            if (++i >= argc) { print_usage(); return 1; }
            entry_offset = std::stoull(argv[i]);
        } else if (arg == "--name") {
            if (++i >= argc) { print_usage(); return 1; }
            app_name = argv[i];
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            return 1;
        }
    }

    std::ifstream in(input_path, std::ios::binary);
    if (!in) {
        std::cerr << "Failed to open input binary: " << input_path << "\n";
        return 1;
    }
    std::vector<std::uint8_t> code((std::istreambuf_iterator<char>(in)),
                                     std::istreambuf_iterator<char>());

    std::vector<std::uint8_t> out;
    out.push_back('S'); out.push_back('F'); out.push_back('T'); out.push_back('\0'); // magic
    write_u16(out, 1);                                    // version
    write_u16(out, 0);                                    // flags
    write_u64(out, entry_offset);                          // entry offset
    write_u64(out, static_cast<std::uint64_t>(code.size())); // code size
    write_u32(out, static_cast<std::uint32_t>(app_name.size())); // name length
    out.insert(out.end(), app_name.begin(), app_name.end()); // name
    out.insert(out.end(), code.begin(), code.end());         // raw code

    std::ofstream f(output_path, std::ios::binary);
    if (!f) {
        std::cerr << "Failed to write output: " << output_path << "\n";
        return 1;
    }
    f.write(reinterpret_cast<const char*>(out.data()), static_cast<std::streamsize>(out.size()));

    std::cout << "Wrote " << output_path << " (" << out.size() << " bytes)\n";
    return 0;
}