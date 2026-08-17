#pragma once

#include "decompiler/controlflow/CFG.h"
#include "frontend/elf/ElfFile.h"
#include "frontend/x86/Instruction.h"

#include <cstdint>
#include <string>
#include <vector>

namespace mykisah::core {

struct Parameter {
    std::string name;
    x86::Register source_register = x86::Register::None;
    uint16_t width_bits = 64;
};

struct FunctionSignature {
    std::string function_name;
    uint64_t address = 0;
    std::vector<Parameter> parameters;
    bool return_value_known = false;
    bool returns_value = false;
    uint16_t return_width_bits = 64;
};

class SignatureAnalyzer {
public:
    [[nodiscard]] FunctionSignature analyze(const elf::Function& function, const CFG& cfg) const;

private:
    [[nodiscard]] bool register_is_read_before_definite_write(const CFG& cfg, x86::Register reg) const;
    [[nodiscard]] bool instruction_reads_register(const x86::Instruction& instruction, x86::Register reg) const;
    [[nodiscard]] bool instruction_writes_register(const x86::Instruction& instruction, x86::Register reg) const;
    [[nodiscard]] bool operand_reads_register(const x86::Operand& operand, x86::Register reg) const;
    [[nodiscard]] bool operand_writes_register(const x86::Operand& operand, x86::Register reg) const;
};

[[nodiscard]] std::string format_signature(const FunctionSignature& signature);

} // namespace mykisah::core
