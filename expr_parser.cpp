#include "expr_parser.h"
#include "expr_lexer.h"

#include <QStack>

ExprParser::ExprParser(const QList<Token> &tokens) : m_tokens(tokens) {}

const Token &ExprParser::currentToken() const {
    if (m_pos < m_tokens.size()) {
        return m_tokens.at(m_pos);
    }
    static Token eof(0, QStringLiteral("Конец"), QStringLiteral("EOF"), 0, 0, 0);
    if (!m_tokens.isEmpty()) {
        const Token &last = m_tokens.last();
        eof.line = last.line;
        eof.startCol = last.endCol + 1;
    }
    return eof;
}

bool ExprParser::isAtEnd() const {
    return m_pos >= m_tokens.size();
}

bool ExprParser::check(int code) const {
    return !isAtEnd() && currentToken().code == code;
}

bool ExprParser::advance() {
    if (!isAtEnd()) {
        ++m_pos;
        return true;
    }
    return false;
}

void ExprParser::syntaxError(const QString &expected) {
    const Token &t = currentToken();
    m_errors.append({t.lexeme.isEmpty() ? QStringLiteral("∅") : t.lexeme,
                     t.line,
                     t.startCol,
                     QStringLiteral("Ожидалось: %1").arg(expected)});
    m_hasFatalError = true;
}

QString ExprParser::newTemp() {
    return QStringLiteral("t%1").arg(++m_tempCounter);
}

void ExprParser::emitTetrad(const QString &op, const QString &arg1,
                            const QString &arg2, const QString &result) {
    m_tetrads.append({op, arg1, arg2, result});
}

ExprValue ExprParser::parseF() {
    if (check(ExprTokenCode::NUM)) {
        const Token t = currentToken();
        advance();
        return {t.lexeme, {t.lexeme}};
    }

    if (check(ExprTokenCode::ID)) {
        const Token t = currentToken();
        advance();
        return {t.lexeme, {t.lexeme}};
    }

    if (check(ExprTokenCode::LPAREN)) {
        advance();
        const ExprValue inner = parseE();
        if (!check(ExprTokenCode::RPAREN)) {
            syntaxError(QStringLiteral("')'"));
        } else {
            advance();
        }
        return inner;
    }

    syntaxError(QStringLiteral("num, id или '('"));
    return {QStringLiteral("_err"), {}};
}

ExprValue ExprParser::parseB(const ExprValue &inh) {
    if (check(ExprTokenCode::MULT)) {
        advance();
        const ExprValue right = parseF();
        const QString temp = newTemp();
        emitTetrad(QStringLiteral("*"), inh.place, right.place, temp);
        QStringList rpn = inh.rpn + right.rpn + QStringList{QStringLiteral("*")};
        return parseB({temp, rpn});
    }

    if (check(ExprTokenCode::DIV)) {
        advance();
        const ExprValue right = parseF();
        const QString temp = newTemp();
        emitTetrad(QStringLiteral("/"), inh.place, right.place, temp);
        QStringList rpn = inh.rpn + right.rpn + QStringList{QStringLiteral("/")};
        return parseB({temp, rpn});
    }

    return inh;
}

ExprValue ExprParser::parseT() {
    const ExprValue factor = parseF();
    return parseB(factor);
}

ExprValue ExprParser::parseA(const ExprValue &inh) {
    if (check(ExprTokenCode::PLUS)) {
        advance();
        const ExprValue right = parseT();
        const QString temp = newTemp();
        emitTetrad(QStringLiteral("+"), inh.place, right.place, temp);
        QStringList rpn = inh.rpn + right.rpn + QStringList{QStringLiteral("+")};
        return parseA({temp, rpn});
    }

    if (check(ExprTokenCode::MINUS)) {
        advance();
        const ExprValue right = parseT();
        const QString temp = newTemp();
        emitTetrad(QStringLiteral("-"), inh.place, right.place, temp);
        QStringList rpn = inh.rpn + right.rpn + QStringList{QStringLiteral("-")};
        return parseA({temp, rpn});
    }

    return inh;
}

ExprValue ExprParser::parseE() {
    const ExprValue term = parseT();
    return parseA(term);
}

bool ExprParser::evaluateRpn(const QStringList &rpn, qint64 &value, QString &message) {
    QStack<qint64> stack;

    for (const QString &lexeme : rpn) {
        if (lexeme == QStringLiteral("+") || lexeme == QStringLiteral("-")
            || lexeme == QStringLiteral("*") || lexeme == QStringLiteral("/")) {
            if (stack.size() < 2) {
                message = QStringLiteral("Некорректная ПОЛИЗ: недостаточно операндов");
                return false;
            }
            const qint64 b = stack.pop();
            const qint64 a = stack.pop();
            if (lexeme == QStringLiteral("+")) {
                stack.push(a + b);
            } else if (lexeme == QStringLiteral("-")) {
                stack.push(a - b);
            } else if (lexeme == QStringLiteral("*")) {
                stack.push(a * b);
            } else {
                if (b == 0) {
                    message = QStringLiteral("Деление на ноль");
                    return false;
                }
                stack.push(a / b);
            }
            continue;
        }

        bool ok = false;
        const qint64 num = lexeme.toLongLong(&ok);
        if (ok) {
            stack.push(num);
            continue;
        }

        message = QStringLiteral(
            "Вычисление только для целых чисел (найден идентификатор «%1»)").arg(lexeme);
        return false;
    }

    if (stack.size() != 1) {
        message = QStringLiteral("Некорректная ПОЛИЗ");
        return false;
    }

    value = stack.top();
    message.clear();
    return true;
}

ExprAnalysisResult ExprParser::parse() {
    ExprAnalysisResult result;
    m_pos = 0;
    m_tempCounter = 0;
    m_errors.clear();
    m_tetrads.clear();
    m_hasFatalError = false;

    if (m_tokens.isEmpty()) {
        result.syntaxErrors.append(
            {QString(), 1, 1, QStringLiteral("Пустое выражение")});
        return result;
    }

    for (const Token &t : std::as_const(m_tokens)) {
        if (t.code == ExprTokenCode::ERROR) {
            result.syntaxErrors.append(
                {t.lexeme,
                 t.line,
                 t.startCol,
                 QStringLiteral("Лексическая ошибка: недопустимый символ")});
        }
    }

    if (!result.syntaxErrors.isEmpty()) {
        return result;
    }

    const ExprValue value = parseE();

    if (!isAtEnd()) {
        const Token &extra = currentToken();
        m_errors.append({extra.lexeme,
                         extra.line,
                         extra.startCol,
                         QStringLiteral("Лишние символы после выражения")});
    }

    result.syntaxErrors = m_errors;
    if (m_hasFatalError || !result.syntaxErrors.isEmpty()) {
        return result;
    }

    result.success = true;
    result.tetrads = m_tetrads;
    result.rpn = value.rpn;

    qint64 eval = 0;
    QString evalMsg;
    if (evaluateRpn(result.rpn, eval, evalMsg)) {
        result.hasEvaluation = true;
        result.evaluatedValue = eval;
    } else {
        result.hasEvaluation = false;
        result.evaluationMessage = evalMsg;
    }

    return result;
}
