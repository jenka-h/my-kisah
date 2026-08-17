#include "presentation/cpp/CppEmitter.h"

namespace mykisah::core {

std::string CppEmitter::indent(unsigned level) const {
    return std::string(level * 4U, ' ');
}

std::string CppEmitter::emit_type(const TypeInfo& type, bool known) const {
    if (!known) {
        return "auto /* unknown */";
    }

    switch (type.category) {
        case TypeCategory::Pointer:
            return "void*";
        case TypeCategory::BooleanLike:
            return "bool";
        case TypeCategory::Float:
            return type.width_bits == 32 ? "float" : "double";
        case TypeCategory::Integer:
            if (type.width_bits <= 8) return "int8_t";
            if (type.width_bits <= 16) return "int16_t";
            if (type.width_bits <= 32) return "int32_t";
            return "int64_t";
        case TypeCategory::Unknown:
            return "auto /* unknown */";
    }

    return "auto /* unknown */";
}

void CppEmitter::emit_block(std::string& output, const ASTBlock& block, unsigned indentation) const {
    for (const auto& statement : block.statements) {
        if (!statement) {
            continue;
        }

        switch (statement->kind) {
            case ASTStatementKind::Return:
                output += indent(indentation) + "return " + format_expression(statement->expression, false) + ";\n";
                break;
            case ASTStatementKind::If:
                output += indent(indentation) + "if (condition_" + statement->text + ") {\n";
                emit_block(output, statement->then_block, indentation + 1);
                output += indent(indentation) + "}";
                if (!statement->else_block.statements.empty()) {
                    output += " else {\n";
                    emit_block(output, statement->else_block, indentation + 1);
                    output += indent(indentation) + "}";
                }
                output += "\n";
                break;
            case ASTStatementKind::While:
                output += indent(indentation) + "while (condition) {\n";
                emit_block(output, statement->body, indentation + 1);
                output += indent(indentation) + "}\n";
                break;
            case ASTStatementKind::Goto:
                output += indent(indentation) + "goto " + statement->label + ";\n";
                break;
            case ASTStatementKind::Label:
                output += statement->label + ":\n";
                break;
            case ASTStatementKind::Comment:
                output += indent(indentation) + "/* " + statement->text + " */\n";
                break;
        }
    }
}

std::string CppEmitter::emit(const ASTFunction& function) const {
    std::string output;
    output += emit_type(function.return_type, function.return_type_known) + " " + function.name + "(";

    for (std::size_t i = 0; i < function.parameters.size(); ++i) {
        if (i != 0) {
            output += ", ";
        }
        output += emit_type(function.parameters[i].type, true) + " " + function.parameters[i].name;
    }

    output += ") {\n";
    emit_block(output, function.body, 1);
    output += "}\n";
    return output;
}

} // namespace mykisah::core
