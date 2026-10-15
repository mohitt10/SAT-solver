#pragma once

#include "ast.h"

class NNFConverter {
public:
    // Returns a new AST containing only variables, negated variables, AND, and OR.
    NodePtr convert(const ASTNode& root) const;
};
