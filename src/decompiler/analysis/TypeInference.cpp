#include "decompiler/analysis/TypeInference.h"

#include <sstream>

namespace mykisah::core {
namespace {

uint32_t bit(TypeEvidence evidence) {
    return static_cast<uint32_t>(evidence);
}

bool has(const TypeInfo& info, TypeEvidence evidence) {
    return (info.evidence & bit(evidence)) != 0;
}

TypeEvidence width_evidence(uint16_t width_bits) {
    switch (width_bits) {
        case 8: return TypeEvidence::Width8;
        case 16: return TypeEvidence::Width16;
        case 32: return TypeEvidence::Width32;
        case 64: return TypeEvidence::Width64;
        default: return TypeEvidence::None;
    }
}

bool is_arithmetic(x86::Opcode opcode) {
    return opcode == x86::Opcode::Add || opcode == x86::Opcode::Sub || opcode == x86::Opcode::And ||
           opcode == x86::Opcode::Xor || opcode == x86::Opcode::Imul;
}

} // namespace

void TypeInference::add_evidence(TypeInfo& info, TypeEvidence evidence, uint16_t width_bits) const {
    info.evidence |= bit(evidence);
    if (width_bits != 0) {
        info.width_bits = info.width_bits == 0 ? width_bits : info.width_bits;
        info.evidence |= bit(width_evidence(width_bits));
    }
}

TypeCategory TypeInference::resolve(const TypeInfo& info) const {
    const bool pointer_evidence = has(info, TypeEvidence::UsedAsAddress) || has(info, TypeEvidence::Dereferenced) ||
                                  has(info, TypeEvidence::UsedInPointerArithmetic);
    const bool integer_evidence = has(info, TypeEvidence::UsedInArithmetic);
    const bool boolean_evidence = has(info, TypeEvidence::UsedAsCondition) && has(info, TypeEvidence::ComparedToZero);

    if (pointer_evidence) {
        return TypeCategory::Pointer;
    }
    if (boolean_evidence && !integer_evidence) {
        return TypeCategory::BooleanLike;
    }
    if (integer_evidence || has(info, TypeEvidence::Compared)) {
        return TypeCategory::Integer;
    }
    return TypeCategory::Unknown;
}

void TypeInference::collect_operand_evidence(
    FunctionTypeInfo& result,
    const x86::Instruction& instruction,
    const x86::Operand& operand) const {
    if (operand.kind == x86::OperandKind::Register) {
        auto& info = result.register_types[operand.reg];
        add_evidence(info, width_evidence(operand.width_bits), operand.width_bits);

        if (is_arithmetic(instruction.opcode)) {
            add_evidence(info, TypeEvidence::UsedInArithmetic, operand.width_bits);
        }
        if (instruction.opcode == x86::Opcode::Cmp || instruction.opcode == x86::Opcode::Test) {
            add_evidence(info, TypeEvidence::Compared, operand.width_bits);
        }
    }

    if (operand.kind == x86::OperandKind::Memory) {
        const bool is_lea = instruction.opcode == x86::Opcode::Lea;
        if (operand.memory.base != x86::Register::None && operand.memory.base != x86::Register::RIP) {
            auto& base = result.register_types[operand.memory.base];
            if (is_lea) {
                add_evidence(base, TypeEvidence::UsedInArithmetic, 64);
            } else {
                add_evidence(base, TypeEvidence::UsedAsAddress, 64);
                add_evidence(base, TypeEvidence::Dereferenced, 64);
            }
        }
        if (operand.memory.index != x86::Register::None) {
            auto& index = result.register_types[operand.memory.index];
            if (is_lea) {
                add_evidence(index, TypeEvidence::UsedInArithmetic, 64);
            } else {
                add_evidence(index, TypeEvidence::UsedInPointerArithmetic, 64);
            }
        }
    }
}

FunctionTypeInfo TypeInference::analyze(const CFG& cfg, const FunctionSignature& signature) const {
    FunctionTypeInfo result;

    for (const auto& block : cfg.blocks) {
        for (std::size_t instruction_index = 0; instruction_index < block.instructions.size(); ++instruction_index) {
            const auto& instruction = block.instructions[instruction_index];

            for (const auto& operand : instruction.operands) {
                collect_operand_evidence(result, instruction, operand);
            }

            if ((instruction.opcode == x86::Opcode::Cmp || instruction.opcode == x86::Opcode::Test) &&
                instruction.operands.size() >= 2) {
                const auto& lhs = instruction.operands[0];
                const auto& rhs = instruction.operands[1];
                const bool compares_zero = (rhs.kind == x86::OperandKind::Immediate && rhs.immediate == 0) ||
                                           (instruction.opcode == x86::Opcode::Test && lhs.kind == x86::OperandKind::Register &&
                                            rhs.kind == x86::OperandKind::Register && lhs.reg == rhs.reg);
                if (compares_zero && lhs.kind == x86::OperandKind::Register) {
                    add_evidence(result.register_types[lhs.reg], TypeEvidence::ComparedToZero, lhs.width_bits);
                    if (instruction_index + 1 < block.instructions.size() &&
                        block.instructions[instruction_index + 1].opcode == x86::Opcode::Jcc) {
                        const auto condition = block.instructions[instruction_index + 1].condition;
                        add_evidence(result.register_types[lhs.reg], TypeEvidence::UsedAsCondition, lhs.width_bits);
                        if (condition != x86::ConditionCode::E && condition != x86::ConditionCode::NE) {
                            add_evidence(result.register_types[lhs.reg], TypeEvidence::UsedInArithmetic, lhs.width_bits);
                        }
                    }
                }
            }
        }
    }

    for (auto& [reg, info] : result.register_types) {
        (void)reg;
        info.category = resolve(info);
        const bool pointer_evidence = has(info, TypeEvidence::UsedAsAddress) || has(info, TypeEvidence::Dereferenced);
        const bool integer_evidence = has(info, TypeEvidence::UsedInArithmetic);
        info.conflicting = pointer_evidence && integer_evidence && !has(info, TypeEvidence::UsedInPointerArithmetic);
    }

    if (signature.return_value_known && signature.returns_value) {
        const auto iterator = result.register_types.find(x86::Register::RAX);
        if (iterator != result.register_types.end()) {
            result.return_type = iterator->second;
        }
        if (result.return_type.category == TypeCategory::Unknown) {
            result.return_type.category = TypeCategory::Integer;
            result.return_type.width_bits = signature.return_width_bits;
        }
    }

    return result;
}

std::string type_category_name(TypeCategory category) {
    switch (category) {
        case TypeCategory::Unknown: return "Unknown";
        case TypeCategory::Integer: return "Integer";
        case TypeCategory::Pointer: return "Pointer";
        case TypeCategory::BooleanLike: return "Boolean-like";
        case TypeCategory::Float: return "Float";
    }
    return "Unknown";
}

std::string format_type_info(const TypeInfo& info) {
    std::ostringstream output;
    output << type_category_name(info.category);
    if (info.width_bits != 0) {
        output << info.width_bits;
    }
    if (info.conflicting) {
        output << " (conflicting evidence)";
    }
    return output.str();
}

} // namespace mykisah::core
