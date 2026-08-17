#include "decompiler/analysis/Signature.h"

#include "decompiler/analysis/Expression.h"

#include <algorithm>
#include <queue>
#include <set>
#include <sstream>
#include <tuple>

namespace mykisah::core {
namespace {

constexpr x86::Register ABI_ARGUMENT_REGISTERS[] = {
    x86::Register::RDI,
    x86::Register::RSI,
    x86::Register::RDX,
    x86::Register::RCX,
    x86::Register::R8,
    x86::Register::R9,
};

std::string parameter_name_for_register(x86::Register reg) {
    return abi_argument_name(reg);
}

x86::Register canonical_register(x86::Register reg) {
    // The Register enum already stores canonical register families; width lives on operands.
    return reg;
}

bool memory_uses_register(const x86::MemoryOperand& memory, x86::Register reg) {
    const auto canonical = canonical_register(reg);
    return canonical_register(memory.base) == canonical || canonical_register(memory.index) == canonical;
}

} // namespace

bool SignatureAnalyzer::operand_reads_register(const x86::Operand& operand, x86::Register reg) const {
    const auto canonical = canonical_register(reg);

    if (operand.kind == x86::OperandKind::Register && canonical_register(operand.reg) == canonical) {
        return operand.access == x86::OperandAccess::Read || operand.access == x86::OperandAccess::ReadWrite;
    }

    if (operand.kind == x86::OperandKind::Memory) {
        return memory_uses_register(operand.memory, canonical);
    }

    return false;
}

bool SignatureAnalyzer::operand_writes_register(const x86::Operand& operand, x86::Register reg) const {
    const auto canonical = canonical_register(reg);
    return operand.kind == x86::OperandKind::Register && canonical_register(operand.reg) == canonical &&
           (operand.access == x86::OperandAccess::Write || operand.access == x86::OperandAccess::ReadWrite);
}

bool SignatureAnalyzer::instruction_reads_register(const x86::Instruction& instruction, x86::Register reg) const {
    return std::any_of(instruction.operands.begin(), instruction.operands.end(), [this, reg](const x86::Operand& operand) {
        return operand_reads_register(operand, reg);
    });
}

bool SignatureAnalyzer::instruction_writes_register(const x86::Instruction& instruction, x86::Register reg) const {
    if (instruction.opcode == x86::Opcode::Call && reg == x86::Register::RAX) {
        return true;
    }

    return std::any_of(instruction.operands.begin(), instruction.operands.end(), [this, reg](const x86::Operand& operand) {
        return operand_writes_register(operand, reg);
    });
}

bool SignatureAnalyzer::register_is_read_before_definite_write(const CFG& cfg, x86::Register reg) const {
    if (cfg.entry_block == INVALID_BLOCK_ID || cfg.entry_block >= cfg.blocks.size()) {
        return false;
    }

    struct WorkItem {
        BlockId block;
        bool already_written;
    };

    std::queue<WorkItem> queue;
    std::set<std::tuple<BlockId, bool>> visited;
    queue.push({cfg.entry_block, false});

    while (!queue.empty()) {
        const auto item = queue.front();
        queue.pop();

        if (!visited.insert({item.block, item.already_written}).second) {
            continue;
        }

        if (item.block >= cfg.blocks.size()) {
            continue;
        }

        bool written = item.already_written;
        const auto& block = cfg.blocks[item.block];

        for (const auto& instruction : block.instructions) {
            if (!written && instruction_reads_register(instruction, reg)) {
                return true;
            }

            if (instruction_writes_register(instruction, reg)) {
                written = true;
            }
        }

        for (const auto& edge : block.successors) {
            queue.push({edge.to, written});
        }
    }

    return false;
}

FunctionSignature SignatureAnalyzer::analyze(const elf::Function& function, const CFG& cfg) const {
    FunctionSignature signature;
    signature.function_name = function.name;
    signature.address = function.address;

    for (const auto reg : ABI_ARGUMENT_REGISTERS) {
        if (register_is_read_before_definite_write(cfg, reg)) {
            signature.parameters.push_back(Parameter{parameter_name_for_register(reg), reg, 64});
        }
    }

    bool saw_return = false;
    bool saw_rax_definition = false;

    for (const auto& block : cfg.blocks) {
        for (const auto& instruction : block.instructions) {
            saw_rax_definition = saw_rax_definition || instruction_writes_register(instruction, x86::Register::RAX);
            saw_return = saw_return || instruction.opcode == x86::Opcode::Ret;
        }
    }

    if (saw_return && saw_rax_definition) {
        signature.return_value_known = true;
        signature.returns_value = true;
    }

    return signature;
}

std::string format_signature(const FunctionSignature& signature) {
    std::ostringstream output;
    if (!signature.return_value_known) {
        output << "unknown_return";
    } else {
        output << (signature.returns_value ? "int64_t" : "void");
    }
    output << ' ' << signature.function_name << '(';

    for (std::size_t i = 0; i < signature.parameters.size(); ++i) {
        if (i != 0) {
            output << ", ";
        }
        output << "int64_t " << signature.parameters[i].name;
    }

    output << ')';
    return output.str();
}

} // namespace mykisah::core
