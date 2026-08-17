#include "application/DecompilerSession.h"

#include "decompiler/controlflow/ControlFlow.h"
#include "presentation/cpp/CppEmitter.h"
#include "decompiler/ir/IRLifter.h"
#include "decompiler/analysis/SSAAnalyzer.h"
#include "decompiler/analysis/TypeInference.h"
#include "frontend/elf/ElfValidator.h"
#include "frontend/x86/Decoder.h"
#include "frontend/x86/Formatter.h"

#include <algorithm>
#include <stdexcept>

namespace mykisah::app {

void DecompilerSession::load(const std::string& path) {
    const auto validation = elf::ElfValidator::validate_file(path);
    if (!validation.is_supported_baseline()) {
        throw std::runtime_error(validation.error.empty() ? "unsupported ELF binary" : validation.error);
    }

    const auto functions = elf::ElfFileParser::parse_functions(path);

    x86::Decoder decoder;
    x86::Formatter formatter;
    core::ASTBuilder ast_builder;
    core::CFGBuilder cfg_builder;
    core::ControlFlowRecovery control_flow;
    core::CppEmitter emitter;
    core::IRLifter lifter;
    core::SSAAnalyzer ssa_analyzer;
    core::SignatureAnalyzer signature_analyzer;
    core::TypeInference type_inference;

    std::vector<FunctionResult> new_results;
    new_results.reserve(functions.size());

    for (const auto& function : functions) {
        FunctionResult result;
        result.function = function;
        result.instructions = decoder.decode(function.bytes, function.address);
        result.cfg = cfg_builder.build(function, result.instructions);

        const auto ir = lifter.lift_function(function, result.instructions);
        const auto ssa = ssa_analyzer.analyze(ir);
        result.signature = signature_analyzer.analyze(function, result.cfg);
        const auto types = type_inference.analyze(result.cfg, result.signature);
        const auto structured = control_flow.recover(result.cfg);
        result.ast = ast_builder.build(result.signature, types, ssa, structured);
        result.decompiled_cpp = emitter.emit(result.ast);

        for (const auto& instruction : result.instructions) {
            AssemblyLine line;
            line.address = instruction.address;
            line.size = instruction.size();
            line.bytes = formatter.format_bytes(instruction);
            line.text = formatter.format_instruction(instruction);

            const bool direct_call = instruction.opcode == x86::Opcode::Call;
            const bool tail_jump = instruction.opcode == x86::Opcode::Jmp &&
                instruction.address + instruction.size() == function.address + function.size;
            if ((direct_call || tail_jump) && !instruction.operands.empty() &&
                instruction.operands.front().kind == x86::OperandKind::RelativeAddress) {
                line.has_call_target = true;
                line.call_target = instruction.operands.front().relative_target;
                result.direct_callees.push_back(line.call_target);
            }

            result.assembly.push_back(std::move(line));
        }

        std::sort(result.direct_callees.begin(), result.direct_callees.end());
        result.direct_callees.erase(
            std::unique(result.direct_callees.begin(), result.direct_callees.end()),
            result.direct_callees.end());
        new_results.push_back(std::move(result));
    }

    std::map<uint64_t, std::size_t> new_index;
    core::CallGraphBuilder call_graph_builder;
    for (std::size_t i = 0; i < new_results.size(); ++i) {
        new_index[new_results[i].function.address] = i;
        call_graph_builder.add_function(new_results[i].function.address, new_results[i].function.name);
    }

    for (const auto& result : new_results) {
        for (const auto callee : result.direct_callees) {
            if (new_index.count(callee) > 0) {
                call_graph_builder.add_call(result.function.address, callee);
            }
        }
    }

    path_ = path;
    functions_ = std::move(new_results);
    function_by_address_ = std::move(new_index);
    call_graph_ = call_graph_builder.build();
}

const std::string& DecompilerSession::path() const {
    return path_;
}

const std::vector<FunctionResult>& DecompilerSession::functions() const {
    return functions_;
}

const core::CallGraph& DecompilerSession::call_graph() const {
    return call_graph_;
}

const FunctionResult* DecompilerSession::find_function(uint64_t address) const {
    const auto iterator = function_by_address_.find(address);
    if (iterator == function_by_address_.end()) {
        return nullptr;
    }
    return &functions_.at(iterator->second);
}

const FunctionResult* DecompilerSession::find_function_containing(uint64_t address) const {
    const auto exact = find_function(address);
    if (exact != nullptr) {
        return exact;
    }

    for (const auto& function : functions_) {
        if (address >= function.function.address && address < function.function.address + function.function.size) {
            return &function;
        }
    }
    return nullptr;
}

} // namespace mykisah::app
