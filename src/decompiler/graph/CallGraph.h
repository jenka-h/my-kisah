#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace mykisah::core {

struct CallGraphNode {
    uint64_t function_address = 0;
    std::string function_name;
};

struct CallGraphEdge {
    uint64_t caller = 0;
    uint64_t callee = 0;
};

struct CallGraph {
    std::map<uint64_t, CallGraphNode> nodes;
    std::vector<CallGraphEdge> edges;
};

class CallGraphBuilder {
public:
    void add_function(uint64_t address, std::string name);
    void add_call(uint64_t caller, uint64_t callee);
    [[nodiscard]] CallGraph build() const;

private:
    CallGraph graph_;
};

} // namespace mykisah::core
