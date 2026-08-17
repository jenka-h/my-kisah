#include "decompiler/analysis/Expression.h"

#include <sstream>
#include <utility>

namespace mykisah::core {
namespace {

std::string hex_i64(int64_t value) {
    std::ostringstream output;
    if (value < 0) {
        output << "-0x" << std::hex << static_cast<uint64_t>(-value);
    } else {
        output << "0x" << std::hex << static_cast<uint64_t>(value);
    }
    return output.str();
}

std::string format_memory_operand(const x86::MemoryOperand& memory, bool include_ssa_versions) {
    std::ostringstream output;
    output << '[';
    bool wrote = false;

    if (memory.base != x86::Register::None) {
        output << canonical_register_name(memory.base);
        if (include_ssa_versions && is_abi_argument_register(memory.base)) {
            output << "/*" << abi_argument_name(memory.base) << "*/";
        }
        wrote = true;
    }

    if (memory.index != x86::Register::None) {
        if (wrote) {
            output << " + ";
        }
        output << canonical_register_name(memory.index);
        if (memory.scale != 1) {
            output << " * " << static_cast<unsigned>(memory.scale);
        }
        wrote = true;
    }

    if (memory.displacement != 0) {
        if (wrote) {
            output << (memory.displacement < 0 ? " - " : " + ") << "0x" << std::hex
                   << static_cast<uint64_t>(memory.displacement < 0 ? -memory.displacement : memory.displacement);
        } else {
            output << hex_i64(memory.displacement);
        }
        wrote = true;
    }

    if (!wrote) {
        output << '0';
    }

    output << ']';
    return output.str();
}

} // namespace

ExprPtr Expression::make_unknown(uint16_t width_bits) {
    auto expression = std::make_shared<Expression>();
    expression->kind = ExpressionKind::Unknown;
    expression->width_bits = width_bits;
    return expression;
}

ExprPtr Expression::make_variable(std::string name, uint32_t version, uint16_t width_bits) {
    auto expression = std::make_shared<Expression>();
    expression->kind = ExpressionKind::Variable;
    expression->variable = SSAVariable{std::move(name), version, width_bits};
    expression->width_bits = width_bits;
    return expression;
}

ExprPtr Expression::make_constant(int64_t value, uint16_t width_bits) {
    auto expression = std::make_shared<Expression>();
    expression->kind = ExpressionKind::Constant;
    expression->constant = value;
    expression->width_bits = width_bits;
    return expression;
}

ExprPtr Expression::make_binary(IRBinaryOperator op, ExprPtr lhs, ExprPtr rhs, uint16_t width_bits) {
    auto expression = std::make_shared<Expression>();
    expression->kind = ExpressionKind::Binary;
    expression->binary_operator = op;
    expression->children.push_back(std::move(lhs));
    expression->children.push_back(std::move(rhs));
    expression->width_bits = width_bits;
    return expression;
}

ExprPtr Expression::make_comparison(x86::ConditionCode condition, ExprPtr lhs, ExprPtr rhs) {
    auto expression = std::make_shared<Expression>();
    expression->kind = ExpressionKind::Comparison;
    expression->condition = condition;
    expression->children.push_back(std::move(lhs));
    expression->children.push_back(std::move(rhs));
    expression->width_bits = 1;
    return expression;
}

ExprPtr Expression::make_select(ExprPtr condition, ExprPtr when_true, ExprPtr when_false, uint16_t width_bits) {
    auto expression = std::make_shared<Expression>();
    expression->kind = ExpressionKind::Select;
    expression->children.push_back(std::move(condition));
    expression->children.push_back(std::move(when_true));
    expression->children.push_back(std::move(when_false));
    expression->width_bits = width_bits;
    return expression;
}

ExprPtr Expression::make_address_of(x86::MemoryOperand memory, uint16_t width_bits) {
    auto expression = std::make_shared<Expression>();
    expression->kind = ExpressionKind::AddressOf;
    expression->memory = memory;
    expression->width_bits = width_bits;
    return expression;
}

ExprPtr Expression::make_memory(x86::MemoryOperand memory, uint16_t width_bits) {
    auto expression = std::make_shared<Expression>();
    expression->kind = ExpressionKind::Memory;
    expression->memory = memory;
    expression->width_bits = width_bits;
    return expression;
}

ExprPtr Expression::make_call(uint64_t target, std::vector<ExprPtr> arguments, uint16_t width_bits) {
    auto expression = std::make_shared<Expression>();
    expression->kind = ExpressionKind::Call;
    expression->call_target = target;
    expression->children = std::move(arguments);
    expression->width_bits = width_bits;
    return expression;
}

std::string canonical_register_name(x86::Register reg) {
    switch (reg) {
        case x86::Register::RAX: return "rax";
        case x86::Register::RCX: return "rcx";
        case x86::Register::RDX: return "rdx";
        case x86::Register::RBX: return "rbx";
        case x86::Register::RSP: return "rsp";
        case x86::Register::RBP: return "rbp";
        case x86::Register::RSI: return "rsi";
        case x86::Register::RDI: return "rdi";
        case x86::Register::R8: return "r8";
        case x86::Register::R9: return "r9";
        case x86::Register::R10: return "r10";
        case x86::Register::R11: return "r11";
        case x86::Register::R12: return "r12";
        case x86::Register::R13: return "r13";
        case x86::Register::R14: return "r14";
        case x86::Register::R15: return "r15";
        case x86::Register::RIP: return "rip";
        case x86::Register::EFLAGS: return "eflags";
        case x86::Register::None: return "none";
    }
    return "unknown";
}

bool is_abi_argument_register(x86::Register reg) {
    return reg == x86::Register::RDI || reg == x86::Register::RSI || reg == x86::Register::RDX ||
           reg == x86::Register::RCX || reg == x86::Register::R8 || reg == x86::Register::R9;
}

std::string abi_argument_name(x86::Register reg) {
    switch (reg) {
        case x86::Register::RDI: return "arg0";
        case x86::Register::RSI: return "arg1";
        case x86::Register::RDX: return "arg2";
        case x86::Register::RCX: return "arg3";
        case x86::Register::R8: return "arg4";
        case x86::Register::R9: return "arg5";
        default: return "";
    }
}

std::string format_expression(const ExprPtr& expression, bool include_ssa_versions) {
    if (!expression) {
        return "unknown";
    }

    switch (expression->kind) {
        case ExpressionKind::Unknown:
            return "unknown";
        case ExpressionKind::Variable: {
            std::ostringstream output;
            output << expression->variable.name;
            if (include_ssa_versions) {
                output << '_' << expression->variable.version;
            }
            return output.str();
        }
        case ExpressionKind::Constant:
            return hex_i64(expression->constant);
        case ExpressionKind::Binary:
            if (expression->children.size() < 2) {
                return "unknown_binary";
            }
            return "(" + format_expression(expression->children[0], include_ssa_versions) + " " +
                   ir_binary_operator_name(expression->binary_operator) + " " +
                   format_expression(expression->children[1], include_ssa_versions) + ")";
        case ExpressionKind::Comparison: {
            if (expression->children.size() < 2) {
                return "unknown_condition";
            }
            std::string op;
            switch (expression->condition) {
                case x86::ConditionCode::E: op = "=="; break;
                case x86::ConditionCode::NE: op = "!="; break;
                case x86::ConditionCode::L: op = "<"; break;
                case x86::ConditionCode::LE: op = "<="; break;
                case x86::ConditionCode::G: op = ">"; break;
                case x86::ConditionCode::GE: op = ">="; break;
                case x86::ConditionCode::B: op = "<"; break;
                case x86::ConditionCode::BE: op = "<="; break;
                case x86::ConditionCode::A: op = ">"; break;
                case x86::ConditionCode::AE: op = ">="; break;
                default: op = "?"; break;
            }
            return "(" + format_expression(expression->children[0], include_ssa_versions) + " " + op + " " +
                   format_expression(expression->children[1], include_ssa_versions) + ")";
        }
        case ExpressionKind::Select:
            if (expression->children.size() < 3) {
                return "unknown_select";
            }
            return "(" + format_expression(expression->children[0], include_ssa_versions) + " ? " +
                   format_expression(expression->children[1], include_ssa_versions) + " : " +
                   format_expression(expression->children[2], include_ssa_versions) + ")";
        case ExpressionKind::AddressOf:
            return "address_of " + format_memory_operand(expression->memory, include_ssa_versions);
        case ExpressionKind::Memory:
            return "*" + format_memory_operand(expression->memory, include_ssa_versions);
        case ExpressionKind::Call: {
            std::ostringstream output;
            output << "call_0x" << std::hex << expression->call_target << '(';
            for (std::size_t i = 0; i < expression->children.size(); ++i) {
                if (i != 0) {
                    output << ", ";
                }
                output << format_expression(expression->children[i], include_ssa_versions);
            }
            output << ')';
            return output.str();
        }
    }

    return "unknown";
}

} // namespace mykisah::core
