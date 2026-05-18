#ifndef EXPR_PARSER_H
#define EXPR_PARSER_H

#include <QList>
#include <QString>
#include <QStringList>
#include "token.h"
#include "parser.h"

struct Tetrad {
    QString op;
    QString arg1;
    QString arg2;
    QString result;
};

struct ExprValue {
    QString place;
    QStringList rpn;
};

struct ExprAnalysisResult {
    bool success = false;
    QList<SyntaxError> syntaxErrors;
    QList<Tetrad> tetrads;
    QStringList rpn;
    bool hasEvaluation = false;
    qint64 evaluatedValue = 0;
    QString evaluationMessage;
};

class ExprParser {
public:
    explicit ExprParser(const QList<Token> &tokens);

    ExprAnalysisResult parse();

private:
    QList<Token> m_tokens;
    int m_pos = 0;
    int m_tempCounter = 0;
    QList<SyntaxError> m_errors;
    QList<Tetrad> m_tetrads;
    bool m_hasFatalError = false;

    const Token &currentToken() const;
    bool isAtEnd() const;
    bool check(int code) const;
    bool advance();
    void syntaxError(const QString &expected);
    QString newTemp();
    void emitTetrad(const QString &op, const QString &arg1,
                    const QString &arg2, const QString &result);

    ExprValue parseE();
    ExprValue parseA(const ExprValue &inh);
    ExprValue parseT();
    ExprValue parseB(const ExprValue &inh);
    ExprValue parseF();

    static bool evaluateRpn(const QStringList &rpn, qint64 &value, QString &message);
};

#endif // EXPR_PARSER_H
