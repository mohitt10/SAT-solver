
#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include <memory>
#include <vector>
#include <string>

using namespace std;

// Represents the different types of nodes in the AST.
enum NodeType {
    VAR_NODE,       
    NOT_NODE,       
    AND_NODE,     
    OR_NODE,       
    IMPLIES_NODE,  // Implication operation (->)
    IFF_NODE       // Biconditional operation (<->)
};

// Represents one node in the Abstract Syntax Tree (AST).
struct ASTNode {
    NodeType type;                  
    string value;                  
    shared_ptr<ASTNode> left;       // Pointer to the left child
    shared_ptr<ASTNode> right;      // Pointer to the right child

    // Constructor to initialize an AST node.
    ASTNode(NodeType type, string value = "",
            shared_ptr<ASTNode> left = nullptr,
            shared_ptr<ASTNode> right = nullptr)
        : type(type), value(value), left(left), right(right) {}
};

// Converts infix tokens into postfix notation using Shunting-Yard.
vector<Token> infixToPostfix(const vector<Token>& tokens);

// Builds an AST from postfix tokens using a stack.
shared_ptr<ASTNode> buildAST(const vector<Token>& postfix);

// Connects tokenization, postfix conversion, and AST construction.
shared_ptr<ASTNode> parseFormula(const string& formula);

#endif
