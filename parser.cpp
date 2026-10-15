
#include "parser.h"
#include <stdexcept>
#include <utility>

using namespace std;

// Returns the precedence of each operator.
// A higher number means higher precedence.
namespace {

int precedence(TokenType type) {
    switch (type) {
        case NOT:     return 5; // Highest precedence
        case AND:     return 4;
        case OR:      return 3;
        case IMPLIES: return 2;
        case IFF:     return 1; // Lowest precedence
        default:      return 0;
    }
}

// Checks whether a token is an operator.
bool isOperator(TokenType type) {
    return type == NOT ||
           type == AND ||
           type == OR ||
           type == IMPLIES ||
           type == IFF;
}

// Checks whether an operator is right-associative.
// NOT and implication are treated as right-associative.
bool isRightAssociative(TokenType type) {
    return type == NOT || type == IMPLIES;
}

} // namespace

// Converts infix notation into postfix notation
// using the Shunting-Yard algorithm.
vector<Token> infixToPostfix(const vector<Token>& tokens) {
    vector<Token> output;    // Stores the postfix expression
    vector<Token> operators; // Stack for operators and parentheses
    bool expects_operand = true;

    // Process each token from left to right.
    for (const Token& token : tokens) {
        // Variables go directly into the output.
        if (token.type == VARIABLE) {
            if (!expects_operand) {
                throw runtime_error("Missing operator before variable");
            }
            output.push_back(token);
            expects_operand = false;
        }
        // Opening parentheses are pushed onto the operator stack.
        else if (token.type == LPAREN) {
            if (!expects_operand) {
                throw runtime_error("Missing operator before opening parenthesis");
            }
            operators.push_back(token);
        }
        // When a closing parenthesis appears, pop operators
        // until the matching opening parenthesis is found.
        else if (token.type == RPAREN) {
            if (expects_operand) {
                throw runtime_error("Missing operand before closing parenthesis");
            }
            while (!operators.empty() &&
                   operators.back().type != LPAREN) {
                output.push_back(operators.back());
                operators.pop_back();
            }

            // No opening parenthesis means the expression is invalid.
            if (operators.empty()) {
                throw runtime_error("Mismatched parentheses");
            }

            // Remove the matching opening parenthesis.
            operators.pop_back();
            expects_operand = false;
        }
        // Process operators according to precedence and associativity.
        else if (isOperator(token.type)) {
            if (token.type == NOT) {
                if (!expects_operand) {
                    throw runtime_error("Unexpected NOT after operand");
                }
            } else {
                if (expects_operand) {
                    throw runtime_error("Missing operand for binary operator");
                }
                expects_operand = true;
            }

            while (!operators.empty() &&
                   isOperator(operators.back().type)) {
                TokenType top = operators.back().type;

                bool shouldPop;

                // For right-associative operators, pop only
                // operators with strictly higher precedence.
                if (isRightAssociative(token.type)) {
                    shouldPop =
                        precedence(top) > precedence(token.type);
                }
                // For left-associative operators, pop operators
                // with higher or equal precedence.
                else {
                    shouldPop =
                        precedence(top) >= precedence(token.type);
                }

                // Stop if the operator on the stack should remain there.
                if (!shouldPop) {
                    break;
                }

                // Move the top operator to the postfix output.
                output.push_back(operators.back());
                operators.pop_back();
            }

            // Push the current operator onto the stack.
            operators.push_back(token);
        }
        else {
            // Reject any token that is not recognized here.
            throw runtime_error("Invalid token");
        }
    }

    if (expects_operand) {
        throw runtime_error("Formula ends with a missing operand");
    }

    // Pop all remaining operators into the output.
    while (!operators.empty()) {
        // An opening parenthesis remaining here is unmatched.
        if (operators.back().type == LPAREN) {
            throw runtime_error("Mismatched parentheses");
        }

        output.push_back(operators.back());
        operators.pop_back();
    }

    // Return the completed postfix expression.
    return output;
}

// Builds an Abstract Syntax Tree from postfix tokens
// using a stack of AST node pointers.
NodePtr buildAST(const vector<Token>& postfix) {
    vector<NodePtr> stack; // Stores partial AST nodes

    // Process each token in postfix order.
    for (const Token& token : postfix) {
        // A variable creates a leaf node and is pushed onto the stack.
        if (token.type == VARIABLE) {
            stack.push_back(make_shared<VarNode>(token.value));
        }
        // NOT is a unary operator, so it needs one operand.
        else if (token.type == NOT) {
            if (stack.empty()) {
                throw runtime_error("Missing operand for NOT");
            }

            // Pop the operand for the NOT node.
            auto operand = stack.back();
            stack.pop_back();

            // Create a NOT node with the operand as its left child.
            stack.push_back(make_shared<NotNode>(move(operand)));
        }
        // AND, OR, implication, and biconditional are binary operators.
        else if (token.type == AND ||
                 token.type == OR ||
                 token.type == IMPLIES ||
                 token.type == IFF) {

            // Binary operators require two operands.
            if (stack.size() < 2) {
                throw runtime_error("Missing operand for binary operator");
            }

            // The first popped node is the right operand.
            auto right = stack.back();
            stack.pop_back();

            // The second popped node is the left operand.
            auto left = stack.back();
            stack.pop_back();

            // Determine the AST node type for this operator.
            switch (token.type) {
                case AND:
                    stack.push_back(make_shared<AndNode>(move(left), move(right)));
                    break;
                case OR:
                    stack.push_back(make_shared<OrNode>(move(left), move(right)));
                    break;
                case IMPLIES:
                    stack.push_back(make_shared<ImpliesNode>(move(left), move(right)));
                    break;
                case IFF:
                    stack.push_back(make_shared<IffNode>(move(left), move(right)));
                    break;
                default:
                    throw runtime_error("Invalid operator");
            }
        }
    }

    // A valid expression must leave exactly one AST root on the stack.
    if (stack.size() != 1) {
        throw runtime_error("Invalid formula");
    }

    // Return the root of the completed AST.
    return stack.back();
}

// Main parsing function that connects all three stages.
NodePtr parseFormula(const string& formula) {
    // Stage 1: Convert the input string into tokens using the lexer.
    vector<Token> tokens = tokenize(formula);

    // Stage 2: Convert the infix tokens into postfix notation.
    vector<Token> postfix = infixToPostfix(tokens);

    // Stage 3: Build and return the AST from the postfix tokens.
    return buildAST(postfix);
}
