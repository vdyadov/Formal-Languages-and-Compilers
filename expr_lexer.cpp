#include "expr_lexer.h"

QList<Token> ExprLexer::tokenize(const QString &source) {
    QList<Token> tokens;
    int pos = 0;
    int line = 1;
    int column = 1;

    auto appendToken = [&](int code, const QString &typeName, const QString &lexeme,
                           int startCol, int endCol) {
        tokens.append(Token(code, typeName, lexeme, line, startCol, endCol));
    };

    while (pos < source.length()) {
        const QChar ch = source[pos];

        if (ch.isSpace()) {
            if (ch == QLatin1Char('\n')) {
                ++line;
                column = 1;
            } else {
                ++column;
            }
            ++pos;
            continue;
        }

        const int startCol = column;

        if (ch.isDigit()) {
            QString num;
            while (pos < source.length() && source[pos].isDigit()) {
                num += source[pos];
                ++pos;
                ++column;
            }
            appendToken(ExprTokenCode::NUM, QStringLiteral("num"),
                        num, startCol, column - 1);
            continue;
        }

        if (ch.isLetter()) {
            QString id;
            while (pos < source.length()
                   && (source[pos].isLetter() || source[pos].isDigit())) {
                id += source[pos];
                ++pos;
                ++column;
            }
            appendToken(ExprTokenCode::ID, QStringLiteral("id"),
                        id, startCol, column - 1);
            continue;
        }

        switch (ch.unicode()) {
        case '+':
            appendToken(ExprTokenCode::PLUS, QStringLiteral("+"),
                        QStringLiteral("+"), startCol, startCol);
            ++pos;
            ++column;
            break;
        case '-':
            appendToken(ExprTokenCode::MINUS, QStringLiteral("-"),
                        QStringLiteral("-"), startCol, startCol);
            ++pos;
            ++column;
            break;
        case '*':
            appendToken(ExprTokenCode::MULT, QStringLiteral("*"),
                        QStringLiteral("*"), startCol, startCol);
            ++pos;
            ++column;
            break;
        case '/':
            appendToken(ExprTokenCode::DIV, QStringLiteral("/"),
                        QStringLiteral("/"), startCol, startCol);
            ++pos;
            ++column;
            break;
        case '(':
            appendToken(ExprTokenCode::LPAREN, QStringLiteral("("),
                        QStringLiteral("("), startCol, startCol);
            ++pos;
            ++column;
            break;
        case ')':
            appendToken(ExprTokenCode::RPAREN, QStringLiteral(")"),
                        QStringLiteral(")"), startCol, startCol);
            ++pos;
            ++column;
            break;
        default:
            appendToken(ExprTokenCode::ERROR, QStringLiteral("ОШИБКА"),
                        QString(ch), startCol, startCol);
            ++pos;
            ++column;
            break;
        }
    }

    return tokens;
}
