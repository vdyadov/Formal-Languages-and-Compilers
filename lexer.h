#ifndef LEXER_H
#define LEXER_H

#include <QString>
#include <QList>
#include "token.h"

class Lexer {
public:
    Lexer() = default;
    QList<Token> tokenize(const QString &source);

private:
    const QStringList keywords = {"const", "string"};
};

#endif // LEXER_H
