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

    while (!isAtEnd()) {
        if (currentToken().code == -1) {
            m_errors.append({currentToken().lexeme, currentToken().line, currentToken().startCol, "Лексическая ошибка"});
            m_pos++;
            continue;
        }

        if (currentToken().code == 1 || currentToken().code == 3) {
            int currentLine = currentToken().line;
            bool lineHasError = false;

            if (!match(1, "Ожидалось 'Const'")) lineHasError = true;
            if (!lineHasError && !match(3, "Ожидалось имя переменной")) lineHasError = true;
            if (!lineHasError && !match(5, "Ожидалось ':'")) lineHasError = true;
            if (!lineHasError && !match(2, "Ожидалось 'string'")) lineHasError = true;
            if (!lineHasError && !match(6, "Ожидалось '='")) lineHasError = true;
            if (!lineHasError && !match(7, "Ожидалось строковое значение")) lineHasError = true;
            if (!lineHasError && !match(8, "Пропущена ';' в конце объявления")) lineHasError = true;

            if (lineHasError) {
                while (!isAtEnd() && currentToken().line == currentLine && currentToken().code != 8) {
                    m_pos++;
                }
                if (!isAtEnd() && currentToken().code == 8) m_pos++;
            }
        } else {
            m_errors.append({currentToken().lexeme, currentToken().line, currentToken().startCol, "Неожиданный фрагмент вне Const"});
            m_pos++;
        }
    }
    return m_errors;
}
