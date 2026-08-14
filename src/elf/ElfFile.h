#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mykisah::elf {

struct ElfSection {
    std::string name;
    uint32_t type = 0;
    uint64_t flags = 0;
    uint64_t file_offset = 0;
    uint64_t virtual_address = 0;
    uint64_t size = 0;
    uint32_t link = 0;
    uint64_t entry_size = 0;
};

struct Function {
    std::string name;
    uint64_t address = 0;
    uint64_t size = 0;
    std::vector<uint8_t> bytes;
};

struct ElfFile {
    std::vector<ElfSection> sections;
    std::vector<Function> functions;
};

class ElfFileParser {
public:
    [[nodiscard]] static ElfFile parse_sections(const std::string& path);
    [[nodiscard]] static std::vector<Function> parse_functions(const std::string& path);
};

} // namespace mykisah::elf
