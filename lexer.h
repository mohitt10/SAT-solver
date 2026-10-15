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
};

std::vector<Token> tokenize(const std::string& formula);

#endif