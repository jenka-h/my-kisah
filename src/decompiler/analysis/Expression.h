#pragma once

#include "decompiler/ir/IR.h"
#include "frontend/x86/Instruction.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace mykisah::core {

enum class ExpressionKind {
    Unknown,
    Variable,
    Constant,
    Binary,
    Comparison,
    Select,
    AddressOf,
    Memory,
    Call,
};

struct Expression;
using ExprPtr = std::shared_ptr<Expression>;

struct SSAVariable {
    std::string name;
    uint32_t version = 0;
    uint16_t width_bits = 0;
};

struct Expression {
    ExpressionKind kind = ExpressionKind::Unknown;
    uint16_t width_bits = 0;

    SSAVariable variable;
    int64_t constant = 0;
    IRBinaryOperator binary_operator = IRBinaryOperator::None;
    x86::ConditionCode condition = x86::ConditionCode::None;
    x86::MemoryOperand memory;
    uint64_t call_target = 0;
    std::vector<ExprPtr> children;

    [[nodiscard]] static ExprPtr make_unknown(uint16_t width_bits = 0);
    [[nodiscard]] static ExprPtr make_variable(std::string name, uint32_t version, uint16_t width_bits);
    [[nodiscard]] static ExprPtr make_constant(int64_t value, uint16_t width_bits);
    [[nodiscard]] static ExprPtr make_binary(IRBinaryOperator op, ExprPtr lhs, ExprPtr rhs, uint16_t width_bits);
    [[nodiscard]] static ExprPtr make_comparison(x86::ConditionCode condition, ExprPtr lhs, ExprPtr rhs);
    [[nodiscard]] static ExprPtr make_select(ExprPtr condition, ExprPtr when_true, ExprPtr when_false, uint16_t width_bits);
    [[nodiscard]] static ExprPtr make_address_of(x86::MemoryOperand memory, uint16_t width_bits);
    [[nodiscard]] static ExprPtr make_memory(x86::MemoryOperand memory, uint16_t width_bits);
    [[nodiscard]] static ExprPtr make_call(uint64_t target, std::vector<ExprPtr> arguments, uint16_t width_bits);
};

[[nodiscard]] std::string format_expression(const ExprPtr& expression, bool include_ssa_versions = true);
[[nodiscard]] std::string canonical_register_name(x86::Register reg);
[[nodiscard]] std::string abi_argument_name(x86::Register reg);
[[nodiscard]] bool is_abi_argument_register(x86::Register reg);

} // namespace mykisah::core
