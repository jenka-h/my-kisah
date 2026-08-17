#pragma once

#include "frontend/x86/Instruction.h"

#include <string>

namespace mykisah::x86 {

class Formatter {
public:
    [[nodiscard]] std::string format_bytes(const Instruction& instruction) const;
    [[nodiscard]] std::string format_operand(const Operand& operand) const;
    [[nodiscard]] std::string format_instruction(const Instruction& instruction) const;
};

} // namespace mykisah::x86
