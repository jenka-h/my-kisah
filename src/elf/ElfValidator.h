#pragma once

#include <cstdint>
#include <string>

namespace mykisah::elf {

struct ElfValidationResult {
    bool readable = false;
    bool has_elf_magic = false;
    bool is_elf64 = false;
    bool is_little_endian = false;
    bool is_x86_64 = false;
    std::string error;

    [[nodiscard]] bool is_supported_baseline() const;
};

class ElfValidator {
public:
    [[nodiscard]] static ElfValidationResult validate_file(const std::string& path);
};

} // namespace mykisah::elf
