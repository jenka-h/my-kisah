#pragma once

#include "decompiler/controlflow/ControlFlow.h"
#include "decompiler/analysis/Expression.h"
#include "decompiler/analysis/SSAAnalyzer.h"
#include "decompiler/analysis/Signature.h"
#include "decompiler/analysis/TypeInference.h"

#include <memory>
#include <string>
#include <vector>

namespace mykisah::core {

enum class ASTStatementKind {
    Return,
    If,
    While,
    Goto,
    Label,
    Comment,
};

struct ASTStatement;
using ASTStatementPtr = std::shared_ptr<ASTStatement>;

struct ASTBlock {
    std::vector<ASTStatementPtr> statements;
};

struct ASTStatement {
    ASTStatementKind kind = ASTStatementKind::Comment;
    ExprPtr expression;
    std::string text;
    std::string label;
    ASTBlock then_block;
    ASTBlock else_block;
    ASTBlock body;
};

struct ASTParameter {
    std::string name;
    TypeInfo type;
};

struct ASTFunction {
    std::string name;
    TypeInfo return_type;
    bool return_type_known = false;
    std::vector<ASTParameter> parameters;
    ASTBlock body;
};

class ASTBuilder {
public:
    [[nodiscard]] ASTFunction build(
        const FunctionSignature& signature,
        const FunctionTypeInfo& types,
        const SSAFunction& ssa,
        const StructuredFunction& structured) const;

private:
    [[nodiscard]] ASTStatementPtr make_comment(std::string text) const;
    [[nodiscard]] ASTStatementPtr make_return(ExprPtr expression) const;
};

} // namespace mykisah::core
