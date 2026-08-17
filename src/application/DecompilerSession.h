#pragma once

#include "decompiler/ast/AST.h"
#include "decompiler/controlflow/CFG.h"
#include "decompiler/graph/CallGraph.h"
#include "decompiler/analysis/Signature.h"
#include "frontend/elf/ElfFile.h"
#include "frontend/x86/Instruction.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace mykisah::app {

struct AssemblyLine {
    uint64_t address = 0;
    uint64_t size = 0;
    std::string bytes;
    std::string text;
    bool has_call_target = false;
    uint64_t call_target = 0;
};

struct FunctionResult {
    elf::Function function;
    std::vector<x86::Instruction> instructions;
    core::CFG cfg;
    core::FunctionSignature signature;
    core::ASTFunction ast;
    std::string decompiled_cpp;
    std::vector<AssemblyLine> assembly;
    std::vector<uint64_t> direct_callees;
};

class DecompilerSession {
public:
    void load(const std::string& path);

    [[nodiscard]] const std::string& path() const;
    [[nodiscard]] const std::vector<FunctionResult>& functions() const;
    [[nodiscard]] const core::CallGraph& call_graph() const;
    [[nodiscard]] const FunctionResult* find_function(uint64_t address) const;
    [[nodiscard]] const FunctionResult* find_function_containing(uint64_t address) const;

private:
    std::string path_;
    std::vector<FunctionResult> functions_;
    std::map<uint64_t, std::size_t> function_by_address_;
    core::CallGraph call_graph_;
};

} // namespace mykisah::app
