#pragma once
#include "types.hpp"

// .sft file parser -- implements the header format designed earlier:
//
// Offset   Size    Field
// 0x00     4       Magic bytes: "SFT\0"
// 0x04     2       Format version (u16)
// 0x06     2       Flags (u16) -- reserved
// 0x08     8       Entry point offset (u64)
// 0x10     8       Code size in bytes (u64)
// 0x18     4       App name length (u32)
// 0x1C     N       App name (UTF-8, N bytes)
// ...      --      Raw code/data, padded to alignment
//
// Deliberately stores app_name and code as (pointer, length) pairs
// into the ORIGINAL buffer rather than copying into a String/Vector
// -- avoids depending on kernel-side kstd utilities from within the
// loader, keeping this self-contained. Caller must keep the original
// buffer alive as long as the returned SftFile is used.

namespace sft {

constexpr std::size_t HEADER_FIXED_LEN = 0x1C;

enum class SftError {
    TooShort,
    BadMagic,
    UnsupportedVersion,
    TruncatedName,
    TruncatedCode,
};

struct SftHeader {
    std::uint16_t version;
    std::uint16_t flags;
    std::uint64_t entry_offset;
    std::uint64_t code_size;
    const char* app_name;      // NOT null-terminated -- use app_name_len
    std::size_t app_name_len;
};

struct SftFile {
    SftHeader header;
    const std::uint8_t* code;
    std::size_t code_len;
};

// Returns true and fills `out` on success. Returns false and fills
// `err` on failure. `data` must outlive the returned SftFile, since
// app_name/code point directly into it.
bool parse(const std::uint8_t* data, std::size_t len, SftFile& out, SftError& err);

} // namespace sft