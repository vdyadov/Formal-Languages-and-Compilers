#ifndef PARSER_H
#define PARSER_H

#include <QList>
#include <QString>
#include <memory>
#include "ast.h"
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
    std::unique_ptr<ProgramNode> takeProgram();

private:
    QList<Token> m_tokens;
    int m_pos;
    QList<SyntaxError> m_errors;
    std::unique_ptr<ProgramNode> m_program;

    const Token& currentToken();
    bool isAtEnd();

    bool tryParseConstLine();
    void parseLineWithErrors(int lineStart);
};

#endif
