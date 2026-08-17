#include "frontend/x86/Formatter.h"

#include <iomanip>
#include <sstream>

namespace mykisah::x86 {
namespace {

std::string hex_u64(uint64_t value) {
    std::ostringstream output;
    output << "0x" << std::hex << value;
    return output.str();
}

std::string signed_displacement(int64_t displacement) {
    std::ostringstream output;
    if (displacement < 0) {
        output << " - 0x" << std::hex << static_cast<uint64_t>(-displacement);
    } else if (displacement > 0) {
        output << " + 0x" << std::hex << static_cast<uint64_t>(displacement);
    }
    return output.str();
}

std::string memory_width_prefix(uint16_t width_bits) {
    switch (width_bits) {
        case 8: return "byte ptr ";
        case 16: return "word ptr ";
        case 32: return "dword ptr ";
        case 64: return "qword ptr ";
        default: return "ptr ";
    }
}

} // namespace

std::string Formatter::format_bytes(const Instruction& instruction) const {
    std::ostringstream output;
    for (std::size_t i = 0; i < instruction.encoding.bytes.size(); ++i) {
        if (i != 0) {
            output << ' ';
        }
        output << std::hex << std::setw(2) << std::setfill('0')
               << static_cast<unsigned>(instruction.encoding.bytes[i]);
    }
    return output.str();
}

std::string Formatter::format_operand(const Operand& operand) const {
    switch (operand.kind) {
        case OperandKind::None:
            return "";
        case OperandKind::Register:
            return register_name(operand.reg, operand.width_bits);
        case OperandKind::Immediate:
            if (operand.immediate < 0) {
                return "-" + hex_u64(static_cast<uint64_t>(-operand.immediate));
            }
            return hex_u64(static_cast<uint64_t>(operand.immediate));
        case OperandKind::RelativeAddress:
            return hex_u64(operand.relative_target);
        case OperandKind::Memory: {
            std::ostringstream output;
            output << memory_width_prefix(operand.width_bits) << '[';

            bool wrote_component = false;
            if (operand.memory.base != Register::None) {
                output << register_name(operand.memory.base, 64);
                wrote_component = true;
            }

            if (operand.memory.index != Register::None) {
                if (wrote_component) {
                    output << " + ";
                }
                output << register_name(operand.memory.index, 64);
                if (operand.memory.scale != 1) {
                    output << " * " << static_cast<unsigned>(operand.memory.scale);
                }
                wrote_component = true;
            }

            if (operand.memory.displacement != 0) {
                if (wrote_component) {
                    output << signed_displacement(operand.memory.displacement);
                } else if (operand.memory.displacement < 0) {
                    output << "-0x" << std::hex << static_cast<uint64_t>(-operand.memory.displacement);
                } else {
                    output << "0x" << std::hex << static_cast<uint64_t>(operand.memory.displacement);
                }
                wrote_component = true;
            }

            if (!wrote_component) {
                output << '0';
            }

            output << ']';
            return output.str();
        }
    }

    return "<?>";
}

std::string Formatter::format_instruction(const Instruction& instruction) const {
    if (instruction.opcode == Opcode::Invalid) {
        std::ostringstream output;
        output << "db ";
        if (!instruction.encoding.bytes.empty()) {
            output << hex_u64(instruction.encoding.bytes.front());
        } else {
            output << "<?>";
        }
        return output.str();
    }

    std::ostringstream output;
    output << opcode_name(instruction.opcode, instruction.condition);

    if (!instruction.operands.empty()) {
        output << ' ';
        for (std::size_t i = 0; i < instruction.operands.size(); ++i) {
            if (i != 0) {
                output << ", ";
            }
            output << format_operand(instruction.operands[i]);
        }
    }

    return output.str();
}

} // namespace mykisah::x86
