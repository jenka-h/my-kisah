#pragma once

#include "decompiler/controlflow/CFG.h"
#include "decompiler/analysis/Signature.h"

#include <cstdint>
#include <map>
#include <string>

namespace mykisah::core {

enum class TypeCategory {
    Unknown,
    Integer,
    Pointer,
    BooleanLike,
    Float,
};

enum class TypeEvidence : uint32_t {
    None = 0,
    Width8 = 1U << 0U,
    Width16 = 1U << 1U,
    Width32 = 1U << 2U,
    Width64 = 1U << 3U,
    UsedInArithmetic = 1U << 4U,
    UsedAsAddress = 1U << 5U,
    Dereferenced = 1U << 6U,
    Compared = 1U << 7U,
    ComparedToZero = 1U << 8U,
    UsedAsCondition = 1U << 9U,
    UsedInPointerArithmetic = 1U << 10U,
};

struct TypeInfo {
    TypeCategory category = TypeCategory::Unknown;
    uint16_t width_bits = 0;
    uint32_t evidence = 0;
    bool conflicting = false;
};

struct FunctionTypeInfo {
    std::map<x86::Register, TypeInfo> register_types;
    TypeInfo return_type;
};

class TypeInference {
public:
    [[nodiscard]] FunctionTypeInfo analyze(const CFG& cfg, const FunctionSignature& signature) const;

private:
    void add_evidence(TypeInfo& info, TypeEvidence evidence, uint16_t width_bits = 0) const;
    [[nodiscard]] TypeCategory resolve(const TypeInfo& info) const;
    void collect_operand_evidence(
        FunctionTypeInfo& result,
        const x86::Instruction& instruction,
        const x86::Operand& operand) const;
};

[[nodiscard]] std::string type_category_name(TypeCategory category);
[[nodiscard]] std::string format_type_info(const TypeInfo& info);

} // namespace mykisah::core
