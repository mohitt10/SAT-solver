#include "lexer.h"
#include <cctype>
#include <stdexcept>

using namespace std;

vector<Token> tokenize(const string& formula) {
    vector<Token> tokens;

    for (size_t i = 0; i < formula.size(); i++) {
        char ch = formula[i];

        if (isspace(static_cast<unsigned char>(ch)))
            continue;

        if (isalpha(static_cast<unsigned char>(ch))) {
            string variable;
            while (i < formula.size() &&
                   (isalnum(static_cast<unsigned char>(formula[i])) || formula[i] == '_')) {
                variable += formula[i];
                i++;
            }
            tokens.push_back({VARIABLE, variable});
            i--;
        }else if (ch == '&') {
            tokens.push_back({AND, "&"});
        }else if (ch == '|') {
            tokens.push_back({OR, "|"});
        }else if (ch == '~') {
            tokens.push_back({NOT, "~"});
        }else if (ch == '(') {
            tokens.push_back({LPAREN, "("});
        }else if (ch == ')') {
            tokens.push_back({RPAREN, ")"});
        }else if (ch == '-' && i + 1 < formula.size() && formula[i + 1] == '>') {
            tokens.push_back({IMPLIES, "->"});
            i++;
        }else if (ch == '<' && i + 2 < formula.size() &&
                 formula[i + 1] == '-' && formula[i + 2] == '>') {
            tokens.push_back({IFF, "<->"});
            i += 2;
        }else {
            throw runtime_error("Invalid character");
        }
    }

    return tokens;
}
