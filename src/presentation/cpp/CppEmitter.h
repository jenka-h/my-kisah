#pragma once

#include "decompiler/ast/AST.h"

#include <string>

namespace mykisah::core {

class CppEmitter {
public:
    [[nodiscard]] std::string emit(const ASTFunction& function) const;

private:
    void emit_block(std::string& output, const ASTBlock& block, unsigned indentation) const;
    [[nodiscard]] std::string emit_type(const TypeInfo& type, bool known) const;
    [[nodiscard]] std::string indent(unsigned level) const;
};

} // namespace mykisah::core
