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

    Token(int c, QString type, QString val, int l, int s, int e)
        : code(c), typeName(type), lexeme(val), line(l), startCol(s), endCol(e) {}

    // Удобный метод для формирования строки местоположения
    QString getLocation() const {
        return QString("строка %1, %2-%3").arg(line).arg(startCol).arg(endCol);
    }
};
#endif // TOKEN_H
