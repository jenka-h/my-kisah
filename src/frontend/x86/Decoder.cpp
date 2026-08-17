#include "frontend/x86/Decoder.h"

#include <algorithm>
#include <cstddef>
#include <stdexcept>

namespace mykisah::x86 {
namespace {

struct Cursor {
    const std::vector<uint8_t>& bytes;
    std::size_t offset = 0;

    [[nodiscard]] bool has(std::size_t count) const {
        return count <= bytes.size() - offset;
    }

    uint8_t read_u8() {
        if (!has(1)) {
            throw std::out_of_range("decoder read beyond function bytes");
        }
        return bytes[offset++];
    }

    int8_t read_i8() {
        return static_cast<int8_t>(read_u8());
    }

    uint32_t read_u32() {
        if (!has(4)) {
            throw std::out_of_range("decoder read beyond function bytes");
        }
        const uint32_t value = static_cast<uint32_t>(bytes[offset]) |
            (static_cast<uint32_t>(bytes[offset + 1]) << 8U) |
            (static_cast<uint32_t>(bytes[offset + 2]) << 16U) |
            (static_cast<uint32_t>(bytes[offset + 3]) << 24U);
        offset += 4;
        return value;
    }

    int32_t read_i32() {
        return static_cast<int32_t>(read_u32());
    }
};

struct RexPrefix {
    bool present = false;
    bool w = false;
    bool r = false;
    bool x = false;
    bool b = false;
    uint8_t byte = 0;
};

struct ModRm {
    uint8_t byte = 0;
    uint8_t mod = 0;
    uint8_t reg = 0;
    uint8_t rm = 0;
};

Register register_from_code(uint8_t code) {
    switch (code & 0x0fU) {
        case 0: return Register::RAX;
        case 1: return Register::RCX;
        case 2: return Register::RDX;
        case 3: return Register::RBX;
        case 4: return Register::RSP;
        case 5: return Register::RBP;
        case 6: return Register::RSI;
        case 7: return Register::RDI;
        case 8: return Register::R8;
        case 9: return Register::R9;
        case 10: return Register::R10;
        case 11: return Register::R11;
        case 12: return Register::R12;
        case 13: return Register::R13;
        case 14: return Register::R14;
        case 15: return Register::R15;
        default: return Register::None;
    }
}

uint16_t operand_width(const RexPrefix& rex) {
    return rex.w ? 64 : 32;
}

ModRm read_modrm(Cursor& cursor) {
    const uint8_t byte = cursor.read_u8();
    ModRm modrm;
    modrm.byte = byte;
    modrm.mod = static_cast<uint8_t>((byte >> 6U) & 0x03U);
    modrm.reg = static_cast<uint8_t>((byte >> 3U) & 0x07U);
    modrm.rm = static_cast<uint8_t>(byte & 0x07U);
    return modrm;
}

Operand decode_rm_operand(Cursor& cursor, const RexPrefix& rex, const ModRm& modrm, uint16_t width, OperandAccess access) {
    const uint8_t rm_code = static_cast<uint8_t>(modrm.rm | (rex.b ? 8U : 0U));

    if (modrm.mod == 3) {
        return Operand::make_register(register_from_code(rm_code), width, access);
    }

    MemoryOperand memory;
    memory.scale = 1;

    if (modrm.rm == 4) {
        const uint8_t sib = cursor.read_u8();
        const uint8_t scale_bits = static_cast<uint8_t>((sib >> 6U) & 0x03U);
        const uint8_t index_bits = static_cast<uint8_t>((sib >> 3U) & 0x07U);
        const uint8_t base_bits = static_cast<uint8_t>(sib & 0x07U);

        memory.scale = static_cast<uint8_t>(1U << scale_bits);
        if (index_bits != 4 || rex.x) {
            memory.index = register_from_code(static_cast<uint8_t>(index_bits | (rex.x ? 8U : 0U)));
        }

        if (modrm.mod == 0 && base_bits == 5) {
            memory.base = Register::None;
            memory.displacement = cursor.read_i32();
        } else {
            memory.base = register_from_code(static_cast<uint8_t>(base_bits | (rex.b ? 8U : 0U)));
        }
    } else if (modrm.mod == 0 && modrm.rm == 5) {
        memory.base = Register::RIP;
        memory.rip_relative = true;
        memory.displacement = cursor.read_i32();
    } else {
        memory.base = register_from_code(rm_code);
    }

    if (modrm.mod == 1) {
        memory.displacement = cursor.read_i8();
    } else if (modrm.mod == 2) {
        memory.displacement = cursor.read_i32();
    }

    return Operand::make_memory(memory, width, access);
}

ConditionCode condition_from_low_nibble(uint8_t low) {
    switch (low & 0x0fU) {
        case 0x0: return ConditionCode::O;
        case 0x1: return ConditionCode::NO;
        case 0x2: return ConditionCode::B;
        case 0x3: return ConditionCode::AE;
        case 0x4: return ConditionCode::E;
        case 0x5: return ConditionCode::NE;
        case 0x6: return ConditionCode::BE;
        case 0x7: return ConditionCode::A;
        case 0x8: return ConditionCode::S;
        case 0x9: return ConditionCode::NS;
        case 0xa: return ConditionCode::P;
        case 0xb: return ConditionCode::NP;
        case 0xc: return ConditionCode::L;
        case 0xd: return ConditionCode::GE;
        case 0xe: return ConditionCode::LE;
        case 0xf: return ConditionCode::G;
        default: return ConditionCode::None;
    }
}

void finalize_bytes(Instruction& instruction, const std::vector<uint8_t>& bytes, std::size_t start, std::size_t end) {
    instruction.encoding.bytes.assign(bytes.begin() + static_cast<std::ptrdiff_t>(start), bytes.begin() + static_cast<std::ptrdiff_t>(end));
}

Instruction invalid_instruction(uint64_t address, uint8_t byte) {
    Instruction instruction;
    instruction.address = address;
    instruction.opcode = Opcode::Invalid;
    instruction.encoding.bytes.push_back(byte);
    return instruction;
}

} // namespace

std::vector<Instruction> Decoder::decode(const std::vector<uint8_t>& bytes, uint64_t base_address) const {
    Cursor cursor{bytes, 0};
    std::vector<Instruction> instructions;

    while (cursor.offset < bytes.size()) {
        const std::size_t start = cursor.offset;
        Instruction instruction;
        instruction.address = base_address + start;

        try {
            RexPrefix rex;
            if (cursor.has(1) && bytes[cursor.offset] >= 0x40 && bytes[cursor.offset] <= 0x4f) {
                rex.present = true;
                rex.byte = cursor.read_u8();
                rex.w = (rex.byte & 0x08U) != 0;
                rex.r = (rex.byte & 0x04U) != 0;
                rex.x = (rex.byte & 0x02U) != 0;
                rex.b = (rex.byte & 0x01U) != 0;
                instruction.encoding.has_rex = true;
                instruction.encoding.rex = rex.byte;
            }

            const uint8_t opcode = cursor.read_u8();
            const uint16_t width = operand_width(rex);

            if (opcode == 0x90) {
                instruction.opcode = Opcode::Nop;
            } else if (opcode == 0xc3) {
                instruction.opcode = Opcode::Ret;
                instruction.is_control_flow = true;
                instruction.is_return = true;
            } else if ((opcode & 0xf8U) == 0x50U) {
                instruction.opcode = Opcode::Push;
                const auto reg = register_from_code(static_cast<uint8_t>((opcode & 0x07U) | (rex.b ? 8U : 0U)));
                instruction.operands.push_back(Operand::make_register(reg, 64, OperandAccess::Read));
            } else if ((opcode & 0xf8U) == 0x58U) {
                instruction.opcode = Opcode::Pop;
                const auto reg = register_from_code(static_cast<uint8_t>((opcode & 0x07U) | (rex.b ? 8U : 0U)));
                instruction.operands.push_back(Operand::make_register(reg, 64, OperandAccess::Write));
            } else if ((opcode & 0xf8U) == 0xb8U) {
                instruction.opcode = Opcode::Mov;
                const auto reg = register_from_code(static_cast<uint8_t>((opcode & 0x07U) | (rex.b ? 8U : 0U)));
                const int32_t immediate = cursor.read_i32();
                instruction.operands.push_back(Operand::make_register(reg, width, OperandAccess::Write));
                instruction.operands.push_back(Operand::make_immediate(immediate, width));
            } else if (opcode == 0xe8) {
                const int32_t displacement = cursor.read_i32();
                const uint64_t target = base_address + cursor.offset + displacement;
                instruction.opcode = Opcode::Call;
                instruction.operands.push_back(Operand::make_relative(target, 64));
                instruction.is_control_flow = true;
                instruction.is_call = true;
            } else if (opcode == 0xe9) {
                const int32_t displacement = cursor.read_i32();
                const uint64_t target = base_address + cursor.offset + displacement;
                instruction.opcode = Opcode::Jmp;
                instruction.operands.push_back(Operand::make_relative(target, 64));
                instruction.is_control_flow = true;
            } else if (opcode == 0xeb) {
                const int8_t displacement = cursor.read_i8();
                const uint64_t target = base_address + cursor.offset + displacement;
                instruction.opcode = Opcode::Jmp;
                instruction.operands.push_back(Operand::make_relative(target, 64));
                instruction.is_control_flow = true;
            } else if ((opcode & 0xf0U) == 0x70U) {
                const int8_t displacement = cursor.read_i8();
                const uint64_t target = base_address + cursor.offset + displacement;
                instruction.opcode = Opcode::Jcc;
                instruction.condition = condition_from_low_nibble(opcode);
                instruction.operands.push_back(Operand::make_relative(target, 64));
                instruction.is_control_flow = true;
                instruction.is_conditional_branch = true;
            } else if (opcode == 0x0f) {
                const uint8_t opcode2 = cursor.read_u8();
                if ((opcode2 & 0xf0U) == 0x80U) {
                    const int32_t displacement = cursor.read_i32();
                    const uint64_t target = base_address + cursor.offset + displacement;
                    instruction.opcode = Opcode::Jcc;
                    instruction.condition = condition_from_low_nibble(opcode2);
                    instruction.operands.push_back(Operand::make_relative(target, 64));
                    instruction.is_control_flow = true;
                    instruction.is_conditional_branch = true;
                } else if ((opcode2 & 0xf0U) == 0x40U) {
                    const auto modrm = read_modrm(cursor);
                    instruction.encoding.has_modrm = true;
                    instruction.encoding.modrm = modrm.byte;
                    instruction.opcode = Opcode::Cmov;
                    instruction.condition = condition_from_low_nibble(opcode2);
                    const auto reg = register_from_code(static_cast<uint8_t>(modrm.reg | (rex.r ? 8U : 0U)));
                    instruction.operands.push_back(Operand::make_register(reg, width, OperandAccess::ReadWrite));
                    instruction.operands.push_back(decode_rm_operand(cursor, rex, modrm, width, OperandAccess::Read));
                } else if ((opcode2 & 0xf0U) == 0x90U) {
                    const auto modrm = read_modrm(cursor);
                    instruction.encoding.has_modrm = true;
                    instruction.encoding.modrm = modrm.byte;
                    instruction.opcode = Opcode::Setcc;
                    instruction.condition = condition_from_low_nibble(opcode2);
                    instruction.operands.push_back(decode_rm_operand(cursor, rex, modrm, 8, OperandAccess::Write));
                } else if (opcode2 == 0xaf) {
                    const auto modrm = read_modrm(cursor);
                    instruction.encoding.has_modrm = true;
                    instruction.encoding.modrm = modrm.byte;
                    instruction.opcode = Opcode::Imul;
                    const auto reg = register_from_code(static_cast<uint8_t>(modrm.reg | (rex.r ? 8U : 0U)));
                    instruction.operands.push_back(Operand::make_register(reg, width, OperandAccess::ReadWrite));
                    instruction.operands.push_back(decode_rm_operand(cursor, rex, modrm, width, OperandAccess::Read));
                } else {
                    instruction = invalid_instruction(base_address + start, opcode);
                    cursor.offset = start + 1;
                }
            } else if (opcode == 0x89 || opcode == 0x8b || opcode == 0x8d || opcode == 0x01 || opcode == 0x03 ||
                       opcode == 0x21 || opcode == 0x23 || opcode == 0x29 || opcode == 0x2b || opcode == 0x31 ||
                       opcode == 0x33 || opcode == 0x39 || opcode == 0x3b || opcode == 0x85) {
                const auto modrm = read_modrm(cursor);
                instruction.encoding.has_modrm = true;
                instruction.encoding.modrm = modrm.byte;
                const auto reg = register_from_code(static_cast<uint8_t>(modrm.reg | (rex.r ? 8U : 0U)));

                if (opcode == 0x89) {
                    instruction.opcode = Opcode::Mov;
                    instruction.operands.push_back(decode_rm_operand(cursor, rex, modrm, width, OperandAccess::Write));
                    instruction.operands.push_back(Operand::make_register(reg, width, OperandAccess::Read));
                } else if (opcode == 0x8b) {
                    instruction.opcode = Opcode::Mov;
                    instruction.operands.push_back(Operand::make_register(reg, width, OperandAccess::Write));
                    instruction.operands.push_back(decode_rm_operand(cursor, rex, modrm, width, OperandAccess::Read));
                } else if (opcode == 0x8d) {
                    instruction.opcode = Opcode::Lea;
                    instruction.operands.push_back(Operand::make_register(reg, width, OperandAccess::Write));
                    instruction.operands.push_back(decode_rm_operand(cursor, rex, modrm, width, OperandAccess::Read));
                } else if (opcode == 0x01) {
                    instruction.opcode = Opcode::Add;
                    instruction.operands.push_back(decode_rm_operand(cursor, rex, modrm, width, OperandAccess::ReadWrite));
                    instruction.operands.push_back(Operand::make_register(reg, width, OperandAccess::Read));
                } else if (opcode == 0x03) {
                    instruction.opcode = Opcode::Add;
                    instruction.operands.push_back(Operand::make_register(reg, width, OperandAccess::ReadWrite));
                    instruction.operands.push_back(decode_rm_operand(cursor, rex, modrm, width, OperandAccess::Read));
                } else if (opcode == 0x21) {
                    instruction.opcode = Opcode::And;
                    instruction.operands.push_back(decode_rm_operand(cursor, rex, modrm, width, OperandAccess::ReadWrite));
                    instruction.operands.push_back(Operand::make_register(reg, width, OperandAccess::Read));
                } else if (opcode == 0x23) {
                    instruction.opcode = Opcode::And;
                    instruction.operands.push_back(Operand::make_register(reg, width, OperandAccess::ReadWrite));
                    instruction.operands.push_back(decode_rm_operand(cursor, rex, modrm, width, OperandAccess::Read));
                } else if (opcode == 0x29) {
                    instruction.opcode = Opcode::Sub;
                    instruction.operands.push_back(decode_rm_operand(cursor, rex, modrm, width, OperandAccess::ReadWrite));
                    instruction.operands.push_back(Operand::make_register(reg, width, OperandAccess::Read));
                } else if (opcode == 0x2b) {
                    instruction.opcode = Opcode::Sub;
                    instruction.operands.push_back(Operand::make_register(reg, width, OperandAccess::ReadWrite));
                    instruction.operands.push_back(decode_rm_operand(cursor, rex, modrm, width, OperandAccess::Read));
                } else if (opcode == 0x31) {
                    instruction.opcode = Opcode::Xor;
                    instruction.operands.push_back(decode_rm_operand(cursor, rex, modrm, width, OperandAccess::ReadWrite));
                    instruction.operands.push_back(Operand::make_register(reg, width, OperandAccess::Read));
                } else if (opcode == 0x33) {
                    instruction.opcode = Opcode::Xor;
                    instruction.operands.push_back(Operand::make_register(reg, width, OperandAccess::ReadWrite));
                    instruction.operands.push_back(decode_rm_operand(cursor, rex, modrm, width, OperandAccess::Read));
                } else if (opcode == 0x39) {
                    instruction.opcode = Opcode::Cmp;
                    instruction.operands.push_back(decode_rm_operand(cursor, rex, modrm, width, OperandAccess::Read));
                    instruction.operands.push_back(Operand::make_register(reg, width, OperandAccess::Read));
                } else if (opcode == 0x3b) {
                    instruction.opcode = Opcode::Cmp;
                    instruction.operands.push_back(Operand::make_register(reg, width, OperandAccess::Read));
                    instruction.operands.push_back(decode_rm_operand(cursor, rex, modrm, width, OperandAccess::Read));
                } else if (opcode == 0x85) {
                    instruction.opcode = Opcode::Test;
                    instruction.operands.push_back(decode_rm_operand(cursor, rex, modrm, width, OperandAccess::Read));
                    instruction.operands.push_back(Operand::make_register(reg, width, OperandAccess::Read));
                }
            } else if (opcode == 0x83) {
                const auto modrm = read_modrm(cursor);
                instruction.encoding.has_modrm = true;
                instruction.encoding.modrm = modrm.byte;
                const uint8_t extension = modrm.reg;

                if (extension == 0) {
                    instruction.opcode = Opcode::Add;
                    instruction.operands.push_back(decode_rm_operand(cursor, rex, modrm, width, OperandAccess::ReadWrite));
                    instruction.operands.push_back(Operand::make_immediate(cursor.read_i8(), 8));
                } else if (extension == 4) {
                    instruction.opcode = Opcode::And;
                    instruction.operands.push_back(decode_rm_operand(cursor, rex, modrm, width, OperandAccess::ReadWrite));
                    instruction.operands.push_back(Operand::make_immediate(cursor.read_i8(), 8));
                } else if (extension == 5) {
                    instruction.opcode = Opcode::Sub;
                    instruction.operands.push_back(decode_rm_operand(cursor, rex, modrm, width, OperandAccess::ReadWrite));
                    instruction.operands.push_back(Operand::make_immediate(cursor.read_i8(), 8));
                } else if (extension == 7) {
                    instruction.opcode = Opcode::Cmp;
                    instruction.operands.push_back(decode_rm_operand(cursor, rex, modrm, width, OperandAccess::Read));
                    instruction.operands.push_back(Operand::make_immediate(cursor.read_i8(), 8));
                } else {
                    instruction = invalid_instruction(base_address + start, opcode);
                    cursor.offset = start + 1;
                }
            } else if (opcode == 0xc7) {
                const auto modrm = read_modrm(cursor);
                instruction.encoding.has_modrm = true;
                instruction.encoding.modrm = modrm.byte;
                if (modrm.reg == 0) {
                    instruction.opcode = Opcode::Mov;
                    instruction.operands.push_back(decode_rm_operand(cursor, rex, modrm, width, OperandAccess::Write));
                    instruction.operands.push_back(Operand::make_immediate(cursor.read_i32(), width));
                } else {
                    instruction = invalid_instruction(base_address + start, opcode);
                    cursor.offset = start + 1;
                }
            } else if (opcode == 0xff) {
                const auto modrm = read_modrm(cursor);
                instruction.encoding.has_modrm = true;
                instruction.encoding.modrm = modrm.byte;
                if (modrm.reg == 2) {
                    instruction.opcode = Opcode::Call;
                    instruction.operands.push_back(decode_rm_operand(cursor, rex, modrm, 64, OperandAccess::Read));
                    instruction.is_control_flow = true;
                    instruction.is_call = true;
                } else if (modrm.reg == 4) {
                    instruction.opcode = Opcode::Jmp;
                    instruction.operands.push_back(decode_rm_operand(cursor, rex, modrm, 64, OperandAccess::Read));
                    instruction.is_control_flow = true;
                } else if (modrm.reg == 6) {
                    instruction.opcode = Opcode::Push;
                    instruction.operands.push_back(decode_rm_operand(cursor, rex, modrm, 64, OperandAccess::Read));
                } else {
                    instruction = invalid_instruction(base_address + start, opcode);
                    cursor.offset = start + 1;
                }
            } else {
                instruction = invalid_instruction(base_address + start, opcode);
            }
        } catch (const std::out_of_range&) {
            instruction = invalid_instruction(base_address + start, bytes[start]);
            cursor.offset = start + 1;
        }

        finalize_bytes(instruction, bytes, start, cursor.offset);
        instructions.push_back(instruction);
    }

    return instructions;
}

} // namespace mykisah::x86
