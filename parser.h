#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include <memory>
#include <string>

using namespace std;

// Defines the types of nodes used in the AST.
enum NodeType {
    VAR_NODE,
    NOT_NODE,
    AND_NODE,
    OR_NODE,
    IMPLIES_NODE,
    IFF_NODE
};

// Represents a node in the Abstract Syntax Tree (AST).
struct ASTNode {
    NodeType type;
    string value;
    shared_ptr<ASTNode> left;
    shared_ptr<ASTNode> right;

    // Initializes an AST node with its type, value, and children.
    ASTNode(NodeType t, string v = "",
            shared_ptr<ASTNode> l = nullptr,
            shared_ptr<ASTNode> r = nullptr)
        : type(t), value(v), left(l), right(r) {}
};

// Parses a logical formula and returns the AST root.
shared_ptr<ASTNode> parse(const string &formula);

#endif