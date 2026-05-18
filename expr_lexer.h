#ifndef EXPR_LEXER_H
#define EXPR_LEXER_H

#include <QList>
#include <QString>
#include "token.h"

namespace ExprTokenCode {
constexpr int NUM = 1;
constexpr int ID = 2;
constexpr int PLUS = 3;
constexpr int MINUS = 4;
constexpr int MULT = 5;
constexpr int DIV = 6;
constexpr int LPAREN = 7;
constexpr int RPAREN = 8;
constexpr int ERROR = -1;
}

class ExprLexer {
public:
    QList<Token> tokenize(const QString &source);
};

#endif // EXPR_LEXER_H
