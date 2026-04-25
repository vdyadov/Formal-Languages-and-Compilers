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

static QString escapeForDoubleQuotes(QString s)
{
    s.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    s.replace(QLatin1Char('\"'), QStringLiteral("\\\""));
    s.replace(QLatin1Char('\n'), QStringLiteral("\\n"));
    s.replace(QLatin1Char('\r'), QStringLiteral("\\r"));
    s.replace(QLatin1Char('\t'), QStringLiteral("\\t"));
    return s;
}

static QString escapeForSingleQuotes(QString s)
{
    s.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    s.replace(QLatin1Char('\''), QStringLiteral("\\'"));
    s.replace(QLatin1Char('\n'), QStringLiteral("\\n"));
    s.replace(QLatin1Char('\r'), QStringLiteral("\\r"));
    s.replace(QLatin1Char('\t'), QStringLiteral("\\t"));
    return s;
}

static void appendBranchLine(QString &out, const QString &prefix, bool isLast, const QString &text)
{
    out += prefix;
    out += isLast ? QStringLiteral("└── ") : QStringLiteral("├── ");
    out += text;
    out += QLatin1Char('\n');
}

static QString childPrefix(const QString &prefix, bool parentIsLast)
{
    return prefix + (parentIsLast ? QStringLiteral("    ") : QStringLiteral("│   "));
}

} // namespace

QString formatTree(const ProgramNode &program)
{
    QString out;
    out += QStringLiteral("ProgramNode\n");

    const int n = static_cast<int>(program.declarations.size());
    if (n == 0) {
        appendBranchLine(out, QString(), true, QStringLiteral("declarations: []"));
        return out;
    }

    // declarations: [...]
    appendBranchLine(out, QString(), true, QStringLiteral("declarations:"));
    const QString declsPrefix = childPrefix(QString(), true);

    for (int i = 0; i < n; ++i) {
        const bool lastDecl = (i == n - 1);
        const ConstDeclNode &d = *program.declarations[static_cast<size_t>(i)];

        // [i]: ConstDeclNode
        appendBranchLine(out, declsPrefix, lastDecl,
                         QStringLiteral("[%1]: ConstDeclNode").arg(i));

        const QString declPrefix = childPrefix(declsPrefix, lastDecl);

        // ├── name: "..."
        appendBranchLine(out, declPrefix, false,
                         QStringLiteral("name: \"%1\"").arg(escapeForDoubleQuotes(d.name)));

        // ├── modifiers: ["const"]
        appendBranchLine(out, declPrefix, false,
                         QStringLiteral("modifiers: [\"const\"]"));

        // ├── type: TypeNode
        const bool hasType = static_cast<bool>(d.typeNode);
        const bool hasValue = static_cast<bool>(d.literal);

        if (hasType) {
            appendBranchLine(out, declPrefix, !hasValue,
                             QStringLiteral("type: TypeNode"));

            const QString typePrefix = childPrefix(declPrefix, !hasValue);
            appendBranchLine(out, typePrefix, true, QStringLiteral("name: \"string\""));
        }

        if (hasValue) {
            appendBranchLine(out, declPrefix, true,
                             QStringLiteral("value: StringLiteralNode"));
            const QString valuePrefix = childPrefix(declPrefix, true);
            appendBranchLine(out, valuePrefix, true,
                             QStringLiteral("value: '%1'").arg(escapeForSingleQuotes(d.literal->value)));
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
