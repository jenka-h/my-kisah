#pragma once

#include "frontend/x86/Instruction.h"

#include <cstdint>
#include <string>
#include <vector>

namespace mykisah::core {

enum class IRValueKind {
    Unknown,
    Register,
    Constant,
    Memory,
    Address,
};

enum class IROpcode {
    Unknown,
    Assign,
    LoadEffectiveAddress,
    BinaryOp,
    Compare,
    Test,
    ConditionalSelect,
    SetCondition,
    Push,
    Pop,
    Call,
    Jump,
    Branch,
    Return,
};

enum class IRBinaryOperator {
    None,
    Add,
    Sub,
    And,
    Xor,
    Mul,
};

struct IRValue {
    IRValueKind kind = IRValueKind::Unknown;
    uint16_t width_bits = 0;

    x86::Register reg = x86::Register::None;
    int64_t constant = 0;
    uint64_t address = 0;
    x86::MemoryOperand memory;

    [[nodiscard]] static IRValue unknown(uint16_t width_bits = 0);
    [[nodiscard]] static IRValue make_register(x86::Register reg, uint16_t width_bits);
    [[nodiscard]] static IRValue make_constant(int64_t value, uint16_t width_bits);
    [[nodiscard]] static IRValue make_memory(x86::MemoryOperand memory, uint16_t width_bits);
    [[nodiscard]] static IRValue make_address(uint64_t address, uint16_t width_bits = 64);
};

struct IRInstruction {
    uint64_t source_address = 0;
    IROpcode opcode = IROpcode::Unknown;
    IRBinaryOperator binary_operator = IRBinaryOperator::None;
    x86::ConditionCode condition = x86::ConditionCode::None;

    IRValue destination;
    std::vector<IRValue> sources;
    uint64_t branch_target = 0;

    std::vector<uint8_t> source_bytes;
};

struct IRFunction {
    std::string name;
    uint64_t address = 0;
    std::vector<IRInstruction> instructions;
};

[[nodiscard]] std::string format_ir_value(const IRValue& value);
[[nodiscard]] std::string format_ir_instruction(const IRInstruction& instruction);
[[nodiscard]] std::string ir_opcode_name(IROpcode opcode);
[[nodiscard]] std::string ir_binary_operator_name(IRBinaryOperator op);

} // namespace mykisah::core
