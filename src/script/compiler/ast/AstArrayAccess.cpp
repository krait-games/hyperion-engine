#include <script/compiler/ast/AstArrayAccess.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/Module.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <system/Debug.hpp>

namespace hyperion::compiler {

AstArrayAccess::AstArrayAccess(const std::shared_ptr<AstExpression> &target,
    const std::shared_ptr<AstExpression> &index,
    const SourceLocation &location)
    : AstExpression(location, ACCESS_MODE_LOAD | ACCESS_MODE_STORE),
      m_target(target),
      m_index(index)
{
}

void AstArrayAccess::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_target != nullptr);
    AssertThrow(m_index != nullptr);

    m_target->Visit(visitor, mod);
    m_index->Visit(visitor, mod);

    SymbolTypePtr_t target_type = m_target->GetExprType();

    // check if target is an array
    if (target_type != BuiltinTypes::ANY) {
        if (!target_type->IsArrayType()) {
            // not an array type
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_not_an_array,
                m_location,
                target_type->GetName()
            ));
        }
    }
}

std::unique_ptr<Buildable> AstArrayAccess::Build(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_target != nullptr);
    AssertThrow(m_index != nullptr);

    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    bool target_side_effects = m_target->MayHaveSideEffects();
    bool index_side_effects = m_index->MayHaveSideEffects();
    
    uint8_t rp_before = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
    uint8_t rp;
    uint8_t r0, r1;

    Compiler::ExprInfo info {
        m_target.get(), m_index.get()
    };

    if (!index_side_effects) {
        chunk->Append(Compiler::LoadLeftThenRight(visitor, mod, info));
        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        r0 = rp - 1;
        r1 = rp;
    } else if (index_side_effects && !target_side_effects) {
        // load the index and store it
        chunk->Append(Compiler::LoadRightThenLeft(visitor, mod, info));
        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        r0 = rp;
        r1 = rp - 1;
    } else {
        // load target, store it, then load the index
        chunk->Append(Compiler::LoadLeftAndStore(visitor, mod, info));
        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        r0 = rp - 1;
        r1 = rp;
    }

    // do the operation
    if (m_access_mode == ACCESS_MODE_LOAD) {
        auto instr = BytecodeUtil::Make<RawOperation<>>();
        instr->opcode = LOAD_ARRAYIDX;
        instr->Accept<uint8_t>(r0); // destination
        instr->Accept<uint8_t>(r0); // source
        instr->Accept<uint8_t>(r1); // index

        chunk->Append(std::move(instr));
    } else if (m_access_mode == ACCESS_MODE_STORE) {
        auto instr = BytecodeUtil::Make<RawOperation<>>();
        instr->opcode = MOV_ARRAYIDX_REG;
        instr->Accept<uint8_t>(r0); // destination
        instr->Accept<uint8_t>(r1); // index
        instr->Accept<uint8_t>(rp_before - 1); // source

        chunk->Append(std::move(instr));
    }

    // unclaim register
    visitor->GetCompilationUnit()->GetInstructionStream().DecRegisterUsage();

    return std::move(chunk);
}

void AstArrayAccess::Optimize(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_target != nullptr);
    AssertThrow(m_index != nullptr);

    m_target->Optimize(visitor, mod);
    m_index->Optimize(visitor, mod);
}

Pointer<AstStatement> AstArrayAccess::Clone() const
{
    return CloneImpl();
}

Tribool AstArrayAccess::IsTrue() const
{
    return Tribool::Indeterminate();
}

bool AstArrayAccess::MayHaveSideEffects() const
{
    return m_target->MayHaveSideEffects() || m_index->MayHaveSideEffects() ||
        m_access_mode == ACCESS_MODE_STORE;
}

SymbolTypePtr_t AstArrayAccess::GetExprType() const
{
    AssertThrow(m_target != nullptr);

    SymbolTypePtr_t target_type = m_target->GetExprType();
    AssertThrow(target_type != nullptr);

    if (target_type->GetTypeClass() == TYPE_ARRAY) {
        SymbolTypePtr_t held_type = BuiltinTypes::UNDEFINED;

        if (target_type->GetGenericInstanceInfo().m_generic_args.size() == 1) {
            held_type = target_type->GetGenericInstanceInfo().m_generic_args[0].m_type;
            // todo: tuple?
        }

        AssertThrow(held_type != nullptr);
        return held_type;
    }

    return BuiltinTypes::ANY;
}

AstExpression *AstArrayAccess::GetTarget() const
{
    if (m_target != nullptr) {
        if (auto *nested_target = m_target->GetTarget()) {
            return nested_target;
        }

        return m_target.get();
    }

    return AstExpression::GetTarget();
}

} // namespace hyperion::compiler
