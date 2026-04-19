#ifndef SEMANTIC_H
#define SEMANTIC_H

#include <QList>
#include <QString>
#include <memory>

#include "ast.h"

struct SemanticError {
    QString fragment;
    int line;
    int col;
    QString description;
};

namespace SemanticAnalyzer {

/// Максимальная длина содержимого строкового литерала (без кавычек), правило 3.
constexpr int MAX_STRING_LITERAL_LENGTH = 255;

/**
 * Семантический анализ объявлений констант (строковый поднабор).
 *
 * Правило 1 — уникальность имён в области видимости (файл).
 * Правило 2 — совместимость типов: объявление «: string» с инициализатором-строковым литералом.
 * Правило 3 — допустимая длина строкового литерала (см. MAX_STRING_LITERAL_LENGTH).
 *
 * Правило 4 — использование идентификаторов: в текущей грамматике справа допускается только
 * литерал; ссылок на другие идентификаторы нет, проверка «использование объявленных имён»
 * тривиальна и может быть расширена при добавлении ссылок на константы.
 */
struct Result {
    QList<SemanticError> errors;
    std::unique_ptr<ProgramNode> program;
};

Result analyze(std::unique_ptr<ProgramNode> source);

} // namespace SemanticAnalyzer

#endif
