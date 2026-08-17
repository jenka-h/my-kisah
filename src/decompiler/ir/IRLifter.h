#pragma once

#include "decompiler/ir/IR.h"
#include "frontend/elf/ElfFile.h"
#include "frontend/x86/Instruction.h"

#include <vector>

namespace mykisah::core {

class IRLifter {
public:
    [[nodiscard]] IRFunction lift_function(
        const elf::Function& function,
        const std::vector<x86::Instruction>& instructions) const;

private:
    [[nodiscard]] IRValue lift_operand(const x86::Operand& operand) const;
};

} // namespace mykisah::core
