#include "lexer.h"
#include <cctype>
#include <stdexcept>
using namespace std;

vector<Token> tokenize(const string &formula) {
    vector<Token> tokens;

    for (int i = 0; i < formula.size(); i++) {
        char ch = formula[i];

        if (isspace(ch))
            continue;

        if (ch >= 'a' && ch <= 'z') {
            tokens.push_back({VARIABLE, string(1, ch)});
        }
        else if (ch == '&') {
            tokens.push_back({AND, "&"});
        }
        else if (ch == '|') {
            tokens.push_back({OR, "|"});
        }
        else if (ch == '~') {
            tokens.push_back({NOT, "~"});
        }
        else if (ch == '(') {
            tokens.push_back({LPAREN, "("});
        }
        else if (ch == ')') {
            tokens.push_back({RPAREN, ")"});
        }
        else if (ch == '-' && i + 1 < formula.size() && formula[i + 1] == '>') {
            tokens.push_back({IMPLIES, "->"});
            i++;
        }
        else if (ch == '<' && i + 2 < formula.size() &&
                 formula[i + 1] == '-' && formula[i + 2] == '>') {
            tokens.push_back({IFF, "<->"});
            i += 2;
        }
        else {
            throw runtime_error("Invalid character");
        }
    }

    return tokens;
}