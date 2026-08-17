#pragma once

#include "decompiler/analysis/Expression.h"
#include "decompiler/ir/IR.h"

#include <map>
#include <string>
#include <vector>

namespace mykisah::core {

struct SSAStatement {
    uint64_t source_address = 0;
    SSAVariable destination;
    ExprPtr expression;
    IROpcode source_opcode = IROpcode::Unknown;
    bool has_destination = false;
    bool is_return = false;
    bool is_call = false;
};

struct SSAFunction {
    std::string name;
    uint64_t address = 0;
    std::vector<SSAStatement> statements;
};

class SSAAnalyzer {
public:
    [[nodiscard]] SSAFunction analyze(const IRFunction& function) const;

private:
    struct State {
        std::map<std::string, uint32_t> versions;
        std::map<std::string, ExprPtr> expressions;
        std::map<int64_t, ExprPtr> stack_slots;
        ExprPtr comparison_lhs;
        ExprPtr comparison_rhs;
    };

    [[nodiscard]] ExprPtr expression_for_value(const IRValue& value, State& state) const;
    [[nodiscard]] ExprPtr expression_for_memory_address(const x86::MemoryOperand& memory, uint16_t width_bits, State& state) const;
    [[nodiscard]] SSAVariable define_register(x86::Register reg, uint16_t width_bits, State& state) const;
    [[nodiscard]] SSAVariable define_named(std::string name, uint16_t width_bits, State& state) const;
    [[nodiscard]] ExprPtr current_register(x86::Register reg, uint16_t width_bits, State& state) const;
};

[[nodiscard]] std::string format_ssa_statement(const SSAStatement& statement);

} // namespace mykisah::core
