#include "application/BinaryPatcher.h"

#include "frontend/elf/ElfFile.h"
#include "frontend/elf/ElfValidator.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace mykisah::app {
namespace {

constexpr uint8_t X86_NOP = 0x90;
constexpr uint64_t SHF_EXECINSTR = 0x4;

int hex_value(char character) {
    if (character >= '0' && character <= '9') return character - '0';
    if (character >= 'a' && character <= 'f') return character - 'a' + 10;
    if (character >= 'A' && character <= 'F') return character - 'A' + 10;
    return -1;
}

} // namespace

std::vector<uint8_t> BinaryPatcher::parse_hex_bytes(const std::string& text) {
    std::string digits;
    digits.reserve(text.size());

    for (std::size_t index = 0; index < text.size(); ++index) {
        const char character = text[index];
        if (character == '0' && index + 1 < text.size() && (text[index + 1] == 'x' || text[index + 1] == 'X')) {
            ++index;
            continue;
        }
        if (std::isxdigit(static_cast<unsigned char>(character))) {
            digits.push_back(character);
        } else if (!std::isspace(static_cast<unsigned char>(character)) && character != ',' && character != ':') {
            throw std::runtime_error("replacement contains a non-hex character");
        }
    }

    if (digits.empty()) {
        throw std::runtime_error("replacement bytes cannot be empty");
    }
    if ((digits.size() % 2U) != 0) {
        throw std::runtime_error("replacement must contain complete byte pairs");
    }

    std::vector<uint8_t> bytes;
    bytes.reserve(digits.size() / 2U);
    for (std::size_t index = 0; index < digits.size(); index += 2U) {
        const int high = hex_value(digits[index]);
        const int low = hex_value(digits[index + 1]);
        if (high < 0 || low < 0) {
            throw std::runtime_error("replacement contains invalid hexadecimal bytes");
        }
        bytes.push_back(static_cast<uint8_t>((high << 4) | low));
    }

    return bytes;
}

uint64_t BinaryPatcher::virtual_address_to_file_offset(
    const std::string& path,
    uint64_t virtual_address,
    uint64_t size) const {
    const auto elf_file = elf::ElfFileParser::parse_sections(path);

    for (const auto& section : elf_file.sections) {
        if ((section.flags & SHF_EXECINSTR) == 0 || section.size == 0 || virtual_address < section.virtual_address) {
            continue;
        }

        const uint64_t offset_in_section = virtual_address - section.virtual_address;
        if (offset_in_section <= section.size && size <= section.size - offset_in_section) {
            return section.file_offset + offset_in_section;
        }
    }

    throw std::runtime_error("selected instruction is not fully inside an executable ELF section");
}

PatchResult BinaryPatcher::patch_instruction(const PatchRequest& request) const {
    if (request.source_path.empty() || request.output_path.empty()) {
        throw std::runtime_error("source and output paths are required");
    }
    const auto canonical_source = std::filesystem::weakly_canonical(request.source_path);
    const auto canonical_output = std::filesystem::weakly_canonical(request.output_path);
    if (canonical_source == canonical_output) {
        throw std::runtime_error("refusing to overwrite the original ELF; choose a separate output path");
    }
    if (request.original_size == 0) {
        throw std::runtime_error("selected instruction has zero size");
    }
    if (request.replacement_bytes.empty()) {
        throw std::runtime_error("replacement bytes cannot be empty");
    }
    if (request.replacement_bytes.size() > request.original_size) {
        throw std::runtime_error("replacement is larger than the selected instruction");
    }

    const auto validation = elf::ElfValidator::validate_file(request.source_path);
    if (!validation.is_supported_baseline()) {
        throw std::runtime_error("source file is not a supported ELF64 x86-64 binary");
    }

    const uint64_t file_offset = virtual_address_to_file_offset(
        request.source_path,
        request.virtual_address,
        request.original_size);

    std::filesystem::copy_file(
        request.source_path,
        request.output_path,
        std::filesystem::copy_options::overwrite_existing);

    std::vector<uint8_t> written = request.replacement_bytes;
    written.resize(static_cast<std::size_t>(request.original_size), X86_NOP);

    std::fstream output(request.output_path, std::ios::binary | std::ios::in | std::ios::out);
    if (!output) {
        throw std::runtime_error("cannot open patched copy for writing");
    }

    output.seekp(static_cast<std::streamoff>(file_offset));
    output.write(reinterpret_cast<const char*>(written.data()), static_cast<std::streamsize>(written.size()));
    if (!output) {
        throw std::runtime_error("failed to write replacement bytes");
    }

    return PatchResult{file_offset, std::move(written)};
}

} // namespace mykisah::app
