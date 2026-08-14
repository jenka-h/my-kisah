#include "elf/ElfFile.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <stdexcept>
#include <string_view>

namespace mykisah::elf {
namespace {

constexpr std::size_t ELF64_HEADER_SIZE = 64;
constexpr std::size_t ELF64_SECTION_HEADER_SIZE = 64;

uint16_t read_u16_le(const std::vector<uint8_t>& bytes, std::size_t offset) {
    return static_cast<uint16_t>(bytes.at(offset)) |
           static_cast<uint16_t>(bytes.at(offset + 1) << 8U);
}

uint32_t read_u32_le(const std::vector<uint8_t>& bytes, std::size_t offset) {
    return static_cast<uint32_t>(bytes.at(offset)) |
           (static_cast<uint32_t>(bytes.at(offset + 1)) << 8U) |
           (static_cast<uint32_t>(bytes.at(offset + 2)) << 16U) |
           (static_cast<uint32_t>(bytes.at(offset + 3)) << 24U);
}

uint64_t read_u64_le(const std::vector<uint8_t>& bytes, std::size_t offset) {
    return static_cast<uint64_t>(bytes.at(offset)) |
           (static_cast<uint64_t>(bytes.at(offset + 1)) << 8U) |
           (static_cast<uint64_t>(bytes.at(offset + 2)) << 16U) |
           (static_cast<uint64_t>(bytes.at(offset + 3)) << 24U) |
           (static_cast<uint64_t>(bytes.at(offset + 4)) << 32U) |
           (static_cast<uint64_t>(bytes.at(offset + 5)) << 40U) |
           (static_cast<uint64_t>(bytes.at(offset + 6)) << 48U) |
           (static_cast<uint64_t>(bytes.at(offset + 7)) << 56U);
}

std::vector<uint8_t> read_all_bytes(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open input file");
    }

    input.seekg(0, std::ios::end);
    const auto size = input.tellg();
    if (size < 0) {
        throw std::runtime_error("cannot determine input file size");
    }

    std::vector<uint8_t> bytes(static_cast<std::size_t>(size));
    input.seekg(0, std::ios::beg);
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!input && !bytes.empty()) {
        throw std::runtime_error("failed to read input file");
    }

    return bytes;
}

std::string read_c_string(const std::vector<uint8_t>& bytes, std::size_t offset, std::size_t limit) {
    if (offset >= bytes.size() || offset >= limit) {
        return {};
    }

    std::string name;
    for (std::size_t i = offset; i < bytes.size() && i < limit && bytes[i] != 0; ++i) {
        name.push_back(static_cast<char>(bytes[i]));
    }
    return name;
}

bool is_requested_section(std::string_view name) {
    return name == ".text" || name == ".symtab" || name == ".strtab" ||
           name == ".rodata" || name == ".data";
}

ElfSection section_from_header(
    const std::vector<uint8_t>& bytes,
    uint64_t header_offset,
    std::string name) {
    ElfSection section;
    section.name = std::move(name);
    section.type = read_u32_le(bytes, static_cast<std::size_t>(header_offset + 4));
    section.flags = read_u64_le(bytes, static_cast<std::size_t>(header_offset + 8));
    section.virtual_address = read_u64_le(bytes, static_cast<std::size_t>(header_offset + 16));
    section.file_offset = read_u64_le(bytes, static_cast<std::size_t>(header_offset + 24));
    section.size = read_u64_le(bytes, static_cast<std::size_t>(header_offset + 32));
    section.link = read_u32_le(bytes, static_cast<std::size_t>(header_offset + 40));
    section.entry_size = read_u64_le(bytes, static_cast<std::size_t>(header_offset + 56));
    return section;
}

std::vector<ElfSection> parse_all_sections(const std::vector<uint8_t>& bytes) {
    if (bytes.size() < ELF64_HEADER_SIZE) {
        throw std::runtime_error("file is too small to contain an ELF64 header");
    }

    const uint64_t section_header_offset = read_u64_le(bytes, 40);
    const uint16_t section_header_entry_size = read_u16_le(bytes, 58);
    const uint16_t section_header_count = read_u16_le(bytes, 60);
    const uint16_t section_name_string_table_index = read_u16_le(bytes, 62);

    if (section_header_entry_size < ELF64_SECTION_HEADER_SIZE) {
        throw std::runtime_error("unsupported ELF section header size");
    }

    if (section_name_string_table_index >= section_header_count) {
        throw std::runtime_error("invalid section-name string table index");
    }

    const uint64_t section_table_size = static_cast<uint64_t>(section_header_entry_size) * section_header_count;
    if (section_header_offset > bytes.size() || section_table_size > bytes.size() - section_header_offset) {
        throw std::runtime_error("section header table extends beyond end of file");
    }

    const uint64_t shstrtab_header_offset = section_header_offset +
        static_cast<uint64_t>(section_name_string_table_index) * section_header_entry_size;
    const uint64_t shstrtab_file_offset = read_u64_le(bytes, static_cast<std::size_t>(shstrtab_header_offset + 24));
    const uint64_t shstrtab_size = read_u64_le(bytes, static_cast<std::size_t>(shstrtab_header_offset + 32));

    if (shstrtab_file_offset > bytes.size() || shstrtab_size > bytes.size() - shstrtab_file_offset) {
        throw std::runtime_error("section-name string table extends beyond end of file");
    }

    std::vector<ElfSection> sections;

    for (uint16_t index = 0; index < section_header_count; ++index) {
        const uint64_t header_offset = section_header_offset + static_cast<uint64_t>(index) * section_header_entry_size;
        const uint32_t name_offset = read_u32_le(bytes, static_cast<std::size_t>(header_offset));
        const auto name = read_c_string(
            bytes,
            static_cast<std::size_t>(shstrtab_file_offset + name_offset),
            static_cast<std::size_t>(shstrtab_file_offset + shstrtab_size));

        sections.push_back(section_from_header(bytes, header_offset, name));
    }

    return sections;
}

const ElfSection* find_section_by_name(const std::vector<ElfSection>& sections, std::string_view name) {
    const auto iterator = std::find_if(sections.begin(), sections.end(), [name](const ElfSection& section) {
        return section.name == name;
    });

    if (iterator == sections.end()) {
        return nullptr;
    }
    return &*iterator;
}

const ElfSection* find_allocated_section_containing_address(
    const std::vector<ElfSection>& sections,
    uint64_t address,
    uint64_t size) {
    constexpr uint64_t SHF_ALLOC = 0x2;

    for (const auto& section : sections) {
        if ((section.flags & SHF_ALLOC) == 0 || section.virtual_address == 0 || section.size == 0) {
            continue;
        }

        if (address < section.virtual_address) {
            continue;
        }

        const uint64_t offset_in_section = address - section.virtual_address;
        if (offset_in_section <= section.size && size <= section.size - offset_in_section) {
            return &section;
        }
    }

    return nullptr;
}

void validate_file_range(const std::vector<uint8_t>& bytes, const ElfSection& section, std::string_view context) {
    if (section.file_offset > bytes.size() || section.size > bytes.size() - section.file_offset) {
        throw std::runtime_error(std::string(context) + " extends beyond end of file");
    }
}

} // namespace

ElfFile ElfFileParser::parse_sections(const std::string& path) {
    const auto bytes = read_all_bytes(path);
    auto all_sections = parse_all_sections(bytes);

    ElfFile elf_file;
    for (const auto& section : all_sections) {
        if (is_requested_section(section.name)) {
            elf_file.sections.push_back(section);
        }
    }

    std::sort(elf_file.sections.begin(), elf_file.sections.end(), [](const ElfSection& lhs, const ElfSection& rhs) {
        return lhs.virtual_address < rhs.virtual_address;
    });

    return elf_file;
}

std::vector<Function> ElfFileParser::parse_functions(const std::string& path) {
    constexpr uint8_t STT_FUNC = 2;
    constexpr uint16_t SHN_UNDEF = 0;
    constexpr uint64_t ELF64_SYMBOL_SIZE = 24;

    const auto bytes = read_all_bytes(path);
    const auto sections = parse_all_sections(bytes);

    const auto* symtab = find_section_by_name(sections, ".symtab");
    if (symtab == nullptr) {
        return {};
    }

    validate_file_range(bytes, *symtab, ".symtab");

    if (symtab->entry_size != ELF64_SYMBOL_SIZE) {
        throw std::runtime_error("unsupported ELF symbol entry size");
    }

    if (symtab->link >= sections.size()) {
        throw std::runtime_error(".symtab has invalid linked string table index");
    }

    const auto& string_table = sections.at(symtab->link);
    validate_file_range(bytes, string_table, "symbol string table");

    std::vector<Function> functions;
    const auto symbol_count = symtab->size / symtab->entry_size;

    for (uint64_t index = 0; index < symbol_count; ++index) {
        const auto symbol_offset = static_cast<std::size_t>(symtab->file_offset + index * symtab->entry_size);
        const uint32_t name_offset = read_u32_le(bytes, symbol_offset);
        const uint8_t info = bytes.at(symbol_offset + 4);
        const uint16_t section_index = read_u16_le(bytes, symbol_offset + 6);
        const uint64_t value = read_u64_le(bytes, symbol_offset + 8);
        const uint64_t size = read_u64_le(bytes, symbol_offset + 16);

        const uint8_t symbol_type = info & 0x0fU;
        if (symbol_type != STT_FUNC || section_index == SHN_UNDEF || value == 0 || size == 0) {
            continue;
        }

        Function function;
        function.name = read_c_string(
            bytes,
            static_cast<std::size_t>(string_table.file_offset + name_offset),
            static_cast<std::size_t>(string_table.file_offset + string_table.size));
        function.address = value;
        function.size = size;

        if (function.name.empty()) {
            function.name = "function_" + std::to_string(function.address);
        }

        const auto* containing_section = find_allocated_section_containing_address(sections, function.address, function.size);
        if (containing_section != nullptr) {
            validate_file_range(bytes, *containing_section, containing_section->name);
            const uint64_t offset_in_section = function.address - containing_section->virtual_address;
            const uint64_t function_file_offset = containing_section->file_offset + offset_in_section;
            if (function_file_offset <= bytes.size() && function.size <= bytes.size() - function_file_offset) {
                const auto begin = bytes.begin() + static_cast<std::ptrdiff_t>(function_file_offset);
                const auto end = begin + static_cast<std::ptrdiff_t>(function.size);
                function.bytes.assign(begin, end);
            }
        }

        functions.push_back(function);
    }

    std::sort(functions.begin(), functions.end(), [](const Function& lhs, const Function& rhs) {
        return lhs.address < rhs.address;
    });

    return functions;
}

} // namespace mykisah::elf
