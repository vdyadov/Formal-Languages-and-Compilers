#include "parser.h"

Parser::Parser(const QList<Token> &tokens) : m_pos(0) {
    for (const auto &t : tokens) {
        if (t.code != 4) m_tokens.append(t);
    }
}

const Token& Parser::currentToken() {
    if (m_pos < m_tokens.size()) return m_tokens.at(m_pos);
    static Token eof(-100, "Конец", "EOF", 0, 0, 0);
    if (!m_tokens.isEmpty()) {
        eof.line = m_tokens.last().line;
        eof.startCol = m_tokens.last().endCol + 1;
    }
    return eof;
}

bool Parser::isAtEnd() { return m_pos >= m_tokens.size(); }

bool Parser::match(int expectedCode, const QString &errorDescription) {
    if (!isAtEnd() && currentToken().code == expectedCode) {
        m_pos++;
        return true;
    }

    Token errorToken = currentToken();

    if (expectedCode == 8 && m_pos > 0) {
        Token prev = m_tokens.at(m_pos - 1);
        m_errors.append({prev.lexeme, prev.line, prev.endCol + 1, errorDescription});
    } else {
        m_errors.append({errorToken.lexeme, errorToken.line, errorToken.startCol, errorDescription});
    }

    return false;
}

QList<SyntaxError> Parser::parse() {
    m_errors.clear();
    m_pos = 0;
    if (m_tokens.isEmpty()) return m_errors;

    enum State { CONST_ST, ID_ST, COLON_ST, TYPE_ST, EQUALS_ST, VALUE_ST, SEMI_ST, SYNC_ST };

    QVector<int> expectedCodes = {1, 3, 5, 2, 6, 7, 8};
    QStringList descriptions = {
        "Ожидалось 'Const'", "Ожидалось имя переменной", "Ожидалось ':'",
        "Ожидалось 'string'", "Ожидалось '='", "Ожидалась строка", "Пропущена ';'"
    };

    while (!isAtEnd()) {
        int lineStart = currentToken().line;

        for (int i = 0; i < expectedCodes.size(); ++i) {
            int expected = expectedCodes[i];

            if (isAtEnd() || currentToken().line != lineStart) {
                Token lastT = (m_pos > 0) ? m_tokens[m_pos - 1] : Token();
                m_errors.append({"", lineStart, lastT.endCol + 1, descriptions[i]});
                continue;
            }

            Token t = currentToken();

            if (t.code == expected) {
                m_pos++;
            } else {
                m_errors.append({t.lexeme, t.line, t.startCol, descriptions[i]});

                if (i + 1 < expectedCodes.size() && t.code == expectedCodes[i + 1]) {
                } else {
                    m_pos++;
                }
            }
        }

        while (!isAtEnd() && currentToken().line == lineStart) {
            m_errors.append({currentToken().lexeme, lineStart, currentToken().startCol, "Лишняя лексема"});
            m_pos++;
        }
    }
    return m_errors;
}
