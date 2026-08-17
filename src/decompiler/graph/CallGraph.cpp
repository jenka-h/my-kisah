#include "decompiler/graph/CallGraph.h"

#include <algorithm>
#include <utility>

namespace mykisah::core {

void CallGraphBuilder::add_function(uint64_t address, std::string name) {
    graph_.nodes[address] = CallGraphNode{address, std::move(name)};
}

void CallGraphBuilder::add_call(uint64_t caller, uint64_t callee) {
    const auto duplicate = std::find_if(graph_.edges.begin(), graph_.edges.end(), [caller, callee](const CallGraphEdge& edge) {
        return edge.caller == caller && edge.callee == callee;
    });

    if (duplicate == graph_.edges.end()) {
        graph_.edges.push_back(CallGraphEdge{caller, callee});
    }
}

CallGraph CallGraphBuilder::build() const {
    return graph_;
}

} // namespace mykisah::core
