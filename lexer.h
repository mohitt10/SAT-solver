#ifndef LEXER_H
#define LEXER_H
#include <string>
#include <vector>
using namespace std;

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
    string value;
};

vector<Token> tokenize(const string &formula);

#endif