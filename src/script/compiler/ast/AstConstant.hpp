#ifndef AST_CONSTANT_HPP
#define AST_CONSTANT_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/Operator.hpp>

#include <script/Typedefs.hpp>

namespace hyperion::compiler {

class AstConstant : public AstExpression {
public:
    AstConstant(const SourceLocation &location);
    virtual ~AstConstant() = default;

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override = 0;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;

    virtual bool IsLiteral() const override { return true; }
    virtual Pointer<AstStatement> Clone() const override = 0;

    virtual Tribool IsTrue() const override = 0;
    virtual bool MayHaveSideEffects() const override;
    virtual bool IsNumber() const = 0;
    virtual hyperion::aint32 IntValue() const = 0;
    virtual hyperion::afloat32 FloatValue() const = 0;

    virtual std::shared_ptr<AstConstant> HandleOperator(Operators op_type, const AstConstant *right) const = 0;
};

} // namespace hyperion::compiler

#endif
