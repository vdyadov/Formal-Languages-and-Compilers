#include "lexer.h"

QList<Token> Lexer::tokenize(const QString &source) {
    QList<Token> tokens;
    int pos = 0;
    int line = 1;
    int column = 1;

    while (pos < source.length()) {
        QChar ch = source[pos];

        if (ch == ' ' || ch == '\t') {
            tokens.append(Token(4, "Пробел", "(пробел)", line, column, column));
            pos++; column++;
            continue;
        }

        if (ch == '\n') {
            pos++; line++; column = 1;
            continue;
        }

        if (ch == ';') {
            tokens.append(Token(8, "Оператор конца строки", ";", line, column, column));
            pos++; column++;
            continue;
        }

        if (ch == ':') {
            tokens.append(Token(5, "Оператор присваивания типа", ":", line, column, column));
            pos++; column++;
            continue;
        }

        if (ch == '=') {
            tokens.append(Token(6, "Оператор присваивания значения", "=", line, column, column));
            pos++; column++;
            continue;
        }

        if (ch == '\'') {
            int startCol = column;
            QString val = "'";
            pos++; column++;
            bool closed = false;

            while (pos < source.length() && source[pos] != '\'' && source[pos] != '\n') {
                val += source[pos];
                pos++; column++;
            }

            if (pos < source.length() && source[pos] == '\'') {
                val += "'";
                tokens.append(Token(7, "Инициализатор строки", val, line, startCol, column));
                pos++; column++;
                closed = true;
            }

            if (!closed) {
                tokens.append(Token(-1, "ОШИБКА", "Незакрытая строка: " + val, line, startCol, column));
            }
            continue;
        }

        if (ch.isLetter() || ch == '_') {
            int start = column;
            QString word;
            while (pos < source.length() && (source[pos].isLetterOrNumber() || source[pos] == '_')) {
                word += source[pos];
                pos++; column++;
            }

            if (word.toLower() == "const") {
                tokens.append(Token(1, "Ключевое слово", word, line, start, column - 1));
            } else if (word.toLower() == "string") {
                tokens.append(Token(2, "Тип переменной", word, line, start, column - 1));
            } else {
                tokens.append(Token(3, "Имя переменной", word, line, start, column - 1));
            }
            continue;
        }

        tokens.append(Token(-1, "ОШИБКА", QString(ch), line, column, column));
        pos++; column++;
    }
    return tokens;
}
