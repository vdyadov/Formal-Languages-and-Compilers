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
    QSet<int> suppressedLexCols;

    auto mergeDescriptions = [&](const QString &a, const QString &b) -> QString {
        if (a == b)
            return a;

        const bool aIsPrimary = a.startsWith(QStringLiteral("Ожидалось")) || a.startsWith(QStringLiteral("Пропущена"))
            || a.startsWith(QStringLiteral("Лишний идентификатор"));
        const bool bIsPrimary = b.startsWith(QStringLiteral("Ожидалось")) || b.startsWith(QStringLiteral("Пропущена"))
            || b.startsWith(QStringLiteral("Лишний идентификатор"));

        QString base = a;
        QString detail = b;
        if (!aIsPrimary && bIsPrimary) {
            base = b;
            detail = a;
        }

        if (base.contains(QStringLiteral(" ("))) {
            // Already has details.
            int open = base.lastIndexOf(QStringLiteral(" ("));
            int close = base.endsWith(QLatin1Char(')')) ? base.size() - 1 : -1;
            if (open >= 0 && close > open) {
                const QString prefix = base.left(open);
                const QString inside = base.mid(open + 2, close - (open + 2));
                if (inside.split(QStringLiteral(", ")).contains(detail))
                    return base;
                return prefix + QStringLiteral(" (") + inside + QStringLiteral(", ") + detail + QLatin1Char(')');
            }
        }

        return base + QStringLiteral(" (") + detail + QLatin1Char(')');
    };

    auto addOrMergeErrorAt = [&](const QString &fragment, int line, int col, const QString &desc) {
        for (int k = 0; k < m_errors.size(); ++k) {
            SyntaxError &e = m_errors[k];
            if (e.line == line && e.col == col) {
                e.description = mergeDescriptions(e.description, desc);
                if (e.fragment.isEmpty())
                    e.fragment = fragment;
                return;
            }
        }
        m_errors.append({fragment, line, col, desc});
    };

    auto shouldReportStep = [&](int stepIdx) -> bool {
        (void)stepIdx;
        return true;
    };

    auto reportStepOnce = [&](int stepIdx, const Token &tok, int colOverride = -1) {
        if (stepIdx < 0 || stepIdx >= stepReported.size()) return;
        if (!shouldReportStep(stepIdx)) return;
        if (stepReported[stepIdx]) return;
        const int col = (colOverride >= 0) ? colOverride : tok.startCol;
        addOrMergeErrorAt(tok.lexeme, tok.line, col, descriptions[stepIdx]);
        stepReported[stepIdx] = true;
    };

    auto reportTokenOnce = [&](const Token &tok, const QString &desc) {
        if (tokenReportedCols.contains(tok.startCol)) return;
        if (desc == QStringLiteral("Лексическая ошибка") && suppressedLexCols.contains(tok.startCol))
            return;
        addOrMergeErrorAt(tok.lexeme, tok.line, tok.startCol, desc);
        tokenReportedCols.insert(tok.startCol);
    };

    auto mergeLexIntoStepAt = [&](int stepIdx, const Token &anchorTok, int anchorCol, const QList<int> &lexColsToSuppress) {
        // Ensure "Ожидалось ..." exists at anchor and add "(Лексическая ошибка)".
        if (stepIdx >= 0 && stepIdx < descriptions.size())
            addOrMergeErrorAt(anchorTok.lexeme, anchorTok.line, anchorCol, descriptions[stepIdx]);
        addOrMergeErrorAt(anchorTok.lexeme, anchorTok.line, anchorCol, QStringLiteral("Лексическая ошибка"));
        for (int c : lexColsToSuppress)
            suppressedLexCols.insert(c);
    };

    auto nextOnLine = [&](int fromPos, int wantCode) -> int {
        for (int q = fromPos; q < m_tokens.size(); ++q) {
            const Token &tk = m_tokens.at(q);
            if (tk.line != lineStart)
                break;
            if (tk.code == wantCode)
                return q;
        }
        return -1;
    };

    auto collectBrokenWordLexColsFrom = [&](int fromPos) -> QList<int> {
        // Detect pattern: <id> <invalid-char> <id> with NO spaces between them (contiguous columns),
        // possibly repeated. Return invalid-char columns; empty => not a broken word.
        if (fromPos < 0 || fromPos >= m_tokens.size())
            return {};
        const Token &a0 = m_tokens.at(fromPos);
        if (a0.line != lineStart || a0.code != 3)
            return {};

        QList<int> cols;
        int curEnd = a0.endCol;
        int q = fromPos + 1;
        while (q + 1 < m_tokens.size()) {
            const Token &inv = m_tokens.at(q);
            const Token &b = m_tokens.at(q + 1);
            if (inv.line != lineStart || b.line != lineStart)
                break;
            if (!(inv.code == -1 && inv.lexeme.size() == 1 && !lexemeIsUnclosedStringError(inv.lexeme)))
                break;
            if (b.code != 3)
                break;
            if (inv.startCol != curEnd + 1)
                break;
            if (b.startCol != inv.endCol + 1)
                break;
            cols.append(inv.startCol);

            // Continue chain using b as next left fragment.
            curEnd = b.endCol;
            q += 2;
        }
        return cols;
    };

    auto reportInvalidSinglesInRange = [&](int fromInclusive, int toExclusive) {
        for (int q = fromInclusive; q < toExclusive && q < m_tokens.size(); ++q) {
            const Token &sk = m_tokens.at(q);
            if (sk.line != lineStart)
                break;
            if (sk.code == -1 && sk.lexeme.size() == 1 && !lexemeIsUnclosedStringError(sk.lexeme))
                reportTokenOnce(sk, QStringLiteral("Лексическая ошибка"));
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

    auto hasExpectedLaterOnLine = [&](int expectedCode) -> bool {
        for (int q = m_pos + 1; q < m_tokens.size(); ++q) {
            const Token &tk = m_tokens.at(q);
            if (tk.line != lineStart)
                break;
            if (tk.code == expectedCode)
                return true;
        }
        return false;
    };

    auto findFirstIdentifierBefore = [&](int stopCode) -> const Token* {
        for (int q = m_pos + 1; q < m_tokens.size(); ++q) {
            const Token &tk = m_tokens.at(q);
            if (tk.line != lineStart)
                break;
            if (tk.code == stopCode)
                break;
            if (tk.code == 3)
                return &m_tokens[q];
        }
        return nullptr;
    };

    auto findFirstIdentifierAfterGapBefore = [&](int stopCode) -> const Token* {
        int prevEnd = -1000000;
        for (int q = 0; q < m_tokens.size(); ++q) {
            const Token &tk = m_tokens.at(q);
            if (tk.line != lineStart)
                continue;
            if (q < m_pos)
                continue;
            if (tk.code == stopCode)
                break;
            if (tk.code == 3) {
                if (tk.startCol > prevEnd + 1)
                    return &m_tokens[q];
            }
            prevEnd = tk.endCol;
        }
        return nullptr;
    };

    bool unclosedStringOnLine = false;
    bool constMatchedOnLine = false;

    int i = 0;
    while (i < expectedCodes.size()) {
        int expected = expectedCodes[i];

        if (isAtEnd() || currentToken().line != lineStart) {
            Token lastT = (m_pos > 0) ? m_tokens[m_pos - 1] : Token();
            if (!stepReported[i] && shouldReportStep(i)) {
                addOrMergeErrorAt(QString(), lineStart, lastT.endCol + 1, descriptions[i]);
                stepReported[i] = true;
            }
            i++;
            continue;
        }

        Token t = currentToken();

        // Double '=' (==): extra '=' before string literal.
        if (i == 5 && expected == 7 && t.code == 6) {
            reportTokenOnce(t, QStringLiteral("Лишний символ"));
            m_pos++;
            continue;
        }

        // Два идентификатора подряд до ':' — имя переменной должно быть одно; второй лишний (пробел в имени).
        if (i == 2 && expected == 5 && t.code == 3 && m_pos > 0) {
            const Token &prev = m_tokens.at(m_pos - 1);
            if (prev.line == lineStart && prev.code == 3 && t.startCol > prev.endCol + 1) {
                const int spaceCol = prev.endCol + 1;
                suppressedLexCols.insert(spaceCol);
                addOrMergeErrorAt(t.lexeme, t.line, t.startCol, QStringLiteral("Лишний идентификатор"));
                addOrMergeErrorAt(t.lexeme, t.line, t.startCol, QStringLiteral("Лексическая ошибка"));
                tokenReportedCols.insert(t.startCol);
                m_pos++;
                continue;
            }
        }

        // FSM rule: variable name is checked strictly AFTER the (possibly wrong/broken) Const word.
        // If Const is wrong/broken, we still "consume" its fragments and then expect an identifier next.
        if (i == 0 && expected == 1 && t.code != 1 && t.code != -1) {
            // Report expected Const at the first fragment.
            reportStepOnce(0, t);

            // Merge lexical errors inside the broken keyword (e.g. con#st) into that same message.
            const QList<int> brokenCols = collectBrokenWordLexColsFrom(m_pos);
            if (!brokenCols.isEmpty())
                mergeLexIntoStepAt(0, t, t.startCol, brokenCols);

            // Advance past the whole wrong/broken keyword "word".
            int afterKwPos = m_pos + 1;
            if (!brokenCols.isEmpty()) {
                // Skip contiguous id/invalid/id chain that forms the broken keyword.
                int curEnd = t.endCol;
                int q = m_pos + 1;
                while (q + 1 < m_tokens.size()) {
                    const Token &inv = m_tokens.at(q);
                    const Token &b = m_tokens.at(q + 1);
                    if (inv.line != lineStart || b.line != lineStart)
                        break;
                    if (!(inv.code == -1 && inv.lexeme.size() == 1 && !lexemeIsUnclosedStringError(inv.lexeme)))
                        break;
                    if (b.code != 3)
                        break;
                    if (inv.startCol != curEnd + 1 || b.startCol != inv.endCol + 1)
                        break;
                    curEnd = b.endCol;
                    q += 2;
                    afterKwPos = q;
                }
            }

            m_pos = afterKwPos;
            i = 1; // now expect variable name strictly after Const
            continue;
        }

        if (expected == 2 && t.code == 5) {
            reportTokenOnce(t, QStringLiteral("Лишняя лексема"));
            m_pos++;
            continue;
        }

        // Broken identifier word like "Str$ka" (between Const and ':') should become:
        // "Ожидалось имя переменной (Лексическая ошибка)".
        if (i == 1 && expected == 3 && t.code == 3) {
            const QList<int> brokenCols = collectBrokenWordLexColsFrom(m_pos);
            if (!brokenCols.isEmpty()) {
                mergeLexIntoStepAt(i, t, t.startCol, brokenCols);

                const int colonIdx = nextOnLine(m_pos + 1, 5);
                if (colonIdx >= 0) {
                    m_pos = colonIdx;
                    i = 2; // expect ':'
                } else {
                    // consume this fragment to avoid infinite loop
                    m_pos++;
                    i = 2;
                }
                continue;
            }
        }

        // Broken type word like "st@ring" should become: "Ожидалось 'string' (Лексическая ошибка)".
        if (i == 3 && expected == 2 && t.code == 3) {
            const QList<int> brokenCols = collectBrokenWordLexColsFrom(m_pos);
            if (!brokenCols.isEmpty()) {
                mergeLexIntoStepAt(i, t, t.startCol, brokenCols);
                const int eqIdx = nextOnLine(m_pos + 1, 6);
                if (eqIdx >= 0) {
                    m_pos = eqIdx;
                    i = 4; // expect '='
                } else {
                    // consume this identifier fragment to avoid infinite loop
                    m_pos++;
                    i = 4;
                }
                continue;
            }
        }

        if (t.code == -1) {
            if (lexemeIsUnclosedStringError(t.lexeme)) {
                unclosedStringOnLine = true;
                reportTokenOnce(t, QStringLiteral("Не закрытая строка"));
                m_pos++;
                // Шаг с `;` — последний в цепочке (индекс 6).
                i = 6;
                continue;
            }

            reportTokenOnce(t, QStringLiteral("Лексическая ошибка"));
            if (!hasExpectedLaterOnLine(expected)) {
                reportStepOnce(i, t);
            } else {
                if (expected == 5) {
                    const Token *idTok = findFirstIdentifierAfterGapBefore(5);
                    if (!idTok)
                        idTok = findFirstIdentifierBefore(5);
                    if (idTok && (!constMatchedOnLine || idTok->startCol > t.startCol)) {
                        reportStepOnce(1, *idTok);
                    } else {
                        reportStepOnce(1, t);
                    }
                }
            }
            m_pos++;
            continue;
        }

        if (t.code == expected) {
            if (i == 0 && expected == 1)
                constMatchedOnLine = true;
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

        if (hasExpectedLaterOnLine(expected)) {
            reportTokenOnce(t, QStringLiteral("Лишняя лексема"));
            m_pos++;
            continue;
        }

        if (expected == 8 && t.code != 8) {
            reportTokenOnce(t, QStringLiteral("Лишняя лексема"));
            m_pos++;
            continue;
        }

        reportStepOnce(i, t);
        m_pos++;
    }

    while (!isAtEnd() && currentToken().line == lineStart) {
        Token t = currentToken();
        if (!unclosedStringOnLine) {
            if (t.code == -1) {
                if (!lexemeIsUnclosedStringError(t.lexeme)) {
                    reportTokenOnce(t, QStringLiteral("Лексическая ошибка"));
                }
            } else {
                reportTokenOnce(t, QStringLiteral("Лишняя лексема"));
            }
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
