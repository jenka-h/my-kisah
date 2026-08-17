#include "decompiler/analysis/SSAAnalyzer.h"

#include <sstream>
#include <utility>

namespace mykisah::core {
namespace {

constexpr x86::Register ABI_ARGUMENT_REGISTERS[] = {
    x86::Register::RDI,
    x86::Register::RSI,
    x86::Register::RDX,
    x86::Register::RCX,
    x86::Register::R8,
    x86::Register::R9,
};

bool is_register_value(const IRValue& value) {
    return value.kind == IRValueKind::Register && value.reg != x86::Register::None;
}

bool is_stack_memory(const IRValue& value) {
    return value.kind == IRValueKind::Memory && value.memory.base == x86::Register::RBP && value.memory.index == x86::Register::None;
}

bool same_register_value(const IRValue& lhs, const IRValue& rhs) {
    return lhs.kind == IRValueKind::Register && rhs.kind == IRValueKind::Register && lhs.reg == rhs.reg;
}

} // namespace

SSAVariable SSAAnalyzer::define_named(std::string name, uint16_t width_bits, State& state) const {
    const auto next_version = ++state.versions[name];
    return SSAVariable{std::move(name), next_version, width_bits};
}

SSAVariable SSAAnalyzer::define_register(x86::Register reg, uint16_t width_bits, State& state) const {
    return define_named(canonical_register_name(reg), width_bits, state);
}

ExprPtr SSAAnalyzer::current_register(x86::Register reg, uint16_t width_bits, State& state) const {
    const auto name = canonical_register_name(reg);
    const auto iterator = state.expressions.find(name);
    if (iterator != state.expressions.end()) {
        return iterator->second;
    }

    if (is_abi_argument_register(reg)) {
        auto argument = Expression::make_variable(abi_argument_name(reg), 0, width_bits);
        state.expressions[name] = argument;
        return argument;
    }

    const auto version_iterator = state.versions.find(name);
    const uint32_t version = version_iterator == state.versions.end() ? 0 : version_iterator->second;
    auto variable = Expression::make_variable(name, version, width_bits);
    state.expressions[name] = variable;
    return variable;
}

ExprPtr SSAAnalyzer::expression_for_value(const IRValue& value, State& state) const {
    switch (value.kind) {
        case IRValueKind::Unknown:
            return Expression::make_unknown(value.width_bits);
        case IRValueKind::Register:
            return current_register(value.reg, value.width_bits, state);
        case IRValueKind::Constant:
            return Expression::make_constant(value.constant, value.width_bits);
        case IRValueKind::Address:
            return Expression::make_constant(static_cast<int64_t>(value.address), value.width_bits);
        case IRValueKind::Memory: {
            if (is_stack_memory(value)) {
                const auto iterator = state.stack_slots.find(value.memory.displacement);
                if (iterator != state.stack_slots.end()) {
                    return iterator->second;
                }
            }
            return Expression::make_memory(value.memory, value.width_bits);
        }
    }

    return Expression::make_unknown(value.width_bits);
}

ExprPtr SSAAnalyzer::expression_for_memory_address(const x86::MemoryOperand& memory, uint16_t width_bits, State& state) const {
    ExprPtr expression;

    if (memory.base != x86::Register::None && memory.base != x86::Register::RIP) {
        expression = current_register(memory.base, 64, state);
    }

    if (memory.index != x86::Register::None) {
        auto index_expression = current_register(memory.index, 64, state);
        if (memory.scale != 1) {
            index_expression = Expression::make_binary(
                IRBinaryOperator::Mul,
                index_expression,
                Expression::make_constant(memory.scale, 64),
                64);
        }

        expression = expression ? Expression::make_binary(IRBinaryOperator::Add, expression, index_expression, 64)
                                : index_expression;
    }

    if (memory.displacement != 0) {
        const auto displacement = Expression::make_constant(memory.displacement, 64);
        expression = expression ? Expression::make_binary(IRBinaryOperator::Add, expression, displacement, 64)
                                : displacement;
    }

    if (memory.base == x86::Register::RIP || !expression) {
        return Expression::make_address_of(memory, width_bits);
    }

    return expression;
}

SSAFunction SSAAnalyzer::analyze(const IRFunction& function) const {
    State state;
    SSAFunction ssa_function;
    ssa_function.name = function.name;
    ssa_function.address = function.address;

    for (const auto reg : ABI_ARGUMENT_REGISTERS) {
        const auto canonical = canonical_register_name(reg);
        state.versions[canonical] = 0;
        state.expressions[canonical] = Expression::make_variable(abi_argument_name(reg), 0, 64);
    }

    for (const auto& ir : function.instructions) {
        SSAStatement statement;
        statement.source_address = ir.source_address;
        statement.source_opcode = ir.opcode;

        switch (ir.opcode) {
            case IROpcode::Assign: {
                const auto source = ir.sources.empty() ? Expression::make_unknown(ir.destination.width_bits)
                                                       : expression_for_value(ir.sources.front(), state);

                if (is_register_value(ir.destination)) {
                    const auto variable = define_register(ir.destination.reg, ir.destination.width_bits, state);
                    state.expressions[variable.name] = source;
                    statement.destination = variable;
                    statement.expression = source;
                    statement.has_destination = true;
                } else if (is_stack_memory(ir.destination)) {
                    state.stack_slots[ir.destination.memory.displacement] = source;
                    statement.expression = source;
                } else {
                    statement.expression = source;
                }
                break;
            }
            case IROpcode::LoadEffectiveAddress: {
                const auto source = ir.sources.empty() ? Expression::make_unknown(ir.destination.width_bits)
                                                       : expression_for_value(ir.sources.front(), state);
                ExprPtr expression = source;
                if (!ir.sources.empty() && ir.sources.front().kind == IRValueKind::Memory) {
                    expression = expression_for_memory_address(ir.sources.front().memory, ir.destination.width_bits, state);
                }

                if (is_register_value(ir.destination)) {
                    const auto variable = define_register(ir.destination.reg, ir.destination.width_bits, state);
                    state.expressions[variable.name] = expression;
                    statement.destination = variable;
                    statement.expression = expression;
                    statement.has_destination = true;
                }
                break;
            }
            case IROpcode::BinaryOp: {
                const auto lhs = expression_for_value(ir.destination, state);
                const auto rhs = ir.sources.empty() ? Expression::make_unknown(ir.destination.width_bits)
                                                    : expression_for_value(ir.sources.front(), state);
                auto expression = Expression::make_binary(ir.binary_operator, lhs, rhs, ir.destination.width_bits);
                if (!ir.sources.empty() && same_register_value(ir.destination, ir.sources.front()) &&
                    (ir.binary_operator == IRBinaryOperator::Xor || ir.binary_operator == IRBinaryOperator::Sub)) {
                    expression = Expression::make_constant(0, ir.destination.width_bits);
                }

                if (is_register_value(ir.destination)) {
                    const auto variable = define_register(ir.destination.reg, ir.destination.width_bits, state);
                    state.expressions[variable.name] = expression;
                    statement.destination = variable;
                    statement.expression = expression;
                    statement.has_destination = true;
                } else if (is_stack_memory(ir.destination)) {
                    state.stack_slots[ir.destination.memory.displacement] = expression;
                    statement.expression = expression;
                } else {
                    statement.expression = expression;
                }
                break;
            }
            case IROpcode::Compare:
                if (ir.sources.size() >= 2) {
                    state.comparison_lhs = expression_for_value(ir.sources[0], state);
                    state.comparison_rhs = expression_for_value(ir.sources[1], state);
                }
                statement.expression = Expression::make_unknown();
                break;
            case IROpcode::Test:
                if (ir.sources.size() >= 2) {
                    const auto lhs = expression_for_value(ir.sources[0], state);
                    if (same_register_value(ir.sources[0], ir.sources[1])) {
                        state.comparison_lhs = lhs;
                        state.comparison_rhs = Expression::make_constant(0, ir.sources[0].width_bits);
                    } else {
                        state.comparison_lhs = Expression::make_binary(
                            IRBinaryOperator::And,
                            lhs,
                            expression_for_value(ir.sources[1], state),
                            ir.sources[0].width_bits);
                        state.comparison_rhs = Expression::make_constant(0, ir.sources[0].width_bits);
                    }
                }
                statement.expression = Expression::make_unknown();
                break;
            case IROpcode::ConditionalSelect: {
                const auto previous = expression_for_value(ir.destination, state);
                const auto selected = ir.sources.empty() ? Expression::make_unknown(ir.destination.width_bits)
                                                         : expression_for_value(ir.sources.front(), state);
                const auto condition = Expression::make_comparison(
                    ir.condition,
                    state.comparison_lhs ? state.comparison_lhs : Expression::make_unknown(),
                    state.comparison_rhs ? state.comparison_rhs : Expression::make_unknown());
                const auto expression = Expression::make_select(condition, selected, previous, ir.destination.width_bits);
                if (is_register_value(ir.destination)) {
                    const auto variable = define_register(ir.destination.reg, ir.destination.width_bits, state);
                    state.expressions[variable.name] = expression;
                    statement.destination = variable;
                    statement.expression = expression;
                    statement.has_destination = true;
                }
                break;
            }
            case IROpcode::SetCondition: {
                const auto condition = Expression::make_comparison(
                    ir.condition,
                    state.comparison_lhs ? state.comparison_lhs : Expression::make_unknown(),
                    state.comparison_rhs ? state.comparison_rhs : Expression::make_unknown());
                const auto expression = Expression::make_select(
                    condition,
                    Expression::make_constant(1, ir.destination.width_bits),
                    Expression::make_constant(0, ir.destination.width_bits),
                    ir.destination.width_bits);
                if (is_register_value(ir.destination)) {
                    const auto variable = define_register(ir.destination.reg, ir.destination.width_bits, state);
                    state.expressions[variable.name] = expression;
                    statement.destination = variable;
                    statement.expression = expression;
                    statement.has_destination = true;
                }
                break;
            }
            case IROpcode::Call: {
                std::vector<ExprPtr> arguments;
                for (const auto reg : ABI_ARGUMENT_REGISTERS) {
                    arguments.push_back(current_register(reg, 64, state));
                }

                const uint64_t target = ir.branch_target;
                const auto call_expression = Expression::make_call(target, std::move(arguments), 64);
                const auto return_variable = define_register(x86::Register::RAX, 64, state);
                state.expressions[return_variable.name] = call_expression;

                statement.destination = return_variable;
                statement.expression = call_expression;
                statement.has_destination = true;
                statement.is_call = true;
                break;
            }
            case IROpcode::Return:
                statement.is_return = true;
                statement.expression = ir.sources.empty() ? current_register(x86::Register::RAX, 64, state)
                                                          : expression_for_value(ir.sources.front(), state);
                break;
            case IROpcode::Push:
            case IROpcode::Pop:
            case IROpcode::Jump:
            case IROpcode::Branch:
            case IROpcode::Unknown:
                statement.expression = Expression::make_unknown();
                break;
        }

        ssa_function.statements.push_back(statement);
    }

    return ssa_function;
}

std::string format_ssa_statement(const SSAStatement& statement) {
    std::ostringstream output;

    if (statement.is_return) {
        output << "return " << format_expression(statement.expression);
        return output.str();
    }

    if (statement.has_destination) {
        output << statement.destination.name << '_' << statement.destination.version
               << " = " << format_expression(statement.expression);
        return output.str();
    }

    if (statement.expression) {
        output << "/* " << ir_opcode_name(statement.source_opcode) << ": " << format_expression(statement.expression) << " */";
    } else {
        output << "/* " << ir_opcode_name(statement.source_opcode) << " */";
    }

    return output.str();
}

} // namespace mykisah::core
