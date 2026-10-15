#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "lexer.h"
#include <string>
#include <vector>

// Converts infix tokens into postfix notation using the Shunting-Yard algorithm.
std::vector<Token> infixToPostfix(const std::vector<Token>& tokens);

// Builds the shared visitor-based AST from postfix tokens.
NodePtr buildAST(const std::vector<Token>& postfix);

// Tokenizes, parses, and returns the shared AST for a formula.
NodePtr parseFormula(const std::string& formula);

#endif
