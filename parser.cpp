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

bool Parser::tryParseConstLine()
{
    if (isAtEnd())
        return false;

    const int savePos = m_pos;
    const int lineStart = currentToken().line;

    static const int chain[] = {1, 3, 5, 2, 6, 7, 8};
    QVector<Token> captured;
    captured.reserve(8);

    for (int expected : chain) {
        if (isAtEnd()) {
            m_pos = savePos;
            return false;
        }
        Token t = currentToken();
        if (t.line != lineStart) {
            m_pos = savePos;
            return false;
        }
        if (t.code == -1) {
            m_pos = savePos;
            return false;
        }
        if (t.code != expected) {
            m_pos = savePos;
            return false;
        }
        captured.append(t);
        m_pos++;
    }

    const bool hasExtraOnLine = (!isAtEnd() && currentToken().line == lineStart);

    if (!hasExtraOnLine) {
        QString idLex = captured[1].lexeme;
        QString value = captured[5].lexeme;
        if (value.startsWith(QLatin1Char('\'')) && value.endsWith(QLatin1Char('\'')) && value.size() >= 2)
            value = value.mid(1, value.size() - 2);

        auto literal = std::make_unique<StringLiteralNode>(
            SourcePos{captured[5].line, captured[5].startCol}, std::move(value));
        auto strType = std::make_unique<StringTypeNode>(
            SourcePos{captured[3].line, captured[3].startCol});
        auto decl = std::make_unique<ConstDeclNode>(
            SourcePos{captured[1].line, captured[1].startCol},
            std::move(idLex),
            std::move(strType),
            std::move(literal));

        if (!m_program)
            m_program = std::make_unique<ProgramNode>();
        m_program->declarations.push_back(std::move(decl));
    }

    if (!hasExtraOnLine)
        return true;

    QSet<int> reportedCols;
    while (!isAtEnd() && currentToken().line == lineStart) {
        Token t = currentToken();
        if (t.code == -1) {
            if (!lexemeIsUnclosedStringError(t.lexeme)) {
                if (!reportedCols.contains(t.startCol)) {
                    m_errors.append({t.lexeme, t.line, t.startCol, QStringLiteral("Лексическая ошибка")});
                    reportedCols.insert(t.startCol);
                }
            }
        } else if (!reportedCols.contains(t.startCol)) {
            m_errors.append({t.lexeme, t.line, t.startCol, QStringLiteral("Лишняя лексема")});
            reportedCols.insert(t.startCol);
        }
        m_pos++;
    }

    return true;
}

void Parser::parseLineWithErrors(int lineStart)
{
    QVector<int> expectedCodes = {1, 3, 5, 2, 6, 7, 8};
    QStringList descriptions = {
        QStringLiteral("Ожидалось 'Const'"),
        QStringLiteral("Ожидалось имя переменной"),
        QStringLiteral("Ожидалось ':'"),
        QStringLiteral("Ожидалось 'string'"),
        QStringLiteral("Ожидалось '='"),
        QStringLiteral("Ожидалась закрытие строки"),
        QStringLiteral("Пропущена ';'")
    };

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

    auto reportInvalidSinglesInRange = [&](int fromInclusive, int toExclusive) {
        for (int q = fromInclusive; q < toExclusive && q < m_tokens.size(); ++q) {
            const Token &sk = m_tokens.at(q);
            if (sk.line != lineStart)
                break;
            if (sk.code == -1 && sk.lexeme.size() == 1 && !lexemeIsUnclosedStringError(sk.lexeme))
                reportTokenOnce(sk, QStringLiteral("Недопустимый символ"));
        }
    };

    auto isJunkFragmentToken = [](const Token &tk) -> bool {
        if (tk.code == 3)
            return true;
        if (tk.code == -1 && tk.lexeme.size() == 1 && !lexemeIsUnclosedStringError(tk.lexeme))
            return true;
        return false;
    };

    auto tryStructuralSyncOnLine = [&](int stepIdx, const Token &curTok, int &i) -> bool {
        if (stepIdx < 0 || stepIdx >= expectedCodes.size())
            return false;
        const int want = expectedCodes[stepIdx];
        static const QSet<int> recoverableWants{2, 5, 6, 7, 8};
        if (!recoverableWants.contains(want))
            return false;
        if (curTok.code != 3)
            return false;

        int foundWant = -1;
        for (int q = m_pos + 1; q < m_tokens.size(); ++q) {
            const Token &tk = m_tokens.at(q);
            if (tk.line != lineStart)
                break;
            if (tk.code == want) {
                foundWant = q;
                break;
            }
            if (!isJunkFragmentToken(tk))
                break;
        }
        if (foundWant >= 0) {
            reportInvalidSinglesInRange(m_pos + 1, foundWant);
            m_pos = foundWant;
            return true;
        }

        if (want == 5) {
            int eqIdx = -1;
            for (int q = m_pos + 1; q < m_tokens.size(); ++q) {
                const Token &tk = m_tokens.at(q);
                if (tk.line != lineStart)
                    break;
                if (tk.code == 5)
                    break;
                if (tk.code == 6) {
                    eqIdx = q;
                    break;
                }
                if (!isJunkFragmentToken(tk))
                    break;
            }
            if (eqIdx >= 0) {
                reportStepOnce(stepIdx, curTok);
                if (stepIdx + 1 < stepReported.size())
                    stepReported[stepIdx + 1] = true;
                reportInvalidSinglesInRange(m_pos + 1, eqIdx);
                m_pos = eqIdx;
                i = stepIdx + 2;
                return true;
            }
        }

        if (want == 2) {
            int eqIdx = -1;
            for (int q = m_pos + 1; q < m_tokens.size(); ++q) {
                const Token &tk = m_tokens.at(q);
                if (tk.line != lineStart)
                    break;
                if (tk.code == 2)
                    break;
                if (tk.code == 6) {
                    eqIdx = q;
                    break;
                }
                if (!isJunkFragmentToken(tk))
                    break;
            }
            if (eqIdx >= 0) {
                reportStepOnce(stepIdx, curTok);
                reportInvalidSinglesInRange(m_pos + 1, eqIdx);
                m_pos = eqIdx;
                i = stepIdx + 1;
                return true;
            }
        }

        return false;
    };

    int i = 0;
    while (i < expectedCodes.size()) {
        int expected = expectedCodes[i];

        if (isAtEnd() || currentToken().line != lineStart) {
            Token lastT = (m_pos > 0) ? m_tokens[m_pos - 1] : Token();
            if (!stepReported[i]) {
                m_errors.append({QString(), lineStart, lastT.endCol + 1, descriptions[i]});
                stepReported[i] = true;
            }
            i++;
            continue;
        }

        Token t = currentToken();

        if (t.code == -1) {
            if (lexemeIsUnclosedStringError(t.lexeme)) {
                const int valueStepIdx = expectedCodes.indexOf(7);
                if (valueStepIdx >= 0) {
                    for (int s = 0; s < valueStepIdx; ++s)
                        stepReported[s] = true;
                    reportStepOnce(valueStepIdx, t);
                    m_pos++;
                    i = valueStepIdx + 1;
                    continue;
                }
            }

            reportTokenOnce(t, QStringLiteral("Недопустимый символ"));
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

        if (tryStructuralSyncOnLine(i, t, i))
            continue;

        reportStepOnce(i, t);
        m_pos++;
    }

    while (!isAtEnd() && currentToken().line == lineStart) {
        Token t = currentToken();
        if (t.code == -1) {
            if (!lexemeIsUnclosedStringError(t.lexeme)) {
                reportTokenOnce(t, QStringLiteral("Лексическая ошибка"));
            }
        } else {
            reportTokenOnce(t, QStringLiteral("Лишняя лексема"));
        }
        m_pos++;
    }
}

QList<SyntaxError> Parser::parse()
{
    m_errors.clear();
    m_pos = 0;
    m_program = std::make_unique<ProgramNode>();

    if (m_tokens.isEmpty())
        return m_errors;

    while (!isAtEnd()) {
        if (tryParseConstLine())
            continue;
        const int lineStart = currentToken().line;
        parseLineWithErrors(lineStart);
    }

    return m_errors;
}

std::unique_ptr<ProgramNode> Parser::takeProgram()
{
    return std::move(m_program);
}
