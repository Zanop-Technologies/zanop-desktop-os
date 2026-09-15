#include "sft.hpp"

namespace sft {
namespace {
constexpr std::uint8_t MAGIC[4] = {'S', 'F', 'T', '\0'};

std::uint16_t read_u16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0]) | (static_cast<std::uint16_t>(p[1]) << 8);
}
std::uint32_t read_u32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0])
         | (static_cast<std::uint32_t>(p[1]) << 8)
         | (static_cast<std::uint32_t>(p[2]) << 16)
         | (static_cast<std::uint32_t>(p[3]) << 24);
}
std::uint64_t read_u64(const std::uint8_t* p) {
    std::uint64_t result = 0;
    for (int i = 7; i >= 0; --i) {
        result = (result << 8) | p[i];
    }
    return result;
}
}

bool parse(const std::uint8_t* data, std::size_t len, SftFile& out, SftError& err) {
    if (len < HEADER_FIXED_LEN) {
        err = SftError::TooShort;
        return false;
    }
    if (data[0] != MAGIC[0] || data[1] != MAGIC[1] || data[2] != MAGIC[2] || data[3] != MAGIC[3]) {
        err = SftError::BadMagic;
        return false;
    }

    std::uint16_t version = read_u16(data + 4);
    if (version != 1) {
        err = SftError::UnsupportedVersion;
        return false;
    }

    std::uint16_t flags = read_u16(data + 6);
    std::uint64_t entry_offset = read_u64(data + 8);
    std::uint64_t code_size = read_u64(data + 16);
    std::uint32_t name_len = read_u32(data + 24);

    std::size_t name_start = HEADER_FIXED_LEN;
    std::size_t name_end = name_start + name_len;
    if (len < name_end) {
        err = SftError::TruncatedName;
        return false;
    }

    std::size_t code_start = name_end;
    std::size_t code_end = code_start + static_cast<std::size_t>(code_size);
    if (len < code_end) {
        err = SftError::TruncatedCode;
        return false;
    }

    out.header.version = version;
    out.header.flags = flags;
    out.header.entry_offset = entry_offset;
    out.header.code_size = code_size;
    out.header.app_name = reinterpret_cast<const char*>(data + name_start);
    out.header.app_name_len = name_len;
    out.code = data + code_start;
    out.code_len = static_cast<std::size_t>(code_size);

    return true;
}

} // namespace sft