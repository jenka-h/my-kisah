#pragma once

#include "decompiler/controlflow/CFG.h"
#include "frontend/x86/Instruction.h"

#include <cstdint>
#include <string>
#include <vector>

namespace mykisah::core {

enum class StructuredNodeKind {
    Block,
    IfElse,
    Goto,
};

struct StructuredNode {
    StructuredNodeKind kind = StructuredNodeKind::Block;
    BlockId block = INVALID_BLOCK_ID;
    x86::ConditionCode condition = x86::ConditionCode::None;
    BlockId true_block = INVALID_BLOCK_ID;
    BlockId false_block = INVALID_BLOCK_ID;
    BlockId follow_block = INVALID_BLOCK_ID;
    uint64_t goto_target = 0;
};

struct StructuredFunction {
    std::string name;
    uint64_t address = 0;
    std::vector<StructuredNode> nodes;
};

class ControlFlowRecovery {
public:
    [[nodiscard]] StructuredFunction recover(const CFG& cfg) const;

private:
    [[nodiscard]] bool is_simple_if_else(
        const CFG& cfg,
        const BasicBlock& block,
        StructuredNode& output) const;

    [[nodiscard]] BlockId single_successor(const BasicBlock& block) const;
    [[nodiscard]] x86::ConditionCode block_condition(const BasicBlock& block) const;
};

[[nodiscard]] std::string format_structured_function(const StructuredFunction& function);

} // namespace mykisah::core
