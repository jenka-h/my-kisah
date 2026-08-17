#include "decompiler/ast/AST.h"

#include <utility>

namespace mykisah::core {

ASTStatementPtr ASTBuilder::make_comment(std::string text) const {
    auto statement = std::make_shared<ASTStatement>();
    statement->kind = ASTStatementKind::Comment;
    statement->text = std::move(text);
    return statement;
}

ASTStatementPtr ASTBuilder::make_return(ExprPtr expression) const {
    auto statement = std::make_shared<ASTStatement>();
    statement->kind = ASTStatementKind::Return;
    statement->expression = std::move(expression);
    return statement;
}

ASTFunction ASTBuilder::build(
    const FunctionSignature& signature,
    const FunctionTypeInfo& types,
    const SSAFunction& ssa,
    const StructuredFunction& structured) const {
    ASTFunction function;
    function.name = signature.function_name;
    function.return_type_known = signature.return_value_known;
    function.return_type = types.return_type;

    for (const auto& parameter : signature.parameters) {
        ASTParameter ast_parameter;
        ast_parameter.name = parameter.name;
        const auto iterator = types.register_types.find(parameter.source_register);
        if (iterator != types.register_types.end()) {
            ast_parameter.type = iterator->second;
        }
        function.parameters.push_back(ast_parameter);
    }

    bool emitted_structured_if = false;
    for (const auto& node : structured.nodes) {
        if (node.kind != StructuredNodeKind::IfElse) {
            continue;
        }

        auto statement = std::make_shared<ASTStatement>();
        statement->kind = ASTStatementKind::If;
        statement->text = x86::opcode_name(x86::Opcode::Jcc, node.condition);
        statement->then_block.statements.push_back(make_comment("recovered block_" + std::to_string(node.true_block)));
        statement->else_block.statements.push_back(make_comment("recovered block_" + std::to_string(node.false_block)));
        function.body.statements.push_back(statement);
        emitted_structured_if = true;
    }

    ExprPtr recovered_return;
    for (const auto& statement : ssa.statements) {
        if (statement.is_return && statement.expression) {
            recovered_return = statement.expression;
        }
    }

    if (recovered_return && signature.return_value_known && signature.returns_value) {
        function.body.statements.push_back(make_return(recovered_return));
    } else if (signature.return_value_known && signature.returns_value) {
        function.body.statements.push_back(make_comment("return value could not be reconstructed"));
    }

    if (function.body.statements.empty()) {
        function.body.statements.push_back(make_comment("no high-level statements recovered"));
    } else if (emitted_structured_if) {
        function.body.statements.push_back(make_comment("control-flow block contents are not fully reconstructed yet"));
    }

    return function;
}

} // namespace mykisah::core
