#ifndef AST_H
#define AST_H

#include <QString>
#include <QJsonObject>
#include <vector>
#include <memory>

struct SourcePos {
    int line = 0;
    int col = 0;
};

class StringTypeNode;
class StringLiteralNode;

class ConstDeclNode {
public:
    SourcePos namePos;
    QString name;
    std::unique_ptr<StringTypeNode> typeNode;
    std::unique_ptr<StringLiteralNode> literal;

    ConstDeclNode(SourcePos idPos, QString idName,
                  std::unique_ptr<StringTypeNode> typeN,
                  std::unique_ptr<StringLiteralNode> lit);
};

class StringTypeNode {
public:
    SourcePos pos;

    explicit StringTypeNode(SourcePos p);
};

class StringLiteralNode {
public:
    SourcePos pos;
    QString value;

    StringLiteralNode(SourcePos p, QString unquoted);
};

class ProgramNode {
public:
    std::vector<std::unique_ptr<ConstDeclNode>> declarations;
};

QString formatTree(const ProgramNode &program);
QJsonObject toJson(const ProgramNode &program);

#endif
