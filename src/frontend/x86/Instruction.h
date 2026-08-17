#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mykisah::x86 {

enum class Register {
    None,

    RAX,
    RCX,
    RDX,
    RBX,
    RSP,
    RBP,
    RSI,
    RDI,
    R8,
    R9,
    R10,
    R11,
    R12,
    R13,
    R14,
    R15,

    RIP,
    EFLAGS,
};

enum class Opcode {
    Invalid,
    Nop,

    Mov,
    Lea,
    Push,
    Pop,
    Add,
    Sub,
    And,
    Xor,
    Imul,
    Cmp,
    Test,
    Cmov,
    Setcc,
    Jmp,
    Jcc,
    Call,
    Ret,
};

enum class OperandKind {
    None,
    Register,
    Immediate,
    Memory,
    RelativeAddress,
};

enum class ConditionCode {
    None,
    O,
    NO,
    B,
    AE,
    E,
    NE,
    BE,
    A,
    S,
    NS,
    P,
    NP,
    L,
    GE,
    LE,
    G,
};

enum class OperandAccess : uint8_t {
    None = 0,
    Read = 1,
    Write = 2,
    ReadWrite = 3,
};

struct MemoryOperand {
    Register segment = Register::None;
    Register base = Register::None;
    Register index = Register::None;
    uint8_t scale = 1;
    int64_t displacement = 0;
    bool rip_relative = false;
};

struct Operand {
    OperandKind kind = OperandKind::None;
    uint16_t width_bits = 0;
    OperandAccess access = OperandAccess::None;

    Register reg = Register::None;
    int64_t immediate = 0;
    uint64_t relative_target = 0;
    MemoryOperand memory;

    [[nodiscard]] static Operand make_register(Register reg, uint16_t width_bits, OperandAccess access = OperandAccess::None);
    [[nodiscard]] static Operand make_immediate(int64_t value, uint16_t width_bits);
    [[nodiscard]] static Operand make_memory(MemoryOperand memory, uint16_t width_bits, OperandAccess access = OperandAccess::None);
    [[nodiscard]] static Operand make_relative(uint64_t target, uint16_t width_bits);
};

struct InstructionEncoding {
    std::vector<uint8_t> bytes;
    bool has_rex = false;
    uint8_t rex = 0;
    bool has_modrm = false;
    uint8_t modrm = 0;
    bool has_sib = false;
    uint8_t sib = 0;
};

struct Instruction {
    uint64_t address = 0;
    Opcode opcode = Opcode::Invalid;
    ConditionCode condition = ConditionCode::None;
    std::vector<Operand> operands;
    InstructionEncoding encoding;

    bool is_control_flow = false;
    bool is_conditional_branch = false;
    bool is_call = false;
    bool is_return = false;

    [[nodiscard]] uint64_t size() const;
};

[[nodiscard]] std::string register_name(Register reg, uint16_t width_bits);
[[nodiscard]] std::string opcode_name(Opcode opcode, ConditionCode condition = ConditionCode::None);
[[nodiscard]] bool is_valid_instruction(const Instruction& instruction);

} // namespace mykisah::x86
