#include "decompiler/controlflow/CFG.h"

#include <algorithm>
#include <set>
#include <sstream>

namespace mykisah::core {
namespace {

bool has_relative_target(const x86::Instruction& instruction) {
    return !instruction.operands.empty() && instruction.operands.front().kind == x86::OperandKind::RelativeAddress;
}

uint64_t relative_target(const x86::Instruction& instruction) {
    return instruction.operands.front().relative_target;
}

bool is_terminator(const x86::Instruction& instruction) {
    return instruction.opcode == x86::Opcode::Jmp || instruction.opcode == x86::Opcode::Jcc ||
           instruction.opcode == x86::Opcode::Ret;
}

uint64_t next_instruction_address(const x86::Instruction& instruction) {
    return instruction.address + instruction.size();
}

void add_edge(CFG& cfg, BlockId from, BlockId to, CFGEdgeKind kind) {
    if (from == INVALID_BLOCK_ID || to == INVALID_BLOCK_ID || from >= cfg.blocks.size() || to >= cfg.blocks.size()) {
        return;
    }

    CFGEdge edge{from, to, kind};
    cfg.blocks[from].successors.push_back(edge);
    cfg.blocks[to].predecessors.push_back(edge);
}

BlockId find_block_containing_or_starting_at(const CFG& cfg, uint64_t address) {
    const auto by_start = cfg.block_by_start_address.find(address);
    if (by_start != cfg.block_by_start_address.end()) {
        return by_start->second;
    }

    for (const auto& block : cfg.blocks) {
        if (address >= block.start_address && address < block.end_address) {
            return block.id;
        }
    }

    return INVALID_BLOCK_ID;
}

} // namespace

std::vector<uint64_t> CFGBuilder::find_block_leaders(
    const elf::Function& function,
    const std::vector<x86::Instruction>& instructions) const {
    std::set<uint64_t> leaders;
    if (instructions.empty()) {
        return {};
    }

    leaders.insert(function.address);

    for (std::size_t i = 0; i < instructions.size(); ++i) {
        const auto& instruction = instructions[i];

        if ((instruction.opcode == x86::Opcode::Jmp || instruction.opcode == x86::Opcode::Jcc) && has_relative_target(instruction)) {
            leaders.insert(relative_target(instruction));
        }

        if (is_terminator(instruction) && i + 1 < instructions.size()) {
            leaders.insert(instructions[i + 1].address);
        }
    }

    std::vector<uint64_t> result;
    result.reserve(leaders.size());
    for (const auto leader : leaders) {
        if (leader >= function.address && leader < function.address + function.size) {
            result.push_back(leader);
        }
    }

    return result;
}

CFG CFGBuilder::build(const elf::Function& function, const std::vector<x86::Instruction>& instructions) const {
    CFG cfg;
    cfg.function_name = function.name;
    cfg.function_address = function.address;

    if (instructions.empty()) {
        return cfg;
    }

    const auto leaders = find_block_leaders(function, instructions);
    std::set<uint64_t> leader_set(leaders.begin(), leaders.end());

    BasicBlock current;
    current.id = 0;
    current.start_address = instructions.front().address;

    for (std::size_t i = 0; i < instructions.size(); ++i) {
        const auto& instruction = instructions[i];
        const bool starts_new_block = instruction.address != current.start_address && leader_set.count(instruction.address) > 0;

        if (starts_new_block && !current.instructions.empty()) {
            current.end_address = current.instructions.back().address + current.instructions.back().size();
            cfg.block_by_start_address[current.start_address] = current.id;
            cfg.blocks.push_back(current);

            current = BasicBlock{};
            current.id = cfg.blocks.size();
            current.start_address = instruction.address;
        }

        current.instructions.push_back(instruction);

        if (is_terminator(instruction) && i + 1 < instructions.size()) {
            current.end_address = instruction.address + instruction.size();
            cfg.block_by_start_address[current.start_address] = current.id;
            cfg.blocks.push_back(current);

            current = BasicBlock{};
            current.id = cfg.blocks.size();
            current.start_address = instructions[i + 1].address;
        }
    }

    if (!current.instructions.empty()) {
        current.end_address = current.instructions.back().address + current.instructions.back().size();
        cfg.block_by_start_address[current.start_address] = current.id;
        cfg.blocks.push_back(current);
    }

    cfg.entry_block = find_block_containing_or_starting_at(cfg, function.address);

    for (std::size_t i = 0; i < cfg.blocks.size(); ++i) {
        auto& block = cfg.blocks[i];
        if (block.instructions.empty()) {
            continue;
        }

        const auto& terminator = block.instructions.back();

        if (terminator.opcode == x86::Opcode::Ret) {
            continue;
        }

        if (terminator.opcode == x86::Opcode::Jmp && has_relative_target(terminator)) {
            add_edge(cfg, block.id, find_block_containing_or_starting_at(cfg, relative_target(terminator)), CFGEdgeKind::Jump);
            continue;
        }

        if (terminator.opcode == x86::Opcode::Jcc && has_relative_target(terminator)) {
            add_edge(cfg, block.id, find_block_containing_or_starting_at(cfg, relative_target(terminator)), CFGEdgeKind::ConditionalTrue);
            add_edge(cfg, block.id, find_block_containing_or_starting_at(cfg, next_instruction_address(terminator)), CFGEdgeKind::ConditionalFalse);
            continue;
        }

        const auto fallthrough = find_block_containing_or_starting_at(cfg, block.end_address);
        if (fallthrough != INVALID_BLOCK_ID) {
            add_edge(cfg, block.id, fallthrough, CFGEdgeKind::Fallthrough);
        }
    }

    return cfg;
}

std::string cfg_edge_kind_name(CFGEdgeKind kind) {
    switch (kind) {
        case CFGEdgeKind::Fallthrough: return "fallthrough";
        case CFGEdgeKind::ConditionalTrue: return "true";
        case CFGEdgeKind::ConditionalFalse: return "false";
        case CFGEdgeKind::Jump: return "jump";
        case CFGEdgeKind::Return: return "return";
    }

    return "unknown";
}

std::string format_cfg(const CFG& cfg) {
    std::ostringstream output;
    output << "entry: ";
    if (cfg.entry_block == INVALID_BLOCK_ID) {
        output << "none\n";
    } else {
        output << "block_" << cfg.entry_block << "\n";
    }

    for (const auto& block : cfg.blocks) {
        output << "block_" << block.id << " [0x" << std::hex << block.start_address << ", 0x" << block.end_address << ")";
        if (block.successors.empty()) {
            output << " -> <exit>";
        }
        output << "\n";

        for (const auto& edge : block.successors) {
            output << "  -> block_" << std::dec << edge.to << " (" << cfg_edge_kind_name(edge.kind) << ")\n";
        }
    }

    return output.str();
}

} // namespace mykisah::core
