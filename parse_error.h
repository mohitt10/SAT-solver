#ifndef PARSE_ERROR_H
#define PARSE_ERROR_H

#include <cstddef>
#include <exception>
#include <string>
#include <utility>
#include <vector>

enum class ErrorCategory {
    LEXICAL,
    SYNTAX
};

enum class ErrorKind {
    INVALID_CHARACTER,
    INVALID_TOKEN,
    UNEXPECTED_TOKEN,
    MISSING_OPERAND,
    MISSING_OPERATOR,
    MISSING_CLOSING_PAREN,
    UNEXPECTED_CLOSING_PAREN,
    UNEXPECTED_END_OF_INPUT
};

class ParseError : public std::exception {
public:
    ErrorCategory category;
    ErrorKind kind;
    std::string message;
    std::size_t offset;
    std::size_t line;
    std::size_t column;
    std::string found;
    std::vector<std::string> expected;

    ParseError(ErrorCategory category,
               ErrorKind kind,
               std::string message,
               std::size_t offset,
               std::size_t line,
               std::size_t column,
               std::string found = {},
               std::vector<std::string> expected = {})
        : category(category),
          kind(kind),
          message(std::move(message)),
          offset(offset),
          line(line),
          column(column),
          found(std::move(found)),
          expected(std::move(expected)) {}

    const char* what() const noexcept override {
        return message.c_str();
    }
};

#endif
