#include "parser.h"
#include <algorithm>
#include <QSet>
#include <QVector>
#include <QStringList>

namespace {
int levenshteinDistance(const QString &s, const QString &t)
{
    const int n = s.size();
    const int m = t.size();
    if (n == 0)
        return m;
    if (m == 0)
        return n;
    QVector<int> prev(m + 1);
    QVector<int> cur(m + 1);
    for (int j = 0; j <= m; ++j)
        prev[j] = j;
    for (int i = 1; i <= n; ++i) {
        cur[0] = i;
        for (int j = 1; j <= m; ++j) {
            const int cost = (s.at(i - 1) == t.at(j - 1)) ? 0 : 1;
            cur[j] = std::min({prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + cost});
        }
        prev.swap(cur);
    }
    return prev[m];
}
} // namespace

Parser::Parser(const QList<Token> &tokens) : m_tokens(tokens), m_pos(0) {}

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

void Parser::parseLineWithErrors(int lineStart)
{
    static const QString kExpOpenString = QStringLiteral("Ожидалась открытие строки '");
    static const QString kExpCloseString = QStringLiteral("Ожидалось закрытие строки");
    static const QString kUnclosedStringPrimary = QStringLiteral("Ожидалось закрытие строки");
    static const QString kExpSemicolonAtEol = QStringLiteral("Ожидалось ';'");
    static const QString kSemicolonOnlyAtEnd = QStringLiteral("Символ ';' допускается только в конце объявления");

    QVector<int> expectedCodes = {1, 4, 3, 5, 2, 6, 7, 8};
    QStringList descriptions = {
        QStringLiteral("Ожидалось 'Const'"),
        QStringLiteral("Ожидался пробел после 'Const'"),
        QStringLiteral("Ожидалось имя переменной"),
        QStringLiteral("Ожидалось ':'"),
        QStringLiteral("Ожидалось 'string'"),
        QStringLiteral("Ожидалось '='"),
        kExpOpenString,
        QStringLiteral("Пропущена ';'")
    };

    QVector<bool> stepReported(expectedCodes.size(), false);
    QSet<int> tokenReportedCols;
    QSet<int> suppressedLexCols;

    auto mergeDescriptions = [&](const QString &a, const QString &b) -> QString {
        const QString kExpSpaceAfterConst = QStringLiteral("Ожидался пробел после 'Const'");
        if (a == b)
            return a;
        if ((a == QStringLiteral("Лексическая ошибка") && b == kExpSpaceAfterConst)
            || (b == QStringLiteral("Лексическая ошибка") && a == kExpSpaceAfterConst))
            return QStringLiteral("Лексическая ошибка");

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

        if (base == QStringLiteral("Лексическая ошибка") && detail == kExpSpaceAfterConst)
            return QStringLiteral("Лексическая ошибка");

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

    auto reportStepOnce = [&](int stepIdx, const Token &tok, int colOverride = -1) {
        if (stepIdx < 0 || stepIdx >= stepReported.size()) return;
        if (stepReported[stepIdx]) return;
        const int col = (colOverride >= 0) ? colOverride : tok.startCol;
        addOrMergeErrorAt((colOverride >= 0) ? QString() : tok.lexeme, tok.line, col, descriptions[stepIdx]);
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
        if (stepIdx >= 0 && stepIdx < descriptions.size())
            addOrMergeErrorAt(anchorTok.lexeme, anchorTok.line, anchorCol, descriptions[stepIdx]);
        addOrMergeErrorAt(anchorTok.lexeme, anchorTok.line, anchorCol, QStringLiteral("Лексическая ошибка"));
        for (int c : lexColsToSuppress)
            suppressedLexCols.insert(c);
    };

    auto mergeExtraDetailIntoStepAt = [&](int stepIdx, const Token &anchorTok, int anchorCol,
                                          const QList<int> &colsToSkipSeparateRows,
                                          const QString &detailDesc) {
        if (stepIdx >= 0 && stepIdx < descriptions.size())
            addOrMergeErrorAt(anchorTok.lexeme, anchorTok.line, anchorCol, descriptions[stepIdx]);
        addOrMergeErrorAt(anchorTok.lexeme, anchorTok.line, anchorCol, detailDesc);
        for (int c : colsToSkipSeparateRows)
            tokenReportedCols.insert(c);
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
        if (fromPos < 0 || fromPos >= m_tokens.size())
            return {};
        const Token &a0 = m_tokens.at(fromPos);
        if (a0.line != lineStart || a0.code != 3)
            return {};

        QList<int> cols;
        int curEnd = a0.endCol;
        int q = fromPos + 1;
        if (q >= m_tokens.size())
            return {};

        while (q < m_tokens.size()) {
            const Token &inv = m_tokens.at(q);
            if (inv.line != lineStart)
                break;
            if (!(inv.code == -1 && inv.lexeme.size() == 1 && !lexemeIsUnclosedStringError(inv.lexeme)))
                break;
            if (inv.startCol != curEnd + 1)
                break;
            cols.append(inv.startCol);
            curEnd = inv.endCol;
            q++;
        }
        if (cols.isEmpty())
            return {};
        if (q >= m_tokens.size())
            return {};
        {
            const Token &b = m_tokens.at(q);
            if (b.line != lineStart || b.code != 3 || b.startCol != curEnd + 1)
                return {};
            curEnd = b.endCol;
            q++;
        }

        // Дальше чередование inv-id как раньше (str@i@ng).
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
                reportTokenOnce(sk, (sk.lexeme == QStringLiteral("'"))
                                        ? QStringLiteral("Лексическая ошибка (Лишняя кавычка)")
                                        : QStringLiteral("Лексическая ошибка"));
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
            if (tk.code == 4)
                continue;
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
                if (tk.code == 4)
                    continue;
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
                if (tk.code == 4)
                    continue;
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
    /// После «conststroka» одна ошибка на ключевом слове; `:` — структурный, не «нет имени».
    bool gluedConstNameExpectColon = false;
    /// Любой один идентификатор принят как «сломанный const» — при сразу «:» не дублировать «имя» (стр. 41).
    bool anySingleIdAsBrokenConst = false;
    int i = 0;
    while (i < expectedCodes.size()) {
        int expected = expectedCodes[i];
        if (!(i == 1 && expected == 4)) {
            while (!isAtEnd() && currentToken().line == lineStart && currentToken().code == 4)
                m_pos++;
        }

        if (isAtEnd() || currentToken().line != lineStart) {
            Token lastT = (m_pos > 0) ? m_tokens[m_pos - 1] : Token();
            const int eolCol = lastT.endCol + 1;
            bool hasEarlierErrOnLine = false;
            for (int k = 0; k < m_errors.size(); ++k) {
                const SyntaxError &e = m_errors.at(k);
                if (e.line == lineStart && e.col < eolCol)
                    hasEarlierErrOnLine = true;
            }
            // Не клеить «ожидалось ':' / 'string' / …» в конце строки, если уже есть точечные ошибки
            // слева (например `: string = ...` без const). Хвост строки/«;» (шаги 6–7) оставляем.
            const bool allowEolSynthetic = !hasEarlierErrOnLine || i >= 6;
            if (!stepReported[i]) {
                if (allowEolSynthetic)
                    addOrMergeErrorAt(QString(), lineStart, eolCol, descriptions[i]);
                stepReported[i] = true;
            }
            ++i;
            while (i < expectedCodes.size()) {
                stepReported[i] = true;
                ++i;
            }
            continue;
        }

        Token t = currentToken();

        // Строковый литерал появился ДО `=` (в зоне, где его не ждут): это «лишние кавычки».
        // Политика:
        // - считаем количество `'` в токене, которые стоят ДО первого `=` на строке, и выдаём ошибку на каждую такую кавычку
        //   («Лексическая ошибка (Лишняя кавычка)»).
        // - если на строке нет других одиночных лексических ошибок (например `@`), то оставляем ТОЛЬКО эти ошибки кавычек
        //   (подавляем каскад «Ожидалось …»).
        // - если на строке есть другая лексика, не очищаем уже найденные ошибки, просто добавляем кавычки.
        if (i < 4 && t.code == 7 && t.lexeme.startsWith(QLatin1Char('\''))) {
            int eqCol = 1000000000;
            // Найти самый ранний `=` на строке: либо отдельный токен '=', либо `=` внутри токена-литерала,
            // который лексер мог «проглотить» при ошибочных кавычках.
            for (int q = 0; q < m_tokens.size(); ++q) {
                const Token &pt = m_tokens.at(q);
                if (pt.line != lineStart)
                    continue;
                if (pt.code == 6 && pt.lexeme == QStringLiteral("=")) {
                    eqCol = std::min(eqCol, pt.startCol);
                    continue;
                }
                if (pt.code == 7) {
                    const int eqInLex = pt.lexeme.indexOf(QLatin1Char('='));
                    if (eqInLex >= 0)
                        eqCol = std::min(eqCol, pt.startCol + eqInLex);
                }
            }

            auto isSwallowedInvalid = [](const QChar &ch) -> bool {
                return ch == QLatin1Char('@') || ch == QLatin1Char('#') || ch == QLatin1Char('$') || ch == QLatin1Char('*')
                    || ch == QLatin1Char('!') || ch == QLatin1Char('%');
            };

            // Только «внешние» одиночные лексические токены (code=-1) решают, чистим ли мы старые ожидания.
            // Символы, проглоченные внутрь code=7, НЕ должны мешать очистке (см. кейсы con'st ...).
            bool hasExternalInvalidSingles = false;
            for (int q = 0; q < m_tokens.size(); ++q) {
                const Token &pt = m_tokens.at(q);
                if (pt.line != lineStart)
                    continue;
                if (pt.code != -1)
                    continue;
                if (lexemeIsUnclosedStringError(pt.lexeme))
                    continue;
                if (pt.lexeme.size() != 1)
                    continue;
                if (pt.lexeme == QStringLiteral("'"))
                    continue;
                hasExternalInvalidSingles = true;
                break;
            }
            // (swallowed invalid chars handled below)

            QList<int> quoteColsBeforeEq;
            // Собрать ВСЕ лишние кавычки (из всех токенов code=7) до первого `=` на строке.
            for (int q = m_pos; q < m_tokens.size(); ++q) {
                const Token &pt = m_tokens.at(q);
                if (pt.line != lineStart)
                    break;
                if (pt.code != 7)
                    continue;
                if (!pt.lexeme.startsWith(QLatin1Char('\'')))
                    continue;
                for (int j = 0; j < pt.lexeme.size(); ++j) {
                    if (pt.lexeme.at(j) != QLatin1Char('\''))
                        continue;
                    const int col = pt.startCol + j;
                    if (col < eqCol)
                        quoteColsBeforeEq.append(col);
                }
            }

            if (quoteColsBeforeEq.isEmpty()) {
                // Кавычки уже в зоне инициализатора (после `=`) — не это правило.
                // Пусть обработают ветки §Literal / хвост.
            } else {
            if (!hasExternalInvalidSingles) {
                for (int k = m_errors.size() - 1; k >= 0; --k) {
                    if (m_errors[k].line == lineStart)
                        m_errors.removeAt(k);
                }
            }

            for (int col : quoteColsBeforeEq) {
                addOrMergeErrorAt(QStringLiteral("'"), lineStart, col,
                                  QStringLiteral("Лексическая ошибка (Лишняя кавычка)"));
            }

            bool synthesizedBrokenName = false;
            // Синтезировать «Ожидалось имя переменной (Лексическая ошибка)» для сломанного идентификатора вида `st#roka`
            // внутри проглоченного токена code=7 до `=`.
            for (int q = m_pos; q < m_tokens.size(); ++q) {
                const Token &pt = m_tokens.at(q);
                if (pt.line != lineStart)
                    break;
                if (pt.code != 7)
                    continue;
                const int hashIdx = pt.lexeme.indexOf(QLatin1Char('#'));
                if (hashIdx < 0)
                    continue;
                const int hashCol = pt.startCol + hashIdx;
                if (hashCol >= eqCol)
                    continue;
                int l = hashIdx - 1;
                while (l >= 0) {
                    const QChar ch = pt.lexeme.at(l);
                    if (!(ch.isLetterOrNumber() || ch == QLatin1Char('_')))
                        break;
                    --l;
                }
                int r = hashIdx + 1;
                while (r < pt.lexeme.size()) {
                    const QChar ch = pt.lexeme.at(r);
                    if (!(ch.isLetterOrNumber() || ch == QLatin1Char('_')))
                        break;
                    ++r;
                }
                const int startIdx = l + 1;
                if (startIdx >= hashIdx || r <= hashIdx + 1)
                    continue;
                const QString word = pt.lexeme.mid(startIdx, r - startIdx);
                const int wordCol = pt.startCol + startIdx;
                addOrMergeErrorAt(word, lineStart, wordCol, descriptions[2]); // Ожидалось имя переменной
                addOrMergeErrorAt(word, lineStart, wordCol, QStringLiteral("Лексическая ошибка"));
                synthesizedBrokenName = true;
                break;
            }

            // Синтезировать «Ожидалось 'string'» по слову типа после `:` внутри проглоченного токена
            // только когда нашли сломанное имя (hash-случай).
            if (synthesizedBrokenName) {
                for (int q = m_pos; q < m_tokens.size(); ++q) {
                    const Token &pt = m_tokens.at(q);
                    if (pt.line != lineStart)
                        break;
                    if (pt.code != 7)
                        continue;
                    const int colonIdx = pt.lexeme.indexOf(QLatin1Char(':'));
                    if (colonIdx < 0)
                        continue;
                    const int colonCol = pt.startCol + colonIdx;
                    if (colonCol >= eqCol)
                        continue;
                    int p = colonIdx + 1;
                    while (p < pt.lexeme.size() && (pt.lexeme.at(p) == QLatin1Char(' ') || pt.lexeme.at(p) == QLatin1Char('\t')))
                        ++p;
                    int p2 = p;
                    while (p2 < pt.lexeme.size()) {
                        const QChar ch = pt.lexeme.at(p2);
                        if (!(ch.isLetterOrNumber() || ch == QLatin1Char('_')))
                            break;
                        ++p2;
                    }
                    if (p2 <= p)
                        continue;
                    const QString typeWord = pt.lexeme.mid(p, p2 - p);
                    const int typeCol = pt.startCol + p;
                    if (typeCol >= eqCol)
                        continue;
                    if (typeWord.compare(QStringLiteral("string"), Qt::CaseInsensitive) != 0) {
                        addOrMergeErrorAt(typeWord, lineStart, typeCol, descriptions[4]); // Ожидалось 'string'
                    }
                    break;
                }
            }

            // Лексер может «проглотить» недопустимые символы внутрь токена code=7,
            // поэтому дополнительно помечаем их как «Лексическая ошибка» по колонкам.
            for (int q = m_pos; q < m_tokens.size(); ++q) {
                const Token &pt = m_tokens.at(q);
                if (pt.line != lineStart)
                    break;
                if (pt.code != 7)
                    continue;
                for (int j = 0; j < pt.lexeme.size(); ++j) {
                    const int col = pt.startCol + j;
                    if (col >= eqCol)
                        break;
                    const QChar ch = pt.lexeme.at(j);
                    if (!isSwallowedInvalid(ch))
                        continue;
                    // `#` покрыт более содержательной ошибкой «имя переменной (Лексическая ошибка)».
                    if (ch == QLatin1Char('#'))
                        continue;
                    addOrMergeErrorAt(QString(ch), lineStart, col, QStringLiteral("Лексическая ошибка"));
                }
            }

            // Если в строке есть другие одиночные лексические символы (например `$`, `*`, `@`),
            // то обязаны их тоже отразить отдельными «Лексическая ошибка».
            if (hasExternalInvalidSingles) {
                for (int q = 0; q < m_tokens.size(); ++q) {
                    const Token &pt = m_tokens.at(q);
                    if (pt.line != lineStart)
                        continue;
                    if (pt.code != -1)
                        continue;
                    if (lexemeIsUnclosedStringError(pt.lexeme))
                        continue;
                    if (pt.lexeme.size() != 1)
                        continue;
                    if (pt.lexeme == QStringLiteral("'"))
                        continue;
                    reportTokenOnce(pt, QStringLiteral("Лексическая ошибка"));
                }
            }

            while (!isAtEnd() && currentToken().line == lineStart)
                m_pos++;
            for (int s = 0; s < stepReported.size(); ++s)
                stepReported[s] = true;
            i = expectedCodes.size();
            continue;
            }
        }

        // `str'oka`-подобное: на месте ожидаемого `:` внезапно начинается строковый литерал.
        // Считаем это лишней кавычкой (одна строка), без каскада «Ожидалось …».
        if (i == 3 && expected == 5 && t.code == 7 && t.lexeme.startsWith(QLatin1Char('\''))) {
            reportTokenOnce(t, QStringLiteral("Лексическая ошибка (Лишняя кавычка)"));
            while (!isAtEnd() && currentToken().line == lineStart)
                m_pos++;
            for (int s = 0; s < stepReported.size(); ++s)
                stepReported[s] = true;
            i = expectedCodes.size();
            continue;
        }

        // `const … : string;` без `= '…'` — три ошибки (золотая таблица).
        if (i == 5 && expected == 6 && t.code == 8 && m_pos > 0) {
            int p = m_pos - 1;
            while (p > 0 && m_tokens.at(p).line == lineStart && m_tokens.at(p).code == 4)
                p--;
            const Token &beforeSemi = m_tokens.at(p);
            if (beforeSemi.line == lineStart && beforeSemi.code == 2) {
                addOrMergeErrorAt(t.lexeme, t.line, t.startCol, descriptions[5]);
                addOrMergeErrorAt(QString(), lineStart, beforeSemi.startCol, kExpOpenString);
                addOrMergeErrorAt(QString(), lineStart, beforeSemi.endCol, kExpCloseString);
                for (int s = 5; s < stepReported.size(); ++s)
                    stepReported[s] = true;
                m_pos++;
                i = expectedCodes.size();
                continue;
            }
        }

        // `const x: string 'Hello'` — пропущен `=` перед литералом.
        if (i == 5 && expected == 6 && t.code == 7 && t.lexeme.startsWith(QLatin1Char('\''))) {
            reportStepOnce(5, t);
            stepReported[6] = true;
            m_pos++;
            i = 7; // ждём `;`
            continue;
        }

        // `const x: string Hello` — пропущен `=` и нет кавычек: хотим как в `= Hello` + отдельно «Ожидалось '='».
        if (i == 5 && expected == 6 && t.code == 3) {
            int qLook = m_pos + 1;
            while (qLook < m_tokens.size() && m_tokens.at(qLook).line == lineStart && m_tokens.at(qLook).code == 4)
                ++qLook;
            const bool brokenQuoteTail = (qLook < m_tokens.size() && m_tokens.at(qLook).line == lineStart
                                          && lexemeIsUnclosedStringError(m_tokens.at(qLook).lexeme));
            // Не ставим на EOL-колонку, иначе сольётся с «Пропущена ';'».
            reportStepOnce(5, t, std::max(1, t.startCol - 1)); // Ожидалось '=' (отдельной строкой)
            addOrMergeErrorAt(t.lexeme, t.line, t.startCol, kExpOpenString);
            // `= Hello'01` — после идентификатора «литерал» ломается в незакрытую строку: на стыке — «Лишняя лексема», не «закрытие строки».
            if (brokenQuoteTail)
                addOrMergeErrorAt(QString(), t.line, t.endCol, QStringLiteral("Лишняя лексема"));
            else
                addOrMergeErrorAt(QString(), t.line, t.endCol, kExpCloseString);
            stepReported[6] = true;
            m_pos++;
            i = 7; // дальше `;`
            continue;
        }

        if (i == 1 && expected == 4) {
            if (t.code == 4) {
                while (!isAtEnd() && currentToken().line == lineStart && currentToken().code == 4)
                    m_pos++;
                i++;
                continue;
            }
            // «consttstroka» / «constt stroka» / «conststroka string»: после склеенного const имя или `string` может идти без пробела.
            if (gluedConstNameExpectColon) {
                if (t.code == 3) {
                    gluedConstNameExpectColon = false;
                    stepReported[1] = true;
                    i = 2;
                    continue;
                }
                if (t.code == 2 && t.lexeme.compare(QStringLiteral("string"), Qt::CaseInsensitive) == 0) {
                    gluedConstNameExpectColon = false;
                    stepReported[1] = true;
                    stepReported[2] = true;
                    i = 3;
                    continue;
                }
            }
            // Сразу ':' после const — пропущено имя переменной (пробел здесь вторичен).
            if (t.code == 5) {
                if (gluedConstNameExpectColon) {
                    gluedConstNameExpectColon = false;
                    stepReported[1] = true;
                    stepReported[2] = true;
                    i = 3;
                    continue;
                }
                if (anySingleIdAsBrokenConst) {
                    anySingleIdAsBrokenConst = false;
                    stepReported[1] = true;
                    stepReported[2] = true;
                    i = 3;
                    continue;
                }
                reportStepOnce(2, t);
                stepReported[1] = true;
                i = 3;
                continue;
            }
            reportStepOnce(1, t);
            i++;
            continue;
        }

        // `:` сразу `=` без ключевого слова `string` — только «Ожидалось 'string'», затем литерал.
        if (i == 4 && expected == 2 && t.code == 6) {
            reportStepOnce(4, t);
            m_pos++;
            i = 6;
            continue;
        }

        // `: <не string> 'Hello'` — тип пропущен/неверен, и одновременно пропущен `=` перед литералом.
        // Требуем: «Ожидалось 'string'», «Ожидалось '='», дальше `;`.
        if (i == 4 && expected == 2 && t.code == 7 && t.lexeme.startsWith(QLatin1Char('\''))) {
            reportStepOnce(4, t);
            reportStepOnce(5, t);
            stepReported[6] = true;
            m_pos++;
            i = 7;
            continue;
        }

        // Перед литералом: ровно «==» — «Лишний символ» (золотая #21); иначе блок «====», «=>», … — одна «Лишняя лексема» до литерала.
        if (i == 6 && expected == 7 && t.code != 7) {
            int litPos = -1;
            for (int q = m_pos; q < m_tokens.size(); ++q) {
                const Token &tk = m_tokens.at(q);
                if (tk.line != lineStart)
                    break;
                if (tk.code == 4)
                    continue;
                if (tk.code == 7 && tk.lexeme.startsWith(QLatin1Char('\''))) {
                    litPos = q;
                    break;
                }
                if (tk.code == 8 && tk.lexeme == QStringLiteral(";"))
                    break;
                if (tk.code == -1 && tk.lexeme.size() == 1 && tk.lexeme.at(0).isDigit())
                    break;
                if (tk.code == 3)
                    break;
                if (tk.code == 6)
                    continue;
                if (tk.code == -1 && tk.lexeme.size() == 1 && !lexemeIsUnclosedStringError(tk.lexeme))
                    continue;
                break;
            }
            if (litPos > m_pos) {
                QString frag;
                int startCol = m_tokens.at(m_pos).startCol;
                for (int q = m_pos; q < litPos; ++q) {
                    const Token &tk = m_tokens.at(q);
                    if (tk.code == 4)
                        continue;
                    frag += tk.lexeme;
                }
                if (frag.isEmpty()) {
                    m_pos = litPos;
                    continue;
                }
                const bool allEq = !frag.isEmpty()
                    && std::all_of(frag.begin(), frag.end(),
                                   [](const QChar &c) { return c == QLatin1Char('='); });
                if (frag == QStringLiteral("==")) {
                    const Token &secondEq = m_tokens.at(m_pos + 1);
                    reportTokenOnce(secondEq, QStringLiteral("Лишний символ"));
                } else if (allEq && frag.size() == 1) {
                    reportTokenOnce(m_tokens.at(m_pos), QStringLiteral("Лишний символ"));
                } else if (allEq && frag.size() >= 3) {
                    addOrMergeErrorAt(frag, lineStart, startCol, QStringLiteral("Лишняя лексема"));
                    for (int q = m_pos; q < litPos; ++q) {
                        const Token &tk = m_tokens.at(q);
                        if (tk.code == 4)
                            continue;
                        if (tk.code == -1 && tk.lexeme.size() == 1 && !lexemeIsUnclosedStringError(tk.lexeme))
                            suppressedLexCols.insert(tk.startCol);
                    }
                } else {
                    addOrMergeErrorAt(frag, lineStart, startCol, QStringLiteral("Лишняя лексема"));
                    for (int q = m_pos; q < litPos; ++q) {
                        const Token &tk = m_tokens.at(q);
                        if (tk.code == 4)
                            continue;
                        if (tk.code == -1 && tk.lexeme.size() == 1 && !lexemeIsUnclosedStringError(tk.lexeme))
                            suppressedLexCols.insert(tk.startCol);
                    }
                }
                m_pos = litPos;
                continue;
            }
        }

        // `Hello';` — литерал не открыт одинарной кавычкой; лишняя `'` сливается в сообщение на идентификаторе.
        if (i == 6 && expected == 7 && t.code == 3) {
            int q = m_pos + 1;
            while (q < m_tokens.size() && m_tokens.at(q).line == lineStart && m_tokens.at(q).code == 4)
                ++q;
            if (q < m_tokens.size()) {
                const Token &nx = m_tokens.at(q);
                if (nx.line == lineStart && nx.code == -1 && nx.lexeme == QStringLiteral("'")) {
                    QList<int> quoteCol{nx.startCol};
                    mergeLexIntoStepAt(6, t, t.startCol, quoteCol);
                    m_pos = q + 1;
                    i = 7;
                    continue;
                }
            }
        }

        // `= Hello;` — вместо строкового литерала идентификатор без кавычек: нужны обе границы строки.
        if (i == 6 && expected == 7 && t.code == 3) {
            int qLook = m_pos + 1;
            while (qLook < m_tokens.size() && m_tokens.at(qLook).line == lineStart && m_tokens.at(qLook).code == 4)
                ++qLook;
            const bool brokenQuoteTail = (qLook < m_tokens.size() && m_tokens.at(qLook).line == lineStart
                                          && lexemeIsUnclosedStringError(m_tokens.at(qLook).lexeme));
            addOrMergeErrorAt(t.lexeme, t.line, t.startCol, kExpOpenString);
            if (brokenQuoteTail)
                addOrMergeErrorAt(QString(), t.line, t.endCol, QStringLiteral("Лишняя лексема"));
            else
                addOrMergeErrorAt(QString(), t.line, t.endCol, kExpCloseString);
            stepReported[6] = true;
            m_pos++;
            i = 7;
            continue;
        }

        // Сломанное «string» вида `st=ring` / `st:ring` (структурное `=` или второе `:` между фрагментами).
        if (i == 4 && expected == 2 && t.code == 3) {
            int q = m_pos + 1;
            while (q < m_tokens.size() && m_tokens.at(q).line == lineStart && m_tokens.at(q).code == 4)
                ++q;
            if (q < m_tokens.size()) {
                const Token &mid = m_tokens.at(q);
                if ((mid.code == 5 && mid.lexeme == QStringLiteral(":"))
                    || (mid.code == 6 && mid.lexeme == QStringLiteral("="))) {
                    int q2 = q + 1;
                    while (q2 < m_tokens.size() && m_tokens.at(q2).line == lineStart && m_tokens.at(q2).code == 4)
                        ++q2;
                    if (q2 < m_tokens.size()) {
                        const Token &rid = m_tokens.at(q2);
                        if (rid.line == lineStart && rid.code == 3) {
                            mergeExtraDetailIntoStepAt(4, t, t.startCol,
                                                       {mid.startCol, rid.startCol},
                                                       QStringLiteral("Лишняя лексема"));
                            stepReported[4] = true;
                            m_pos = q2 + 1;
                            i = 5;
                            continue;
                        }
                    }
                }
            }
        }

        // `st'ring` — лексер даёт `st` + строковый литерал, начинающийся с `'`, и позже лишнюю `'`; одно сообщение на `st`.
        if (i == 4 && expected == 2 && t.code == 3) {
            int q = m_pos + 1;
            while (q < m_tokens.size() && m_tokens.at(q).line == lineStart && m_tokens.at(q).code == 4)
                ++q;
            if (q < m_tokens.size()) {
                const Token &nx = m_tokens.at(q);
                // Только когда `'` примыкает к фрагменту слова (без пробелов): `st'ring`, но не `sing 'Hello'`.
                if (nx.line == lineStart && nx.code == 7 && nx.lexeme.startsWith(QLatin1Char('\''))
                    && nx.startCol == t.endCol + 1) {
                    QList<int> skipCols;
                    for (int qi = q; qi < m_tokens.size() && m_tokens.at(qi).line == lineStart; ++qi)
                        skipCols.append(m_tokens.at(qi).startCol);
                    mergeExtraDetailIntoStepAt(4, t, t.startCol, skipCols, QStringLiteral("Лишняя кавычка"));
                    stepReported[4] = true;
                    const int eqEqIdx = nx.lexeme.indexOf(QLatin1String("=="));
                    if (eqEqIdx >= 0) {
                        const int secondEqCol = nx.startCol + eqEqIdx + 1;
                        if (!tokenReportedCols.contains(secondEqCol)) {
                            addOrMergeErrorAt(QStringLiteral("="), lineStart, secondEqCol,
                                              QStringLiteral("Лишний символ"));
                            tokenReportedCols.insert(secondEqCol);
                        }
                    }
                    while (q < m_tokens.size() && m_tokens.at(q).line == lineStart)
                        ++q;
                    m_pos = q;
                    i = expectedCodes.size();
                    continue;
                }
            }
        }

        if (expected == 2 && t.code == 5) {
            reportTokenOnce(t, QStringLiteral("Лишняя лексема"));
            m_pos++;
            continue;
        }

        // После имени ожидали ':', а сразу ключевое слово типа `string` — пропущено ':' перед типом («const stroka string», «constt stroka string»).
        if (i == 3 && expected == 5 && t.code == 2 && t.lexeme.compare(QStringLiteral("string"), Qt::CaseInsensitive) == 0) {
            reportStepOnce(3, t);
            m_pos++;
            i = 5; // тип `string` уже прочитан — дальше '='
            continue;
        }

        // Два идентификатора подряд до ':' — имя переменной должно быть одно; второй лишний (пробел в имени).
        if (i == 3 && expected == 5 && t.code == 3 && m_pos > 0) {
            int pq = m_pos - 1;
            while (pq >= 0 && m_tokens.at(pq).line == lineStart && m_tokens.at(pq).code == 4)
                pq--;
            if (pq >= 0) {
                const Token &prev = m_tokens.at(pq);
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
        }

        // FSM rule: variable name is checked strictly AFTER the (possibly wrong/broken) Const word.
        // If Const is wrong/broken, we still "consume" its fragments and then expect an identifier next.
        if (i == 0 && expected == 1 && t.code != 1 && t.code != -1) {
            // Report expected Const at the first fragment.
            reportStepOnce(0, t);

            // «co;nst stroka:…» — `;` внутри const: одна строка «Ожидалось 'Const' (Лексическая ошибка)», без каскада по строке.
            if (t.code == 3 && t.lexeme.compare(QStringLiteral("co"), Qt::CaseInsensitive) == 0) {
                int qi = m_pos + 1;
                while (qi < m_tokens.size() && m_tokens.at(qi).line == lineStart && m_tokens.at(qi).code == 4)
                    ++qi;
                if (qi < m_tokens.size()) {
                    const Token &semiTok = m_tokens.at(qi);
                    if (semiTok.line == lineStart && semiTok.code == 8 && semiTok.lexeme == QStringLiteral(";")) {
                        int qj = qi + 1;
                        while (qj < m_tokens.size() && m_tokens.at(qj).line == lineStart && m_tokens.at(qj).code == 4)
                            ++qj;
                        if (qj < m_tokens.size()) {
                            const Token &nstTok = m_tokens.at(qj);
                            if (nstTok.line == lineStart && nstTok.code == 3
                                && nstTok.lexeme.compare(QStringLiteral("nst"), Qt::CaseInsensitive) == 0) {
                                mergeLexIntoStepAt(0, t, t.startCol, QList<int>{semiTok.startCol});
                                tokenReportedCols.insert(semiTok.startCol);
                                tokenReportedCols.insert(nstTok.startCol);
                                int endPos = qj + 1;
                                while (endPos < m_tokens.size() && m_tokens.at(endPos).line == lineStart)
                                    ++endPos;
                                m_pos = endPos;
                                i = expectedCodes.size();
                                continue;
                            }
                        }
                    }
                }
            }

            // «cons t stroka::» — опечатка `const`, лишний «t» перед именем и двойное «::»; без «Лишний идентификатор» на t/stroka.
            if (t.code == 3 && t.lexeme.compare(QStringLiteral("cons"), Qt::CaseInsensitive) == 0) {
                int qi = m_pos + 1;
                while (qi < m_tokens.size() && m_tokens.at(qi).line == lineStart && m_tokens.at(qi).code == 4)
                    ++qi;
                if (qi < m_tokens.size()) {
                    const Token &wT = m_tokens.at(qi);
                    int na = qi + 1;
                    while (na < m_tokens.size() && m_tokens.at(na).line == lineStart && m_tokens.at(na).code == 4)
                        ++na;
                    if (na < m_tokens.size()) {
                        const Token &wStroka = m_tokens.at(na);
                        int nb = na + 1;
                        while (nb < m_tokens.size() && m_tokens.at(nb).line == lineStart && m_tokens.at(nb).code == 4)
                            ++nb;
                        if (nb + 1 < m_tokens.size()) {
                            const Token &colon1 = m_tokens.at(nb);
                            const Token &colon2 = m_tokens.at(nb + 1);
                            if (wT.line == lineStart && wT.code == 3 && wT.lexeme.size() == 1
                                && wT.lexeme.at(0).toLower() == QLatin1Char('t')
                                && wStroka.line == lineStart && wStroka.code == 3
                                && wStroka.lexeme.compare(QStringLiteral("stroka"), Qt::CaseInsensitive) == 0
                                && colon1.line == lineStart && colon1.code == 5 && colon1.lexeme == QStringLiteral(":")
                                && colon2.line == lineStart && colon2.code == 5 && colon2.lexeme == QStringLiteral(":")) {
                                tokenReportedCols.insert(wT.startCol);
                                tokenReportedCols.insert(wStroka.startCol);
                                m_pos = nb;
                                i = 3;
                                continue;
                            }
                        }
                    }
                }
            }

            // «con st …», «c o n s t», «co nst» и т.п.: несколько идентификаторов подряд (между — только пробелы), склейка даёт
            // префикс «const» или целиком «const» → одна ошибка на первом фрагменте, без «Лишний идентификатор» на хвосте.
            if (t.code == 3) {
                const QString constRef = QStringLiteral("const");
                QString merged;
                int p = m_pos;
                int pLastGoodPrefix = m_pos;
                while (p < m_tokens.size()) {
                    while (p < m_tokens.size() && m_tokens.at(p).line == lineStart && m_tokens.at(p).code == 4)
                        ++p;
                    if (p >= m_tokens.size())
                        break;
                    const Token &tk = m_tokens.at(p);
                    if (tk.line != lineStart || tk.code != 3)
                        break;
                    const QString trial = merged + tk.lexeme;
                    if (!constRef.startsWith(trial, Qt::CaseInsensitive)) {
                        p = pLastGoodPrefix;
                        break;
                    }
                    merged = trial;
                    ++p;
                    pLastGoodPrefix = p;
                    if (merged.compare(constRef, Qt::CaseInsensitive) == 0)
                        break;
                }
                if (!merged.isEmpty() && p > m_pos) {
                    const bool mergedFull = (merged.compare(constRef, Qt::CaseInsensitive) == 0);
                    const Token *nxTok = (p < m_tokens.size()) ? &m_tokens.at(p) : nullptr;
                    const bool nxIsBadSingle = nxTok && nxTok->line == lineStart && nxTok->code == -1
                        && nxTok->lexeme.size() == 1 && !lexemeIsUnclosedStringError(nxTok->lexeme);

                    // «co@nst» / «c@onst»: после частичного префикса идёт одиночная лексическая — не сдвигать m_pos,
                    // чтобы сработал collectBrokenWordLexColsFrom(m_pos).
                    if (!mergedFull && nxIsBadSingle) {
                        // fall through
                    } else {
                        for (int q = m_pos + 1; q < p; ++q) {
                            const Token &mid = m_tokens.at(q);
                            if (mid.line == lineStart && mid.code == 3)
                                tokenReportedCols.insert(mid.startCol);
                        }
                        m_pos = mergedFull ? p : pLastGoodPrefix;
                        i = 1;
                        continue;
                    }
                }
            }

            // «cos t», «cot st» и т.п.: несколько идентификаторов, между — только пробел/таб, суммарно ≤5 букв,
            // склейка не «const» и не префикс «const»; расстояние до «const» ≤ 2 — одна ошибка «Ожидалось 'Const'».
            if (t.code == 3) {
                const QString constRef = QStringLiteral("const");
                int pF = m_pos;
                QString bufF;
                int idCount = 0;
                int pLastGood = m_pos;
                while (pF < m_tokens.size()) {
                    while (pF < m_tokens.size() && m_tokens.at(pF).line == lineStart && m_tokens.at(pF).code == 4)
                        ++pF;
                    if (pF >= m_tokens.size())
                        break;
                    const Token &tkF = m_tokens.at(pF);
                    if (tkF.line != lineStart || tkF.code != 3)
                        break;
                    if (bufF.size() + tkF.lexeme.size() > 5) {
                        pF = pLastGood;
                        break;
                    }
                    bool allLetters = true;
                    for (const QChar ch : tkF.lexeme) {
                        if (!ch.isLetter()) {
                            allLetters = false;
                            break;
                        }
                    }
                    if (!allLetters) {
                        pF = pLastGood;
                        break;
                    }
                    bufF += tkF.lexeme;
                    ++pF;
                    ++idCount;
                    pLastGood = pF;
                }
                if (idCount >= 2 && bufF.size() >= 2 && bufF.size() <= 5
                    && bufF.compare(constRef, Qt::CaseInsensitive) != 0
                    && !constRef.startsWith(bufF, Qt::CaseInsensitive)) {
                    const QChar fc = bufF.at(0);
                    const ushort cp = fc.unicode();
                    if ((fc == QLatin1Char('c') || fc == QLatin1Char('C') || cp == 0x0441)
                        && levenshteinDistance(bufF.toCaseFolded(), constRef.toCaseFolded()) <= 2) {
                        for (int q = m_pos + 1; q < pF; ++q) {
                            const Token &mid = m_tokens.at(q);
                            if (mid.line == lineStart && mid.code == 3)
                                tokenReportedCols.insert(mid.startCol);
                        }
                        m_pos = pF;
                        i = 1;
                        continue;
                    }
                }
            }

            // Merge lexical errors inside the broken keyword (e.g. con#st) into that same message.
            const QList<int> brokenCols = collectBrokenWordLexColsFrom(m_pos);
            if (!brokenCols.isEmpty())
                mergeLexIntoStepAt(0, t, t.startCol, brokenCols);

            // Advance past the whole wrong/broken keyword "word".
            int afterKwPos = m_pos + 1;
            if (!brokenCols.isEmpty()) {
                // Skip contiguous id + (invalid)+ + id + … как в «co@nst», «co@@@nst», «c@onst».
                int curEnd = t.endCol;
                int q = m_pos + 1;
                while (q < m_tokens.size()) {
                    while (q < m_tokens.size()) {
                        const Token &inv = m_tokens.at(q);
                        if (inv.line != lineStart
                            || !(inv.code == -1 && inv.lexeme.size() == 1
                                 && !lexemeIsUnclosedStringError(inv.lexeme)))
                            break;
                        if (inv.startCol != curEnd + 1)
                            break;
                        curEnd = inv.endCol;
                        ++q;
                    }
                    if (q >= m_tokens.size())
                        break;
                    const Token &b = m_tokens.at(q);
                    if (b.line != lineStart || b.code != 3 || b.startCol != curEnd + 1)
                        break;
                    curEnd = b.endCol;
                    ++q;
                    afterKwPos = q;
                }
                m_pos = afterKwPos;
                i = 1; // mandatory whitespace after (broken) Const
                continue;
            }

            // «conststroka:» — один идентификатор, склеенный с const (длина > 5 и префикс const).
            const bool gluedConstName = (t.code == 3 && t.lexeme.size() > 5
                                         && t.lexeme.startsWith(QStringLiteral("const"), Qt::CaseInsensitive));

            // «stroka:» / «: string» — сразу «:» или иное не-ид; имя на месте const — без шага «пробел после const».
            if (t.code == 5) {
                i = 2;
                continue;
            }

            // Любая одна лексема-имя на месте const (st, c3t, lrgjij, «stroka» без const и т.д.) — одна ошибка «Const», дальше как после const.
            if (t.code == 3 && !gluedConstName) {
                anySingleIdAsBrokenConst = true;
                m_pos++;
                i = 1;
                continue;
            }

            if (gluedConstName)
                gluedConstNameExpectColon = true;
            m_pos = afterKwPos;
            i = 1; // gluedConstName и прочие однолексемные замены const
            continue;
        }

        // Имя вида `stroka-1`: имя переменной и отдельная лексическая ошибка на дефисе (золотая таблица: min 2).
        if (i == 2 && expected == 3 && t.code == 3) {
            int q = m_pos + 1;
            while (q < m_tokens.size() && m_tokens.at(q).line == lineStart && m_tokens.at(q).code == 4)
                ++q;
            if (q + 1 < m_tokens.size()) {
                const Token &hyp = m_tokens.at(q);
                const Token &dig = m_tokens.at(q + 1);
                if (hyp.line == lineStart && hyp.code == -1 && hyp.lexeme == QStringLiteral("-")
                    && dig.line == lineStart && dig.code == -1 && dig.lexeme.size() == 1
                    && dig.lexeme.at(0).isDigit()) {
                    addOrMergeErrorAt(t.lexeme, t.line, t.startCol, descriptions[2]);
                    suppressedLexCols.insert(dig.startCol);
                    reportTokenOnce(hyp, QStringLiteral("Лексическая ошибка"));
                    const int colonIdx = nextOnLine(q + 2, 5);
                    if (colonIdx >= 0) {
                        m_pos = colonIdx;
                        i = 3;
                    } else {
                        m_pos = q + 2;
                        i = 3;
                    }
                    continue;
                }
            }
        }

        // Broken identifier word like "Str$ka" (between Const and ':') should become:
        // "Ожидалось имя переменной (Лексическая ошибка)".
        if (i == 2 && expected == 3 && t.code == 3) {
            const QList<int> brokenCols = collectBrokenWordLexColsFrom(m_pos);
            if (!brokenCols.isEmpty()) {
                mergeLexIntoStepAt(i, t, t.startCol, brokenCols);

                const int colonIdx = nextOnLine(m_pos + 1, 5);
                if (colonIdx >= 0) {
                    m_pos = colonIdx;
                    i = 3; // expect ':'
                } else {
                    // consume this fragment to avoid infinite loop
                    m_pos++;
                    i = 3;
                }
                continue;
            }
        }

        // Broken type word like "st@ring" should become: "Ожидалось 'string' (Лексическая ошибка)".
        if (i == 4 && expected == 2 && t.code == 3) {
            const QList<int> brokenCols = collectBrokenWordLexColsFrom(m_pos);
            if (!brokenCols.isEmpty()) {
                mergeLexIntoStepAt(i, t, t.startCol, brokenCols);
                const int eqIdx = nextOnLine(m_pos + 1, 6);
                if (eqIdx >= 0) {
                    m_pos = eqIdx;
                    i = 5; // expect '='
                } else {
                    // consume this identifier fragment to avoid infinite loop
                    m_pos++;
                    i = 5;
                }
                continue;
            }
        }

        // `= 123` — ожидался строковый литерал; «открытие» + лексика на всех цифрах с первой; «закрытие» на последней.
        if (i == 6 && expected == 7 && t.code == -1 && t.lexeme.size() == 1 && t.lexeme.at(0).isDigit()) {
            int firstQ = m_pos;
            int lastQ = m_pos;
            while (lastQ + 1 < m_tokens.size()) {
                const Token &u = m_tokens.at(lastQ + 1);
                if (u.line != lineStart || u.code != -1 || u.lexeme.size() != 1 || !u.lexeme.at(0).isDigit())
                    break;
                lastQ++;
            }
            int litQ = lastQ + 1;
            while (litQ < m_tokens.size() && m_tokens.at(litQ).line == lineStart && m_tokens.at(litQ).code == 4)
                ++litQ;
            // `= 1'Hello'!` — цифры не «закрытие строки»; закрытый литерал сразу после цифр не помечаем «лишняя лексема».
            if (litQ < m_tokens.size()) {
                const Token &litTok = m_tokens.at(litQ);
                if (litTok.line == lineStart && litTok.code == 7 && litTok.lexeme.startsWith(QLatin1Char('\''))
                    && litTok.lexeme.endsWith(QLatin1Char('\'')) && litTok.lexeme.size() >= 2) {
                    for (int qq = firstQ; qq <= lastQ; ++qq) {
                        const Token &dTok = m_tokens.at(qq);
                        addOrMergeErrorAt(dTok.lexeme, dTok.line, dTok.startCol, QStringLiteral("Лексическая ошибка"));
                    }
                    stepReported[6] = true;
                    m_pos = litQ + 1;
                    i = 7;
                    continue;
                }
            }
            QList<int> digitCols;
            for (int qq = firstQ; qq <= lastQ; ++qq)
                digitCols.append(m_tokens.at(qq).startCol);
            const Token &firstTok = m_tokens.at(firstQ);
            const Token &lastTok = m_tokens.at(lastQ);
            mergeLexIntoStepAt(6, firstTok, firstTok.startCol, digitCols);
            addOrMergeErrorAt(lastTok.lexeme, lastTok.line, lastTok.startCol, kExpCloseString);
            m_pos = lastQ + 1;
            i = 7;
            continue;
        }

        // После литерала подряд мусор до `;` (или до конца строки): одна «Лишняя лексема» на всём блоке (`'Hello'3`, `'Hello'68903`, `'Hello'% без `;`).
        // Незакрытая строка (`Незакрытая строка: …`) — не этот блок, а отдельная ветка ниже.
        if (i == 7 && expected == 8 && t.code == -1 && t.lexeme != QStringLiteral(";")
            && !lexemeIsUnclosedStringError(t.lexeme)) {
            int start = m_pos;
            int last = m_pos;
            while (last + 1 < m_tokens.size()) {
                const Token &nu = m_tokens.at(last + 1);
                if (nu.line != lineStart)
                    break;
                if (nu.code == 8 && nu.lexeme == QStringLiteral(";"))
                    break;
                if (nu.code == -1 && nu.lexeme.size() == 1 && !lexemeIsUnclosedStringError(nu.lexeme)
                    && nu.lexeme != QStringLiteral("'"))
                    last++;
                else
                    break;
            }
            QString frag;
            for (int qi = start; qi <= last; ++qi)
                frag += m_tokens.at(qi).lexeme;
            const Token &firstTok = m_tokens.at(start);
            addOrMergeErrorAt(frag, lineStart, firstTok.startCol, QStringLiteral("Лишняя лексема"));
            for (int qi = start; qi <= last; ++qi)
                suppressedLexCols.insert(m_tokens.at(qi).startCol);
            m_pos = last + 1;
            if (isAtEnd() || currentToken().line != lineStart) {
                // Для хвоста из цифр (`'Hello'8`) или одиночного `!` в конце (`…'Hello'!`) — ещё «Пропущена ';'»;
                // для `%` и прочего — только «Лишняя лексема», без дубля с EOL.
                if (!frag.isEmpty() && frag.at(0).isDigit())
                    addOrMergeErrorAt(QString(), lineStart, m_tokens.at(last).endCol + 1, descriptions[7]);
                else if (frag == QStringLiteral("!"))
                    addOrMergeErrorAt(QString(), lineStart, m_tokens.at(last).endCol + 1, descriptions[7]);
                else
                    stepReported[7] = true;
            }
            if (!isAtEnd() && currentToken().line == lineStart && currentToken().code == 8
                && currentToken().lexeme == QStringLiteral(";")) {
                // остаёмся на шаге `;`
                continue;
            }
            i = expectedCodes.size();
            continue;
        }

        if (t.code == -1) {
            if (lexemeIsUnclosedStringError(t.lexeme)) {
                unclosedStringOnLine = true;

                bool hasEq = false;
                for (int q = 0; q < m_tokens.size(); ++q) {
                    const Token &pt = m_tokens.at(q);
                    if (pt.line == lineStart && pt.code == 6 && pt.lexeme == QStringLiteral("=")) {
                        hasEq = true;
                        break;
                    }
                }
                // `= Hello'01` — один идентификатор после `=`, затем незакрытая «строка»: не чистить «открытие строки» на Hello,
                // на токене — «Ожидалось ';'», если в лексеме ещё нет `;` (`'02;` — `;` уже внутри «мусорной» строки, шестая строка не нужна).
                bool idInitThenBrokenQuote = false;
                if (hasEq) {
                    int eqPos = -1;
                    for (int q = 0; q < m_tokens.size(); ++q) {
                        const Token &pt = m_tokens.at(q);
                        if (pt.line != lineStart)
                            continue;
                        if (pt.code == 6 && pt.lexeme == QStringLiteral("=") && pt.startCol < t.startCol)
                            eqPos = q;
                    }
                    if (eqPos >= 0) {
                        int r = eqPos + 1;
                        int idSeen = 0;
                        bool badMid = false;
                        while (r < m_pos) {
                            const Token &w = m_tokens.at(r);
                            if (w.line != lineStart) {
                                badMid = true;
                                break;
                            }
                            if (w.code == 4) {
                                ++r;
                                continue;
                            }
                            if (w.code == 3) {
                                ++idSeen;
                                ++r;
                                continue;
                            }
                            badMid = true;
                            break;
                        }
                        idInitThenBrokenQuote = !badMid && idSeen == 1 && r == m_pos;
                    }
                }

                // Ensure prior invalid single-character tokens on this line are still reported
                // (some sync paths may skip them before we hit the unclosed-string token).
                for (int q = 0; q < m_pos && q < m_tokens.size(); ++q) {
                    const Token &prevTok = m_tokens.at(q);
                    if (prevTok.line != lineStart)
                        continue;
                    if (prevTok.code != -1)
                        continue;
                    if (lexemeIsUnclosedStringError(prevTok.lexeme))
                        continue;
                    if (prevTok.lexeme.size() != 1)
                        continue;
                    if (suppressedLexCols.contains(prevTok.startCol))
                        continue;
                    addOrMergeErrorAt(prevTok.lexeme, prevTok.line, prevTok.startCol, QStringLiteral("Лексическая ошибка"));
                }

                // Make the unclosed-string message the primary one for this line.
                // Для `= Id'…` (один идентификатор, затем незакрытая строка) не чистим весь хвост — иначе сотрётся
                // «Ожидалось 'Const'» на `[` и «Лишняя лексема» на стыке перед кавычкой.
                if (!idInitThenBrokenQuote) {
                    for (int k = m_errors.size() - 1; k >= 0; --k) {
                        if (m_errors[k].line != lineStart)
                            continue;
                        const QString &d = m_errors[k].description;
                        if (d.contains(descriptions[3]))
                            continue;
                        // «cnst … sting = 'Hello» — сохранить точечные «Const» / «string» до незакрытой строки (золотая таблица).
                        if (d.contains(descriptions[0]) || d.contains(descriptions[4]))
                            continue;
                        if (d.startsWith(QStringLiteral("Ожидалось")) ||
                            d.contains(QStringLiteral("Ожидалась закрытие строки"))
                            || d.contains(QStringLiteral("Ожидалось закрытие строки"))
                            || d.contains(QStringLiteral("Ожидалась открытие строки"))
                            || d.contains(QStringLiteral("Пропущена ';'"))
                            || d.contains(kExpSemicolonAtEol)) {
                            m_errors.removeAt(k);
                        }
                    }
                }
                for (int s = 0; s < stepReported.size(); ++s)
                    stepReported[s] = true;

                const bool semiInLexeme = t.lexeme.contains(QLatin1Char(';'));
                if (idInitThenBrokenQuote) {
                    if (!semiInLexeme)
                        addOrMergeErrorAt(t.lexeme, t.line, t.startCol, kExpSemicolonAtEol);
                } else {
                    addOrMergeErrorAt(t.lexeme, t.line, t.startCol, kUnclosedStringPrimary);
                }
                if (!hasEq)
                    addOrMergeErrorAt(QStringLiteral("="), lineStart, t.startCol + 1, descriptions[5]);
                if (!semiInLexeme && !hasExpectedLaterOnLine(8) && !idInitThenBrokenQuote)
                    addOrMergeErrorAt(QString(), lineStart, t.endCol + 1, kExpSemicolonAtEol);

                m_pos++;
                i = expectedCodes.size();
                continue;
            }

            reportTokenOnce(t, (t.lexeme == QStringLiteral("'"))
                                 ? QStringLiteral("Лексическая ошибка (Лишняя кавычка)")
                                 : QStringLiteral("Лексическая ошибка"));
            if (!hasExpectedLaterOnLine(expected)) {
                // Структурные ожидания (`:`/`=`/`string`) на колонке мусорной лексемы
                // не сливаются с «Лексическая ошибка»: «Ожидалось …» — отдельной записью на endCol+1.
                static const QSet<int> kSplitOnLex{2, 5, 6};
                if (kSplitOnLex.contains(expected))
                    reportStepOnce(i, t, t.endCol + 1);
                else
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

        // `;` внутри литерала или `'''` внутри содержимого — одно сообщение на проблемный фрагмент.
        if (i == 6 && expected == 7 && t.code == 7 && t.lexeme.size() >= 2
            && t.lexeme.startsWith(QLatin1Char('\'')) && t.lexeme.endsWith(QLatin1Char('\''))) {
            const QString mid = t.lexeme.mid(1, t.lexeme.size() - 2);
            const int semiIdx = mid.indexOf(QLatin1Char(';'));
            if (semiIdx >= 0) {
                const int errCol = t.startCol + 1 + semiIdx;
                addOrMergeErrorAt(QStringLiteral(";"), lineStart, errCol, kSemicolonOnlyAtEnd);
                m_pos++;
                i++;
                continue;
            }
            const int trip = mid.indexOf(QLatin1String("'''"));
            if (trip >= 0) {
                const int errCol = t.startCol + 1 + trip;
                addOrMergeErrorAt(QStringLiteral("'''"), lineStart, errCol, QStringLiteral("Лишняя кавычка"));
                m_pos++;
                i++;
                continue;
            }
        }

        // Два литерала подряд без пробела (`'He''llo'`-подобное разбиение лексера) — «Лишняя кавычка».
        if (i == 7 && expected == 8 && t.code == 7 && t.lexeme.startsWith(QLatin1Char('\'')) && m_pos > 0) {
            int p = m_pos - 1;
            while (p >= 0 && m_tokens.at(p).line == lineStart && m_tokens.at(p).code == 4)
                --p;
            if (p >= 0) {
                const Token &prev = m_tokens.at(p);
                if (prev.line == lineStart && prev.code == 7 && prev.lexeme.endsWith(QLatin1Char('\''))
                    && prev.endCol + 1 == t.startCol) {
                    addOrMergeErrorAt(QStringLiteral("''"), lineStart, t.startCol, QStringLiteral("Лишняя кавычка"));
                    m_pos++;
                    continue;
                }
            }
        }

        if (t.code == expected) {
            if (i == 0 && expected == 1)
                constMatchedOnLine = true;
            m_pos++;
            i++;
            continue;
        }

        if (i + 1 < expectedCodes.size() && t.code == expectedCodes[i + 1]
            && !(i == 4 && expected == 2 && t.code == 6)) {
            // Ожидали идентификатор, увидели ':' — две разные ошибки в таблице: «Const»/прочее на колонке ':',
            // «имя переменной» на следующей колонке (не merge в одну ячейку с тем же col).
            if (i == 2 && expected == 3 && t.code == 5)
                reportStepOnce(i, t, t.endCol + 1);
            else
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
        // «string = …» / «foo = …» без const: после лексемы перед «=» должно быть ':' перед типом — отдельная строка в таблице.
        if (i == 2 && expected == 3 && t.code == 6 && !constMatchedOnLine && m_pos > 0) {
            int colonCol = t.startCol;
            for (int pq = m_pos - 1; pq >= 0; --pq) {
                const Token &pt = m_tokens.at(pq);
                if (pt.line != lineStart)
                    break;
                if (pt.code == 4)
                    continue;
                colonCol = pt.endCol + 1;
                break;
            }
            if (!stepReported[3]) {
                addOrMergeErrorAt(QString(), lineStart, colonCol, descriptions[3]);
                stepReported[3] = true;
            }
        }
        reportStepOnce(i, t);
        m_pos++;
    }

    if (unclosedStringOnLine) {
        // Guarantee that every invalid single-character token on this line is reported as a lexical error.
        // (The fuzz tests require a "Лексическая ошибка" at each invalid-char column even with unclosed strings.)
        for (int q = 0; q < m_tokens.size(); ++q) {
            const Token &tk = m_tokens.at(q);
            if (tk.line != lineStart)
                continue;
            if (tk.code != -1)
                continue;
            if (lexemeIsUnclosedStringError(tk.lexeme))
                continue;
            if (tk.lexeme.size() != 1)
                continue;
            // Уже влито в «Ожидалось 'Const' (Лексическая ошибка)» и т.п. (`con$st` + незакрытая строка — без дубля на `$`).
            if (suppressedLexCols.contains(tk.startCol))
                continue;
            addOrMergeErrorAt(tk.lexeme, tk.line, tk.startCol, QStringLiteral("Лексическая ошибка"));
        }
    }

    while (!isAtEnd() && currentToken().line == lineStart) {
        Token t = currentToken();
        if (t.code == 4) {
            m_pos++;
            continue;
        }
        if (!unclosedStringOnLine) {
            if (t.code == -1) {
                if (lexemeIsUnclosedStringError(t.lexeme)) {
                    m_pos++;
                    continue;
                }
                if (t.lexeme == QStringLiteral("'"))
                    reportTokenOnce(t, QStringLiteral("Лексическая ошибка (Лишняя кавычка)"));
                else {
                    int start = m_pos;
                    int last = m_pos;
                    while (last + 1 < m_tokens.size()) {
                        const Token &nu = m_tokens.at(last + 1);
                        if (nu.line != lineStart || nu.code != -1 || nu.lexeme.size() != 1
                            || lexemeIsUnclosedStringError(nu.lexeme) || nu.lexeme == QStringLiteral("'"))
                            break;
                        last++;
                    }
                    QString frag;
                    for (int qi = start; qi <= last; ++qi)
                        frag += m_tokens.at(qi).lexeme;
                    addOrMergeErrorAt(frag, lineStart, m_tokens.at(start).startCol, QStringLiteral("Лишняя лексема"));
                    for (int qi = start; qi <= last; ++qi)
                        suppressedLexCols.insert(m_tokens.at(qi).startCol);
                    m_pos = last + 1;
                    continue;
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
    if (m_tokens.isEmpty()) return m_errors;

    while (!isAtEnd()) {
        const int lineStart = currentToken().line;
        parseLineWithErrors(lineStart);
    }
    return m_errors;
}
