#ifndef PARSER_H
#define PARSER_H

#include <QList>
#include <QString>
#include "token.h"

struct SyntaxError {
    QString fragment;
    int line;
    int col;
    QString description;
};

class Parser {
public:
    explicit Parser(const QList<Token> &tokens);
    QList<SyntaxError> parse();

private:
    QList<Token> m_tokens;
    int m_pos;
    QList<SyntaxError> m_errors;

    const Token& currentToken();
    bool isAtEnd();

    bool match(int expectedCode, const QString &errorDescription);
};

#endif
