#include "frontend/x86/Instruction.h"

#include <array>

namespace mykisah::x86 {

Operand Operand::make_register(Register reg, uint16_t width_bits, OperandAccess access) {
    Operand operand;
    operand.kind = OperandKind::Register;
    operand.reg = reg;
    operand.width_bits = width_bits;
    operand.access = access;
    return operand;
}

Operand Operand::make_immediate(int64_t value, uint16_t width_bits) {
    Operand operand;
    operand.kind = OperandKind::Immediate;
    operand.immediate = value;
    operand.width_bits = width_bits;
    operand.access = OperandAccess::Read;
    return operand;
}

Operand Operand::make_memory(MemoryOperand memory, uint16_t width_bits, OperandAccess access) {
    Operand operand;
    operand.kind = OperandKind::Memory;
    operand.memory = memory;
    operand.width_bits = width_bits;
    operand.access = access;
    return operand;
}

Operand Operand::make_relative(uint64_t target, uint16_t width_bits) {
    Operand operand;
    operand.kind = OperandKind::RelativeAddress;
    operand.relative_target = target;
    operand.width_bits = width_bits;
    operand.access = OperandAccess::Read;
    return operand;
}

uint64_t Instruction::size() const {
    return encoding.bytes.size();
}

std::string register_name(Register reg, uint16_t width_bits) {
    switch (reg) {
        case Register::RAX:
            return width_bits == 32 ? "eax" : width_bits == 16 ? "ax" : width_bits == 8 ? "al" : "rax";
        case Register::RCX:
            return width_bits == 32 ? "ecx" : width_bits == 16 ? "cx" : width_bits == 8 ? "cl" : "rcx";
        case Register::RDX:
            return width_bits == 32 ? "edx" : width_bits == 16 ? "dx" : width_bits == 8 ? "dl" : "rdx";
        case Register::RBX:
            return width_bits == 32 ? "ebx" : width_bits == 16 ? "bx" : width_bits == 8 ? "bl" : "rbx";
        case Register::RSP:
            return width_bits == 32 ? "esp" : width_bits == 16 ? "sp" : width_bits == 8 ? "spl" : "rsp";
        case Register::RBP:
            return width_bits == 32 ? "ebp" : width_bits == 16 ? "bp" : width_bits == 8 ? "bpl" : "rbp";
        case Register::RSI:
            return width_bits == 32 ? "esi" : width_bits == 16 ? "si" : width_bits == 8 ? "sil" : "rsi";
        case Register::RDI:
            return width_bits == 32 ? "edi" : width_bits == 16 ? "di" : width_bits == 8 ? "dil" : "rdi";
        case Register::R8:
            return width_bits == 32 ? "r8d" : width_bits == 16 ? "r8w" : width_bits == 8 ? "r8b" : "r8";
        case Register::R9:
            return width_bits == 32 ? "r9d" : width_bits == 16 ? "r9w" : width_bits == 8 ? "r9b" : "r9";
        case Register::R10:
            return width_bits == 32 ? "r10d" : width_bits == 16 ? "r10w" : width_bits == 8 ? "r10b" : "r10";
        case Register::R11:
            return width_bits == 32 ? "r11d" : width_bits == 16 ? "r11w" : width_bits == 8 ? "r11b" : "r11";
        case Register::R12:
            return width_bits == 32 ? "r12d" : width_bits == 16 ? "r12w" : width_bits == 8 ? "r12b" : "r12";
        case Register::R13:
            return width_bits == 32 ? "r13d" : width_bits == 16 ? "r13w" : width_bits == 8 ? "r13b" : "r13";
        case Register::R14:
            return width_bits == 32 ? "r14d" : width_bits == 16 ? "r14w" : width_bits == 8 ? "r14b" : "r14";
        case Register::R15:
            return width_bits == 32 ? "r15d" : width_bits == 16 ? "r15w" : width_bits == 8 ? "r15b" : "r15";
        case Register::RIP:
            return "rip";
        case Register::EFLAGS:
            return "eflags";
        case Register::None:
            return "none";
    }

    return "unknown";
}

std::string opcode_name(Opcode opcode, ConditionCode condition) {
    switch (opcode) {
        case Opcode::Invalid:
            return "invalid";
        case Opcode::Nop:
            return "nop";
        case Opcode::Mov:
            return "mov";
        case Opcode::Lea:
            return "lea";
        case Opcode::Push:
            return "push";
        case Opcode::Pop:
            return "pop";
        case Opcode::Add:
            return "add";
        case Opcode::Sub:
            return "sub";
        case Opcode::And:
            return "and";
        case Opcode::Xor:
            return "xor";
        case Opcode::Imul:
            return "imul";
        case Opcode::Cmp:
            return "cmp";
        case Opcode::Test:
            return "test";
        case Opcode::Cmov:
            return "cmov" + opcode_name(Opcode::Jcc, condition).substr(1);
        case Opcode::Setcc:
            return "set" + opcode_name(Opcode::Jcc, condition).substr(1);
        case Opcode::Jmp:
            return "jmp";
        case Opcode::Jcc:
            switch (condition) {
                case ConditionCode::O: return "jo";
                case ConditionCode::NO: return "jno";
                case ConditionCode::B: return "jb";
                case ConditionCode::AE: return "jae";
                case ConditionCode::E: return "je";
                case ConditionCode::NE: return "jne";
                case ConditionCode::BE: return "jbe";
                case ConditionCode::A: return "ja";
                case ConditionCode::S: return "js";
                case ConditionCode::NS: return "jns";
                case ConditionCode::P: return "jp";
                case ConditionCode::NP: return "jnp";
                case ConditionCode::L: return "jl";
                case ConditionCode::GE: return "jge";
                case ConditionCode::LE: return "jle";
                case ConditionCode::G: return "jg";
                case ConditionCode::None: return "jcc";
            }
            return "jcc";
        case Opcode::Call:
            return "call";
        case Opcode::Ret:
            return "ret";
    }

    return "unknown";
}

bool is_valid_instruction(const Instruction& instruction) {
    return instruction.opcode != Opcode::Invalid;
}

} // namespace mykisah::x86
