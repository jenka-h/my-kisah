#pragma once

#include "frontend/elf/ElfFile.h"
#include "frontend/x86/Instruction.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace mykisah::core {

using BlockId = std::size_t;

constexpr BlockId INVALID_BLOCK_ID = static_cast<BlockId>(-1);

enum class CFGEdgeKind {
    Fallthrough,
    ConditionalTrue,
    ConditionalFalse,
    Jump,
    Return,
};

struct CFGEdge {
    BlockId from = INVALID_BLOCK_ID;
    BlockId to = INVALID_BLOCK_ID;
    CFGEdgeKind kind = CFGEdgeKind::Fallthrough;
};

struct BasicBlock {
    BlockId id = INVALID_BLOCK_ID;
    uint64_t start_address = 0;
    uint64_t end_address = 0;
    std::vector<x86::Instruction> instructions;
    std::vector<CFGEdge> successors;
    std::vector<CFGEdge> predecessors;
};

struct CFG {
    std::string function_name;
    uint64_t function_address = 0;
    BlockId entry_block = INVALID_BLOCK_ID;
    std::vector<BasicBlock> blocks;
    std::map<uint64_t, BlockId> block_by_start_address;
};

class CFGBuilder {
public:
    [[nodiscard]] CFG build(const elf::Function& function, const std::vector<x86::Instruction>& instructions) const;

private:
    [[nodiscard]] std::vector<uint64_t> find_block_leaders(
        const elf::Function& function,
        const std::vector<x86::Instruction>& instructions) const;
};

[[nodiscard]] std::string cfg_edge_kind_name(CFGEdgeKind kind);
[[nodiscard]] std::string format_cfg(const CFG& cfg);

} // namespace mykisah::core
