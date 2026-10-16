#ifndef LEXER_H
#define LEXER_H
#include <string>
#include <vector>

enum TokenType {
    VARIABLE,
    AND,
    OR,
    NOT,
    IMPLIES,
    IFF,
    LPAREN,
    RPAREN
};

struct Token {
    TokenType type;
    std::string value;
    std::size_t offset = 0;
    std::size_t line = 1;
    std::size_t column = 1;
    std::string lexeme;
};

std::vector<Token> tokenize(const std::string& formula);

#endif