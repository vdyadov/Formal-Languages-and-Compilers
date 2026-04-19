#include "parser.h"
#include <QSet>
#include <QVector>
#include <QStringList>

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
        "Ожидалось 'string'", "Ожидалось '='", "Ожидалась закрытие строки", "Пропущена ';'"
    };

    while (!isAtEnd()) {
        int lineStart = currentToken().line;
        QVector<bool> stepReported(expectedCodes.size(), false);
        QSet<int> tokenReportedCols;

        auto reportStepOnce = [&](int stepIdx, const Token &tok, int colOverride = -1) {
            if (stepIdx < 0 || stepIdx >= stepReported.size()) return;
            if (stepReported[stepIdx]) return;
            const int col = (colOverride >= 0) ? colOverride : tok.startCol;
            m_errors.append({tok.lexeme, tok.line, col, descriptions[stepIdx]});
            stepReported[stepIdx] = true;
        };

        auto reportTokenOnce = [&](const Token &tok, const QString &desc) {
            if (tokenReportedCols.contains(tok.startCol)) return;
            m_errors.append({tok.lexeme, tok.line, tok.startCol, desc});
            tokenReportedCols.insert(tok.startCol);
        };

        int i = 0;
        while (i < expectedCodes.size()) {
            int expected = expectedCodes[i];

            if (isAtEnd() || currentToken().line != lineStart) {
                Token lastT = (m_pos > 0) ? m_tokens[m_pos - 1] : Token();
                if (!stepReported[i]) {
                    m_errors.append({"", lineStart, lastT.endCol + 1, descriptions[i]});
                    stepReported[i] = true;
                }
                i++;
                continue;
            }

            Token t = currentToken();

            if (t.code == -1) {
                const bool isUnclosedString = t.lexeme.startsWith("Незакрытая строка:");

                if (isUnclosedString) {
                    const int valueStepIdx = expectedCodes.indexOf(7);
                    if (valueStepIdx >= 0) {
                        while (i < valueStepIdx) {
                            reportStepOnce(i, t);
                            i++;
                        }
                        reportStepOnce(valueStepIdx, t);
                        m_pos++;
                        i = valueStepIdx + 1;
                        continue;
                    }
                }

                reportTokenOnce(t, "Недопустимый символ");
                m_pos++;
                continue;
            }

            if (t.code == expected) {
                m_pos++;
                i++;
                continue;
            }

            if (i + 1 < expectedCodes.size() && t.code == expectedCodes[i + 1]) {
                reportStepOnce(i, t);
                i++;
                continue;
            }

            reportStepOnce(i, t);
            m_pos++;
        }

        while (!isAtEnd() && currentToken().line == lineStart) {
            Token t = currentToken();
            if (t.code == -1) {
                if (!t.lexeme.startsWith("Незакрытая строка:")) {
                    reportTokenOnce(t, "Лексическая ошибка");
                }
            } else {
                reportTokenOnce(t, "Лишняя лексема");
            }
            m_pos++;
        }
    }
    return m_errors;
}
