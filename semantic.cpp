#include "semantic.h"

#include <QSet>

SemanticAnalyzer::Result SemanticAnalyzer::analyze(std::unique_ptr<ProgramNode> source)
{
    Result r;
    r.program = std::make_unique<ProgramNode>();

    if (!source)
        return r;

    QSet<QString> definedNames;

    for (auto &declPtr : source->declarations) {
        if (!declPtr)
            continue;

        ConstDeclNode &decl = *declPtr;

        if (definedNames.contains(decl.name)) {
            r.errors.append({decl.name, decl.namePos.line, decl.namePos.col,
                             QStringLiteral("Повторное объявление идентификатора в области видимости")});
            declPtr.reset();
            continue;
        }

        if (!decl.literal || !decl.typeNode) {
            declPtr.reset();
            continue;
        }

        const QString &val = decl.literal->value;
        if (val.length() > MAX_STRING_LITERAL_LENGTH) {
            QString frag = val.length() > 48 ? val.left(48) + QStringLiteral("...") : val;
            r.errors.append({frag,
                             decl.literal->pos.line, decl.literal->pos.col,
                             QStringLiteral("Длина строкового литерала превышает допустимое значение (%1 символов)")
                                 .arg(MAX_STRING_LITERAL_LENGTH)});
            declPtr.reset();
            continue;
        }

        definedNames.insert(decl.name);
        r.program->declarations.push_back(std::move(declPtr));
    }

    return r;
}
