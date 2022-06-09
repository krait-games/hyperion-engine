#ifndef AST_TYPE_EXPRESSION_HPP
#define AST_TYPE_EXPRESSION_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstTypeSpecification.hpp>
#include <script/compiler/ast/AstVariableDeclaration.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>

#include <string>
#include <memory>
#include <vector>

namespace hyperion::compiler {

class AstTypeExpression : public AstExpression {
public:
    AstTypeExpression(
        const std::shared_ptr<AstTypeSpecification> &base_specification,
        const std::vector<std::shared_ptr<AstVariableDeclaration>> &members,
        const std::vector<std::shared_ptr<AstVariableDeclaration>> &static_members,
        const SourceLocation &location);
    virtual ~AstTypeExpression() = default;

    inline const std::vector<std::shared_ptr<AstVariableDeclaration>>
        &GetMembers() const { return m_members; }
    inline int GetNumMembers() const { return m_num_members; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;

    virtual bool IsLiteral() const override;
    virtual Pointer<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;
    virtual const AstExpression *GetValueOf() const override;

protected:
    std::shared_ptr<AstTypeSpecification> m_base_specification;
    std::vector<std::shared_ptr<AstVariableDeclaration>> m_members;
    std::vector<std::shared_ptr<AstVariableDeclaration>> m_static_members;
    int m_num_members;

    SymbolTypePtr_t m_symbol_type;

    std::shared_ptr<AstTypeObject> m_expr;

    inline Pointer<AstTypeExpression> CloneImpl() const
    {
        return Pointer<AstTypeExpression>(new AstTypeExpression(
            CloneAstNode(m_base_specification),
            CloneAllAstNodes(m_members),
            CloneAllAstNodes(m_static_members),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
