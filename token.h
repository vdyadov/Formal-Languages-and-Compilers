#ifndef TOKEN_H
#define TOKEN_H

#include <QString>

struct Token {
    int code;
    QString typeName;
    QString lexeme;
    int line;
    int startCol;
    int endCol;

    Token() : code(0), typeName(""), lexeme(""), line(0), startCol(0), endCol(0) {}

    Token(int c, QString type, QString val, int l, int s, int e)
        : code(c), typeName(type), lexeme(val), line(l), startCol(s), endCol(e) {}

    QString getLocation() const {
        return QString("строка %1, %2-%3").arg(line).arg(startCol).arg(endCol);
    }
};
#endif // TOKEN_H
