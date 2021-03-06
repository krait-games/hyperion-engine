#include <script/compiler/ast/AstSyntaxDefinition.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Lexer.hpp>
#include <script/compiler/Parser.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/CompilationUnit.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

namespace hyperion::compiler {

AstSyntaxDefinition::AstSyntaxDefinition(
    const std::shared_ptr<AstString> &syntax_string,
    const std::shared_ptr<AstString> &transform_string,
    const SourceLocation &location)
    : AstStatement(location),
      m_syntax_string(syntax_string),
      m_transform_string(transform_string)
{
}

void AstSyntaxDefinition::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_syntax_string != nullptr);
    AssertThrow(m_transform_string != nullptr);

    std::cout << "syntax string: " << m_syntax_string->GetValue() << "\n"
      << "transform string: " << m_transform_string->GetValue() << "\n";
}

std::unique_ptr<Buildable> AstSyntaxDefinition::Build(AstVisitor *visitor, Module *mod)
{
    return nullptr;
}

void AstSyntaxDefinition::Optimize(AstVisitor *visitor, Module *mod)
{
}

Pointer<AstStatement> AstSyntaxDefinition::Clone() const
{
    return CloneImpl();
}

} // namespace hyperion::compiler
