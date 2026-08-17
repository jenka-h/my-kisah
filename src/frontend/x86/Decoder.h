#pragma once

#include "frontend/x86/Instruction.h"

#include <cstdint>
#include <vector>

namespace mykisah::x86 {

class Decoder {
public:
    [[nodiscard]] std::vector<Instruction> decode(const std::vector<uint8_t>& bytes, uint64_t base_address) const;
};

} // namespace mykisah::x86
