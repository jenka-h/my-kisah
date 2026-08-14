#include "elf/ElfFile.h"
#include "elf/ElfValidator.h"

#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

const char* yes_no(bool value) {
    return value ? "yes" : "no";
}

void print_usage(const char* program_name) {
    std::cerr << "Usage: " << program_name << " <elf-binary>\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        print_usage(argv[0]);
        return 2;
    }

    const std::string path = argv[1];
    const auto result = mykisah::elf::ElfValidator::validate_file(path);

    std::cout << "ELF magic: " << yes_no(result.has_elf_magic) << '\n';
    std::cout << "ELF64: " << yes_no(result.is_elf64) << '\n';
    std::cout << "Endian: " << (result.is_little_endian ? "little" : "unsupported/unknown") << '\n';
    std::cout << "Architecture: " << (result.is_x86_64 ? "x86-64" : "unsupported/unknown") << '\n';

    if (!result.is_supported_baseline()) {
        if (!result.error.empty()) {
            std::cerr << "Error: " << result.error << '\n';
        }
        return 1;
    }

    try {
        const auto elf_file = mykisah::elf::ElfFileParser::parse_sections(path);

        std::cout << "\nSections:\n";
        std::cout << std::left << std::setw(12) << "Name"
                  << std::right << std::setw(14) << "File Offset"
                  << std::setw(18) << "Virtual Address"
                  << std::setw(14) << "Size" << '\n';

        for (const auto& section : elf_file.sections) {
            std::cout << std::left << std::setw(12) << section.name
                      << std::right << "0x" << std::hex << std::setw(12) << std::setfill('0') << section.file_offset
                      << "  0x" << std::setw(16) << section.virtual_address
                      << "  0x" << std::setw(12) << section.size
                      << std::dec << std::setfill(' ') << '\n';
        }

        const auto functions = mykisah::elf::ElfFileParser::parse_functions(path);
        std::cout << "\nFunctions:\n";
        std::cout << std::left << std::setw(32) << "Name"
                  << std::right << std::setw(18) << "Address"
                  << std::setw(14) << "Size"
                  << std::setw(18) << "Bytes Extracted" << '\n';

        for (const auto& function : functions) {
            std::cout << std::left << std::setw(32) << function.name
                      << std::right << "0x" << std::hex << std::setw(16) << std::setfill('0') << function.address
                      << "  0x" << std::setw(12) << function.size
                      << std::dec << std::setfill(' ') << std::setw(18) << function.bytes.size() << '\n';
        }
    } catch (const std::exception& exception) {
        std::cerr << "Error: failed to parse ELF sections: " << exception.what() << '\n';
        return 1;
    }

    return 0;
}
