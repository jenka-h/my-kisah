#include "decompiler/ir/IR.h"

#include <iomanip>
#include <sstream>

namespace mykisah::core {
namespace {

std::string hex_u64(uint64_t value) {
    std::ostringstream output;
    output << "0x" << std::hex << value;
    return output.str();
}

std::string format_memory(const x86::MemoryOperand& memory) {
    std::ostringstream output;
    output << '[';

    bool wrote = false;
    if (memory.base != x86::Register::None) {
        output << x86::register_name(memory.base, 64);
        wrote = true;
    }

    if (memory.index != x86::Register::None) {
        if (wrote) {
            output << " + ";
        }
        output << x86::register_name(memory.index, 64);
        if (memory.scale != 1) {
            output << " * " << static_cast<unsigned>(memory.scale);
        }
        wrote = true;
    }

    if (memory.displacement != 0) {
        if (wrote) {
            output << (memory.displacement < 0 ? " - " : " + ");
            output << hex_u64(static_cast<uint64_t>(memory.displacement < 0 ? -memory.displacement : memory.displacement));
        } else if (memory.displacement < 0) {
            output << '-' << hex_u64(static_cast<uint64_t>(-memory.displacement));
        } else {
            output << hex_u64(static_cast<uint64_t>(memory.displacement));
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

IRValue IRValue::unknown(uint16_t width_bits) {
    IRValue value;
    value.kind = IRValueKind::Unknown;
    value.width_bits = width_bits;
    return value;
}

IRValue IRValue::make_register(x86::Register reg, uint16_t width_bits) {
    IRValue value;
    value.kind = IRValueKind::Register;
    value.reg = reg;
    value.width_bits = width_bits;
    return value;
}

IRValue IRValue::make_constant(int64_t constant, uint16_t width_bits) {
    IRValue value;
    value.kind = IRValueKind::Constant;
    value.constant = constant;
    value.width_bits = width_bits;
    return value;
}

IRValue IRValue::make_memory(x86::MemoryOperand memory, uint16_t width_bits) {
    IRValue value;
    value.kind = IRValueKind::Memory;
    value.memory = memory;
    value.width_bits = width_bits;
    return value;
}

IRValue IRValue::make_address(uint64_t address, uint16_t width_bits) {
    IRValue value;
    value.kind = IRValueKind::Address;
    value.address = address;
    value.width_bits = width_bits;
    return value;
}

std::string ir_opcode_name(IROpcode opcode) {
    switch (opcode) {
        case IROpcode::Unknown: return "unknown";
        case IROpcode::Assign: return "assign";
        case IROpcode::LoadEffectiveAddress: return "lea";
        case IROpcode::BinaryOp: return "binary";
        case IROpcode::Compare: return "compare";
        case IROpcode::Test: return "test";
        case IROpcode::ConditionalSelect: return "select";
        case IROpcode::SetCondition: return "set_condition";
        case IROpcode::Push: return "push";
        case IROpcode::Pop: return "pop";
        case IROpcode::Call: return "call";
        case IROpcode::Jump: return "jump";
        case IROpcode::Branch: return "branch";
        case IROpcode::Return: return "return";
    }

    return "unknown";
}

std::string ir_binary_operator_name(IRBinaryOperator op) {
    switch (op) {
        case IRBinaryOperator::None: return "?";
        case IRBinaryOperator::Add: return "+";
        case IRBinaryOperator::Sub: return "-";
        case IRBinaryOperator::And: return "&";
        case IRBinaryOperator::Xor: return "^";
        case IRBinaryOperator::Mul: return "*";
    }

    return "?";
}

std::string format_ir_value(const IRValue& value) {
    switch (value.kind) {
        case IRValueKind::Unknown:
            return "unknown";
        case IRValueKind::Register:
            return x86::register_name(value.reg, value.width_bits);
        case IRValueKind::Constant:
            if (value.constant < 0) {
                return "-" + hex_u64(static_cast<uint64_t>(-value.constant));
            }
            return hex_u64(static_cast<uint64_t>(value.constant));
        case IRValueKind::Memory:
            return "mem" + std::to_string(value.width_bits) + format_memory(value.memory);
        case IRValueKind::Address:
            return hex_u64(value.address);
    }

    return "unknown";
}

std::string format_ir_instruction(const IRInstruction& instruction) {
    std::ostringstream output;

    switch (instruction.opcode) {
        case IROpcode::Assign:
            output << format_ir_value(instruction.destination) << " = "
                   << (instruction.sources.empty() ? "unknown" : format_ir_value(instruction.sources.front()));
            break;
        case IROpcode::LoadEffectiveAddress:
            output << format_ir_value(instruction.destination) << " = address_of "
                   << (instruction.sources.empty() ? "unknown" : format_ir_value(instruction.sources.front()));
            break;
        case IROpcode::BinaryOp:
            output << format_ir_value(instruction.destination) << " = "
                   << format_ir_value(instruction.destination) << ' '
                   << ir_binary_operator_name(instruction.binary_operator) << ' '
                   << (instruction.sources.empty() ? "unknown" : format_ir_value(instruction.sources.front()));
            break;
        case IROpcode::Compare:
            output << "flags = compare "
                   << (instruction.sources.size() > 0 ? format_ir_value(instruction.sources[0]) : "unknown")
                   << ", "
                   << (instruction.sources.size() > 1 ? format_ir_value(instruction.sources[1]) : "unknown");
            break;
        case IROpcode::Test:
            output << "flags = test "
                   << (instruction.sources.size() > 0 ? format_ir_value(instruction.sources[0]) : "unknown")
                   << ", "
                   << (instruction.sources.size() > 1 ? format_ir_value(instruction.sources[1]) : "unknown");
            break;
        case IROpcode::ConditionalSelect:
            output << format_ir_value(instruction.destination) << " = select "
                   << x86::opcode_name(x86::Opcode::Jcc, instruction.condition) << ", "
                   << (instruction.sources.empty() ? "unknown" : format_ir_value(instruction.sources.front()));
            break;
        case IROpcode::SetCondition:
            output << format_ir_value(instruction.destination) << " = condition "
                   << x86::opcode_name(x86::Opcode::Jcc, instruction.condition);
            break;
        case IROpcode::Push:
            output << "push " << (instruction.sources.empty() ? "unknown" : format_ir_value(instruction.sources.front()));
            break;
        case IROpcode::Pop:
            output << format_ir_value(instruction.destination) << " = pop";
            break;
        case IROpcode::Call:
            output << "call " << (instruction.sources.empty() ? "unknown" : format_ir_value(instruction.sources.front()));
            break;
        case IROpcode::Jump:
            output << "jump " << hex_u64(instruction.branch_target);
            break;
        case IROpcode::Branch:
            output << "branch " << x86::opcode_name(x86::Opcode::Jcc, instruction.condition)
                   << " " << hex_u64(instruction.branch_target);
            break;
        case IROpcode::Return:
            output << "return " << (instruction.sources.empty() ? "unknown" : format_ir_value(instruction.sources.front()));
            break;
        case IROpcode::Unknown:
            output << "/* unresolved instruction */";
            break;
    }

    return output.str();
}

} // namespace mykisah::core
