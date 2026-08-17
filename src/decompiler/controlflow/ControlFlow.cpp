#include "decompiler/controlflow/ControlFlow.h"

#include <set>
#include <sstream>

namespace mykisah::core {
namespace {

const CFGEdge* find_edge(const BasicBlock& block, CFGEdgeKind kind) {
    for (const auto& edge : block.successors) {
        if (edge.kind == kind) {
            return &edge;
        }
    }
    return nullptr;
}

bool has_successor(const BasicBlock& block, BlockId id) {
    for (const auto& edge : block.successors) {
        if (edge.to == id) {
            return true;
        }
    }
    return false;
}

std::string condition_name(x86::ConditionCode condition) {
    return x86::opcode_name(x86::Opcode::Jcc, condition);
}

} // namespace

BlockId ControlFlowRecovery::single_successor(const BasicBlock& block) const {
    if (block.successors.size() != 1) {
        return INVALID_BLOCK_ID;
    }
    return block.successors.front().to;
}

x86::ConditionCode ControlFlowRecovery::block_condition(const BasicBlock& block) const {
    if (block.instructions.empty()) {
        return x86::ConditionCode::None;
    }

    const auto& instruction = block.instructions.back();
    if (instruction.opcode == x86::Opcode::Jcc) {
        return instruction.condition;
    }
    return x86::ConditionCode::None;
}

bool ControlFlowRecovery::is_simple_if_else(
    const CFG& cfg,
    const BasicBlock& block,
    StructuredNode& output) const {
    const auto* true_edge = find_edge(block, CFGEdgeKind::ConditionalTrue);
    const auto* false_edge = find_edge(block, CFGEdgeKind::ConditionalFalse);
    if (true_edge == nullptr || false_edge == nullptr) {
        return false;
    }

    if (true_edge->to >= cfg.blocks.size() || false_edge->to >= cfg.blocks.size()) {
        return false;
    }

    const auto& true_block = cfg.blocks[true_edge->to];
    const auto& false_block = cfg.blocks[false_edge->to];

    BlockId follow = INVALID_BLOCK_ID;
    const auto true_successor = single_successor(true_block);
    const auto false_successor = single_successor(false_block);

    if (true_successor != INVALID_BLOCK_ID && true_successor == false_edge->to) {
        // if without else: true branch falls into the false/follow block.
        follow = false_edge->to;
    } else if (false_successor != INVALID_BLOCK_ID && false_successor == true_edge->to) {
        // Inverted if without else.
        follow = true_edge->to;
    } else if (true_successor != INVALID_BLOCK_ID && true_successor == false_successor) {
        follow = true_successor;
    } else if (has_successor(true_block, false_edge->to)) {
        follow = false_edge->to;
    } else if (has_successor(false_block, true_edge->to)) {
        follow = true_edge->to;
    }

    output.kind = StructuredNodeKind::IfElse;
    output.block = block.id;
    output.condition = block_condition(block);
    output.true_block = true_edge->to;
    output.false_block = false_edge->to;
    output.follow_block = follow;
    return true;
}

StructuredFunction ControlFlowRecovery::recover(const CFG& cfg) const {
    StructuredFunction function;
    function.name = cfg.function_name;
    function.address = cfg.function_address;

    std::set<BlockId> emitted;

    for (const auto& block : cfg.blocks) {
        if (emitted.count(block.id) > 0) {
            continue;
        }

        StructuredNode structured;
        if (is_simple_if_else(cfg, block, structured)) {
            function.nodes.push_back(structured);
            emitted.insert(block.id);
            if (structured.true_block != INVALID_BLOCK_ID) {
                emitted.insert(structured.true_block);
            }
            if (structured.false_block != INVALID_BLOCK_ID) {
                emitted.insert(structured.false_block);
            }
            continue;
        }

        structured.kind = StructuredNodeKind::Block;
        structured.block = block.id;
        function.nodes.push_back(structured);
        emitted.insert(block.id);
    }

    return function;
}

std::string format_structured_function(const StructuredFunction& function) {
    std::ostringstream output;
    output << "structured " << function.name << "()\n";

    for (const auto& node : function.nodes) {
        switch (node.kind) {
            case StructuredNodeKind::Block:
                output << "  emit block_" << node.block << "\n";
                break;
            case StructuredNodeKind::IfElse:
                output << "  if (" << condition_name(node.condition) << ") {\n"
                       << "    emit block_" << node.true_block << "\n"
                       << "  } else {\n"
                       << "    emit block_" << node.false_block << "\n"
                       << "  }";
                if (node.follow_block != INVALID_BLOCK_ID) {
                    output << " follow block_" << node.follow_block;
                }
                output << "\n";
                break;
            case StructuredNodeKind::Goto:
                output << "  goto 0x" << std::hex << node.goto_target << std::dec << "\n";
                break;
        }
    }

    return output.str();
}

} // namespace mykisah::core
