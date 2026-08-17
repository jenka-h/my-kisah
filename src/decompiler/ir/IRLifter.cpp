#include "decompiler/ir/IRLifter.h"

namespace mykisah::core {
namespace {

IRBinaryOperator binary_operator_for(x86::Opcode opcode) {
    switch (opcode) {
        case x86::Opcode::Add: return IRBinaryOperator::Add;
        case x86::Opcode::Sub: return IRBinaryOperator::Sub;
        case x86::Opcode::And: return IRBinaryOperator::And;
        case x86::Opcode::Xor: return IRBinaryOperator::Xor;
        case x86::Opcode::Imul: return IRBinaryOperator::Mul;
        default: return IRBinaryOperator::None;
    }
}

IRInstruction make_unknown(const x86::Instruction& instruction) {
    IRInstruction ir;
    ir.source_address = instruction.address;
    ir.opcode = IROpcode::Unknown;
    ir.source_bytes = instruction.encoding.bytes;
    return ir;
}

} // namespace

IRValue IRLifter::lift_operand(const x86::Operand& operand) const {
    switch (operand.kind) {
        case x86::OperandKind::None:
            return IRValue::unknown(operand.width_bits);
        case x86::OperandKind::Register:
            return IRValue::make_register(operand.reg, operand.width_bits);
        case x86::OperandKind::Immediate:
            return IRValue::make_constant(operand.immediate, operand.width_bits);
        case x86::OperandKind::Memory:
            return IRValue::make_memory(operand.memory, operand.width_bits);
        case x86::OperandKind::RelativeAddress:
            return IRValue::make_address(operand.relative_target, operand.width_bits);
    }

    return IRValue::unknown(operand.width_bits);
}

IRFunction IRLifter::lift_function(
    const elf::Function& function,
    const std::vector<x86::Instruction>& instructions) const {
    IRFunction ir_function;
    ir_function.name = function.name;
    ir_function.address = function.address;

    for (const auto& instruction : instructions) {
        IRInstruction ir;
        ir.source_address = instruction.address;
        ir.source_bytes = instruction.encoding.bytes;

        switch (instruction.opcode) {
            case x86::Opcode::Mov:
                if (instruction.operands.size() >= 2) {
                    ir.opcode = IROpcode::Assign;
                    ir.destination = lift_operand(instruction.operands[0]);
                    ir.sources.push_back(lift_operand(instruction.operands[1]));
                } else {
                    ir = make_unknown(instruction);
                }
                break;
            case x86::Opcode::Lea:
                if (instruction.operands.size() >= 2) {
                    ir.opcode = IROpcode::LoadEffectiveAddress;
                    ir.destination = lift_operand(instruction.operands[0]);
                    ir.sources.push_back(lift_operand(instruction.operands[1]));
                } else {
                    ir = make_unknown(instruction);
                }
                break;
            case x86::Opcode::Add:
            case x86::Opcode::Sub:
            case x86::Opcode::And:
            case x86::Opcode::Xor:
            case x86::Opcode::Imul:
                if (instruction.operands.size() >= 2) {
                    ir.opcode = IROpcode::BinaryOp;
                    ir.binary_operator = binary_operator_for(instruction.opcode);
                    ir.destination = lift_operand(instruction.operands[0]);
                    ir.sources.push_back(lift_operand(instruction.operands[1]));
                } else {
                    ir = make_unknown(instruction);
                }
                break;
            case x86::Opcode::Cmp:
                if (instruction.operands.size() >= 2) {
                    ir.opcode = IROpcode::Compare;
                    ir.sources.push_back(lift_operand(instruction.operands[0]));
                    ir.sources.push_back(lift_operand(instruction.operands[1]));
                } else {
                    ir = make_unknown(instruction);
                }
                break;
            case x86::Opcode::Test:
                if (instruction.operands.size() >= 2) {
                    ir.opcode = IROpcode::Test;
                    ir.sources.push_back(lift_operand(instruction.operands[0]));
                    ir.sources.push_back(lift_operand(instruction.operands[1]));
                } else {
                    ir = make_unknown(instruction);
                }
                break;
            case x86::Opcode::Cmov:
                if (instruction.operands.size() >= 2) {
                    ir.opcode = IROpcode::ConditionalSelect;
                    ir.condition = instruction.condition;
                    ir.destination = lift_operand(instruction.operands[0]);
                    ir.sources.push_back(lift_operand(instruction.operands[1]));
                } else {
                    ir = make_unknown(instruction);
                }
                break;
            case x86::Opcode::Setcc:
                if (!instruction.operands.empty()) {
                    ir.opcode = IROpcode::SetCondition;
                    ir.condition = instruction.condition;
                    ir.destination = lift_operand(instruction.operands[0]);
                } else {
                    ir = make_unknown(instruction);
                }
                break;
            case x86::Opcode::Push:
                ir.opcode = IROpcode::Push;
                if (!instruction.operands.empty()) {
                    ir.sources.push_back(lift_operand(instruction.operands.front()));
                }
                break;
            case x86::Opcode::Pop:
                ir.opcode = IROpcode::Pop;
                if (!instruction.operands.empty()) {
                    ir.destination = lift_operand(instruction.operands.front());
                }
                break;
            case x86::Opcode::Call:
                ir.opcode = IROpcode::Call;
                if (!instruction.operands.empty()) {
                    ir.sources.push_back(lift_operand(instruction.operands.front()));
                    if (instruction.operands.front().kind == x86::OperandKind::RelativeAddress) {
                        ir.branch_target = instruction.operands.front().relative_target;
                    }
                }
                break;
            case x86::Opcode::Jmp:
                ir.opcode = IROpcode::Jump;
                if (!instruction.operands.empty()) {
                    ir.sources.push_back(lift_operand(instruction.operands.front()));
                    if (instruction.operands.front().kind == x86::OperandKind::RelativeAddress) {
                        ir.branch_target = instruction.operands.front().relative_target;
                    }
                }
                break;
            case x86::Opcode::Jcc:
                ir.opcode = IROpcode::Branch;
                ir.condition = instruction.condition;
                if (!instruction.operands.empty()) {
                    ir.sources.push_back(lift_operand(instruction.operands.front()));
                    if (instruction.operands.front().kind == x86::OperandKind::RelativeAddress) {
                        ir.branch_target = instruction.operands.front().relative_target;
                    }
                }
                break;
            case x86::Opcode::Ret:
                ir.opcode = IROpcode::Return;
                ir.sources.push_back(IRValue::make_register(x86::Register::RAX, 64));
                break;
            case x86::Opcode::Nop:
                continue;
            case x86::Opcode::Invalid:
                ir = make_unknown(instruction);
                break;
        }

        ir_function.instructions.push_back(ir);
    }

    return ir_function;
}

} // namespace mykisah::core
