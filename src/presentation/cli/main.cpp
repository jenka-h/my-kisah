#include "decompiler/ast/AST.h"
#include "decompiler/controlflow/CFG.h"
#include "decompiler/controlflow/ControlFlow.h"
#include "presentation/cpp/CppEmitter.h"
#include "decompiler/ir/IR.h"
#include "decompiler/ir/IRLifter.h"
#include "decompiler/analysis/SSAAnalyzer.h"
#include "decompiler/analysis/Signature.h"
#include "decompiler/analysis/TypeInference.h"
#include "frontend/elf/ElfFile.h"
#include "frontend/elf/ElfValidator.h"
#include "frontend/x86/Decoder.h"
#include "frontend/x86/Formatter.h"

#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

const char* yes_no(bool value) {
    return value ? "yes" : "no";
}

void print_usage(const char* program_name) {
    std::cerr << "Usage: " << program_name << " <elf-binary>\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        print_usage(argv[0]);
        return 2;
    }

    const std::string path = argv[1];
    const auto result = mykisah::elf::ElfValidator::validate_file(path);

    std::cout << "ELF magic: " << yes_no(result.has_elf_magic) << '\n';
    std::cout << "ELF64: " << yes_no(result.is_elf64) << '\n';
    std::cout << "Endian: " << (result.is_little_endian ? "little" : "unsupported/unknown") << '\n';
    std::cout << "Architecture: " << (result.is_x86_64 ? "x86-64" : "unsupported/unknown") << '\n';

    if (!result.is_supported_baseline()) {
        if (!result.error.empty()) {
            std::cerr << "Error: " << result.error << '\n';
        }
        return 1;
    }

    try {
        const auto elf_file = mykisah::elf::ElfFileParser::parse_sections(path);

        std::cout << "\nSections:\n";
        std::cout << std::left << std::setw(12) << "Name"
                  << std::right << std::setw(14) << "File Offset"
                  << std::setw(18) << "Virtual Address"
                  << std::setw(14) << "Size" << '\n';

        for (const auto& section : elf_file.sections) {
            std::cout << std::left << std::setw(12) << section.name
                      << std::right << "0x" << std::hex << std::setw(12) << std::setfill('0') << section.file_offset
                      << "  0x" << std::setw(16) << section.virtual_address
                      << "  0x" << std::setw(12) << section.size
                      << std::dec << std::setfill(' ') << '\n';
        }

        const auto functions = mykisah::elf::ElfFileParser::parse_functions(path);
        std::cout << "\nFunctions:\n";
        std::cout << std::left << std::setw(32) << "Name"
                  << std::right << std::setw(18) << "Address"
                  << std::setw(14) << "Size"
                  << std::setw(18) << "Bytes Extracted" << '\n';

        mykisah::x86::Decoder decoder;
        mykisah::x86::Formatter formatter;
        mykisah::core::ASTBuilder ast_builder;
        mykisah::core::CFGBuilder cfg_builder;
        mykisah::core::ControlFlowRecovery control_flow_recovery;
        mykisah::core::CppEmitter cpp_emitter;
        mykisah::core::IRLifter lifter;
        mykisah::core::SSAAnalyzer ssa_analyzer;
        mykisah::core::SignatureAnalyzer signature_analyzer;
        mykisah::core::TypeInference type_inference;

        for (const auto& function : functions) {
            std::cout << std::left << std::setw(32) << function.name
                      << std::right << "0x" << std::hex << std::setw(16) << std::setfill('0') << function.address
                      << "  0x" << std::setw(12) << function.size
                      << std::dec << std::setfill(' ') << std::setw(18) << function.bytes.size() << '\n';
        }

        std::cout << "\nAssembly / Opcode:\n";
        for (const auto& function : functions) {
            if (function.bytes.empty()) {
                continue;
            }

            std::cout << "\n<" << function.name << ">:\n";
            std::cout << std::left << std::setw(18) << "Address"
                      << std::setw(28) << "Bytes"
                      << "Assembly" << '\n';

            const auto instructions = decoder.decode(function.bytes, function.address);
            for (const auto& instruction : instructions) {
                std::cout << "0x" << std::right << std::hex << std::setw(16) << std::setfill('0') << instruction.address
                          << std::setfill(' ') << "  "
                          << std::left << std::setw(26) << formatter.format_bytes(instruction)
                          << formatter.format_instruction(instruction)
                          << std::right << std::dec << '\n';
            }
        }

        std::cout << "\nLow-Level IR:\n";
        for (const auto& function : functions) {
            if (function.bytes.empty()) {
                continue;
            }

            const auto instructions = decoder.decode(function.bytes, function.address);
            const auto ir_function = lifter.lift_function(function, instructions);

            std::cout << "\n<" << ir_function.name << ">:\n";
            for (const auto& ir_instruction : ir_function.instructions) {
                std::cout << "0x" << std::right << std::hex << std::setw(16) << std::setfill('0') << ir_instruction.source_address
                          << std::setfill(' ') << "  "
                          << mykisah::core::format_ir_instruction(ir_instruction)
                          << std::dec << '\n';
            }
        }

        std::cout << "\nSSA / Symbolic Expressions:\n";
        for (const auto& function : functions) {
            if (function.bytes.empty()) {
                continue;
            }

            const auto instructions = decoder.decode(function.bytes, function.address);
            const auto ir_function = lifter.lift_function(function, instructions);
            const auto ssa_function = ssa_analyzer.analyze(ir_function);

            std::cout << "\n<" << ssa_function.name << ">:\n";
            for (const auto& statement : ssa_function.statements) {
                std::cout << "0x" << std::right << std::hex << std::setw(16) << std::setfill('0') << statement.source_address
                          << std::setfill(' ') << "  "
                          << mykisah::core::format_ssa_statement(statement)
                          << std::dec << '\n';
            }
        }

        std::cout << "\nBasic Blocks / CFG:\n";
        for (const auto& function : functions) {
            if (function.bytes.empty()) {
                continue;
            }

            const auto instructions = decoder.decode(function.bytes, function.address);
            const auto cfg = cfg_builder.build(function, instructions);

            std::cout << "\n<" << cfg.function_name << ">:\n";
            std::cout << mykisah::core::format_cfg(cfg);
        }

        std::cout << "\nRecovered Function Signatures:\n";
        for (const auto& function : functions) {
            if (function.bytes.empty()) {
                continue;
            }

            const auto instructions = decoder.decode(function.bytes, function.address);
            const auto cfg = cfg_builder.build(function, instructions);
            const auto signature = signature_analyzer.analyze(function, cfg);
            std::cout << mykisah::core::format_signature(signature) << '\n';
        }

        std::cout << "\nBasic Type Evidence:\n";
        for (const auto& function : functions) {
            if (function.bytes.empty()) {
                continue;
            }

            const auto instructions = decoder.decode(function.bytes, function.address);
            const auto cfg = cfg_builder.build(function, instructions);
            const auto signature = signature_analyzer.analyze(function, cfg);
            const auto types = type_inference.analyze(cfg, signature);

            std::cout << '<' << function.name << ">:\n";
            for (const auto& parameter : signature.parameters) {
                const auto iterator = types.register_types.find(parameter.source_register);
                std::cout << "  " << parameter.name << ": ";
                if (iterator == types.register_types.end()) {
                    std::cout << "Unknown";
                } else {
                    std::cout << mykisah::core::format_type_info(iterator->second);
                }
                std::cout << '\n';
            }
            if (signature.return_value_known && signature.returns_value) {
                std::cout << "  return: " << mykisah::core::format_type_info(types.return_type) << '\n';
            }
        }

        std::cout << "\nStructured Control Flow:\n";
        for (const auto& function : functions) {
            if (function.bytes.empty()) {
                continue;
            }

            const auto instructions = decoder.decode(function.bytes, function.address);
            const auto cfg = cfg_builder.build(function, instructions);
            const auto structured = control_flow_recovery.recover(cfg);

            std::cout << "\n<" << structured.name << ">:\n";
            std::cout << mykisah::core::format_structured_function(structured);
        }

        std::cout << "\nDecompiled C++-Like Output:\n";
        for (const auto& function : functions) {
            if (function.bytes.empty()) {
                continue;
            }

            const auto instructions = decoder.decode(function.bytes, function.address);
            const auto cfg = cfg_builder.build(function, instructions);
            const auto ir_function = lifter.lift_function(function, instructions);
            const auto ssa_function = ssa_analyzer.analyze(ir_function);
            const auto signature = signature_analyzer.analyze(function, cfg);
            const auto types = type_inference.analyze(cfg, signature);
            const auto structured = control_flow_recovery.recover(cfg);
            const auto ast = ast_builder.build(signature, types, ssa_function, structured);

            std::cout << '\n' << cpp_emitter.emit(ast);
        }
    } catch (const std::exception& exception) {
        std::cerr << "Error: failed to parse ELF sections: " << exception.what() << '\n';
        return 1;
    }

    return 0;
}
