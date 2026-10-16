#include "lexer.h"
#include <algorithm>
#include <cctype>
#include "parse_error.h"

using namespace std;

vector<Token> tokenize(const string& formula) {
    vector<Token> tokens;
    size_t line = 1;
    size_t column = 1;
    size_t i = 0;

    const auto advance = [&](char ch, size_t& current_line, size_t& current_column) {
        if (ch == '\n') {
            ++current_line;
            current_column = 1;
        } else if ((static_cast<unsigned char>(ch) & 0xC0) != 0x80) {
            ++current_column;
        }
    };
    const auto add_token = [&](TokenType type,
                               const string& value,
                               size_t start,
                               size_t start_line,
                               size_t start_column) {
        tokens.push_back({type, value, start, start_line, start_column,
                          formula.substr(start, i - start)});
    };

    while (i < formula.size()) {
        const char ch = formula[i];
        if (isspace(static_cast<unsigned char>(ch))) {
            advance(ch, line, column);
            ++i;
            continue;
        }

        const size_t start = i;
        const size_t start_line = line;
        const size_t start_column = column;

        if (isalnum(static_cast<unsigned char>(ch))) {
            string variable;
            while (i < formula.size() &&
                   (isalnum(static_cast<unsigned char>(formula[i])) || formula[i] == '_')) {
                variable += formula[i];
                advance(formula[i], line, column);
                ++i;
            }
            string normalized = variable;
            for (char& letter : normalized)
                letter = static_cast<char>(tolower(static_cast<unsigned char>(letter)));

            if (normalized == "and")
                add_token(AND, variable, start, start_line, start_column);
            else if (normalized == "or")
                add_token(OR, variable, start, start_line, start_column);
            else if (normalized == "implies")
                add_token(IMPLIES, "->", start, start_line, start_column);
            else if (normalized == "iff")
                add_token(IFF, "<->", start, start_line, start_column);
            else
                add_token(VARIABLE, variable, start, start_line, start_column);
            continue;
        }

        TokenType type;
        size_t token_length = 1;
        switch (ch) {
            case '&': type = AND; break;
            case '|': type = OR; break;
            case '~': type = NOT; break;
            case '(': type = LPAREN; break;
            case ')': type = RPAREN; break;
            case '-':
                if (i + 1 < formula.size() && formula[i + 1] == '>') {
                    type = IMPLIES;
                    token_length = 2;
                } else {
                    throw ParseError(ErrorCategory::LEXICAL, ErrorKind::INVALID_TOKEN,
                                     "invalid token '" + formula.substr(i, 1) + "'",
                                     i, line, column, formula.substr(i, 1),
                                     {"->"});
                }
                break;
            case '<':
                if (i + 2 < formula.size() && formula[i + 1] == '-' &&
                    formula[i + 2] == '>') {
                    type = IFF;
                    token_length = 3;
                } else {
                    const size_t length = (i + 1 < formula.size() && formula[i + 1] == '-')
                                              ? 2
                                              : 1;
                    throw ParseError(ErrorCategory::LEXICAL, ErrorKind::INVALID_TOKEN,
                                     "invalid token '" + formula.substr(i, length) + "'",
                                     i, line, column, formula.substr(i, length),
                                     {"<->"});
                }
                break;
            default: {
                const unsigned char first = static_cast<unsigned char>(ch);
                size_t length = 1;
                if ((first & 0xE0) == 0xC0) length = 2;
                else if ((first & 0xF0) == 0xE0) length = 3;
                else if ((first & 0xF8) == 0xF0) length = 4;
                length = min(length, formula.size() - i);
                const string found = formula.substr(i, length);
                throw ParseError(ErrorCategory::LEXICAL, ErrorKind::INVALID_CHARACTER,
                                 "invalid character '" + found + "'",
                                 i, line, column, found);
            }
        }

        const string value = formula.substr(start, token_length);
        for (size_t j = 0; j < token_length; ++j)
            advance(formula[i + j], line, column);
        i += token_length;
        add_token(type, value, start, start_line, start_column);
    }

    return tokens;
}
