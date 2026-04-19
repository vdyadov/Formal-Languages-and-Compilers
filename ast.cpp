#include "ast.h"

#include <QJsonArray>

ConstDeclNode::ConstDeclNode(SourcePos idPos, QString idName,
                             std::unique_ptr<StringTypeNode> typeN,
                             std::unique_ptr<StringLiteralNode> lit)
    : namePos(idPos)
    , name(std::move(idName))
    , typeNode(std::move(typeN))
    , literal(std::move(lit)) {}

StringTypeNode::StringTypeNode(SourcePos p) : pos(p) {}

StringLiteralNode::StringLiteralNode(SourcePos p, QString unquoted)
    : pos(p), value(std::move(unquoted)) {}

namespace {

static QJsonObject posJson(const SourcePos &p)
{
    QJsonObject o;
    o[QStringLiteral("line")] = p.line;
    o[QStringLiteral("col")] = p.col;
    return o;
}

} // namespace

QString formatTree(const ProgramNode &program)
{
    QString out;
    out += QStringLiteral("Program\n");

    const int n = static_cast<int>(program.declarations.size());
    if (n == 0) {
        out += QStringLiteral("└── (пусто)\n");
        return out;
    }

    for (int i = 0; i < n; ++i) {
        const bool lastDecl = (i == n - 1);
        const QString pre = QString();
        const QString declPre = pre + (lastDecl ? QStringLiteral("└── ") : QStringLiteral("├── "));
        const ConstDeclNode &d = *program.declarations[static_cast<size_t>(i)];

        out += declPre;
        out += QStringLiteral("ConstDecl (%1)\n").arg(d.name);

        const QString childIndent = pre + (lastDecl ? QStringLiteral("    ") : QStringLiteral("│   "));
        if (d.typeNode && d.literal) {
            out += childIndent;
            out += QStringLiteral("├── StringType\n");
            const QString litEsc = d.literal->value;
            out += childIndent;
            out += QStringLiteral("└── StringLiteral \"%1\"\n").arg(litEsc);
        } else if (d.typeNode) {
            out += childIndent;
            out += QStringLiteral("└── StringType\n");
        } else if (d.literal) {
            const QString litEsc = d.literal->value;
            out += childIndent;
            out += QStringLiteral("└── StringLiteral \"%1\"\n").arg(litEsc);
        }
    }

    return out;
}

static QJsonObject litToJson(const StringLiteralNode &n)
{
    QJsonObject o;
    o[QStringLiteral("kind")] = QStringLiteral("StringLiteral");
    o[QStringLiteral("value")] = n.value;
    o[QStringLiteral("pos")] = posJson(n.pos);
    return o;
}

static QJsonObject typeToJson(const StringTypeNode &n)
{
    QJsonObject o;
    o[QStringLiteral("kind")] = QStringLiteral("StringType");
    o[QStringLiteral("pos")] = posJson(n.pos);
    return o;
}

static QJsonObject declToJson(const ConstDeclNode &d)
{
    QJsonObject o;
    o[QStringLiteral("kind")] = QStringLiteral("ConstDecl");
    o[QStringLiteral("name")] = d.name;
    o[QStringLiteral("namePos")] = posJson(d.namePos);
    QJsonArray ch;
    if (d.typeNode)
        ch.append(typeToJson(*d.typeNode));
    if (d.literal)
        ch.append(litToJson(*d.literal));
    o[QStringLiteral("children")] = ch;
    return o;
}

QJsonObject toJson(const ProgramNode &program)
{
    QJsonObject root;
    root[QStringLiteral("kind")] = QStringLiteral("Program");
    QJsonArray decls;
    for (const auto &d : program.declarations)
        decls.append(declToJson(*d));
    root[QStringLiteral("declarations")] = decls;
    return root;
}
