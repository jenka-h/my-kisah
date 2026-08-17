#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mykisah::app {

struct PatchRequest {
    std::string source_path;
    std::string output_path;
    uint64_t virtual_address = 0;
    uint64_t original_size = 0;
    std::vector<uint8_t> replacement_bytes;
};

struct PatchResult {
    uint64_t file_offset = 0;
    std::vector<uint8_t> written_bytes;
};

class BinaryPatcher {
public:
    [[nodiscard]] PatchResult patch_instruction(const PatchRequest& request) const;
    [[nodiscard]] static std::vector<uint8_t> parse_hex_bytes(const std::string& text);

private:
    [[nodiscard]] uint64_t virtual_address_to_file_offset(
        const std::string& path,
        uint64_t virtual_address,
        uint64_t size) const;
};

} // namespace mykisah::app
