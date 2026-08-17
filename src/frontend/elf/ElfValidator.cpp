#include "frontend/elf/ElfValidator.h"

#include <array>
#include <fstream>

namespace mykisah::elf {
namespace {

constexpr std::size_t EI_CLASS = 4;
constexpr std::size_t EI_DATA = 5;
constexpr std::size_t EI_NIDENT = 16;
constexpr uint8_t ELFCLASS64 = 2;
constexpr uint8_t ELFDATA2LSB = 1;
constexpr uint16_t EM_X86_64 = 62;

uint16_t read_u16_le(const std::array<uint8_t, 64>& bytes, std::size_t offset) {
    return static_cast<uint16_t>(bytes[offset]) |
           static_cast<uint16_t>(bytes[offset + 1] << 8U);
}

} // namespace

bool ElfValidationResult::is_supported_baseline() const {
    return readable && has_elf_magic && is_elf64 && is_little_endian && is_x86_64;
}

ElfValidationResult ElfValidator::validate_file(const std::string& path) {
    ElfValidationResult result;

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        result.error = "cannot open input file";
        return result;
    }

    std::array<uint8_t, 64> header{};
    input.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()));
    const auto bytes_read = input.gcount();

    if (bytes_read < static_cast<std::streamsize>(EI_NIDENT)) {
        result.error = "file is too small to contain an ELF identification header";
        return result;
    }

    result.readable = true;
    result.has_elf_magic = header[0] == 0x7f && header[1] == 'E' && header[2] == 'L' && header[3] == 'F';

    if (!result.has_elf_magic) {
        result.error = "missing ELF magic";
        return result;
    }

    result.is_elf64 = header[EI_CLASS] == ELFCLASS64;
    result.is_little_endian = header[EI_DATA] == ELFDATA2LSB;

    if (bytes_read < static_cast<std::streamsize>(header.size())) {
        result.error = "file is too small to contain a complete ELF64 header";
        return result;
    }

    // e_machine is at byte offset 18 in the ELF header. Baseline supports only x86-64.
    const uint16_t machine = read_u16_le(header, 18);
    result.is_x86_64 = machine == EM_X86_64;

    if (!result.is_supported_baseline()) {
        result.error = "unsupported ELF target for baseline decompiler";
    }

    return result;
}

} // namespace mykisah::elf
