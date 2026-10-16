#include "parser.h"
#include "parse_error.h"
#include <algorithm>
#include <utility>

using namespace std;

namespace {

int precedence(TokenType type) {
    switch (type) {
        case NOT:     return 5;
        case AND:     return 4;
        case OR:      return 3;
        case IMPLIES: return 2;
        case IFF:     return 1;
        default:      return 0;
    }
}

bool isOperator(TokenType type) {
    return type == NOT || type == AND || type == OR ||
           type == IMPLIES || type == IFF;
}

bool isRightAssociative(TokenType type) {
    return type == NOT || type == IMPLIES;
}

struct SourcePosition {
    size_t offset;
    size_t line;
    size_t column;
};

SourcePosition positionAt(const string& source, size_t offset) {
    SourcePosition position{0, 1, 1};
    const size_t limit = min(offset, source.size());
    for (size_t i = 0; i < limit; ++i) {
        const unsigned char ch = static_cast<unsigned char>(source[i]);
        if (ch == '\n') {
            ++position.line;
            position.column = 1;
        } else if ((ch & 0xC0) != 0x80) {
            ++position.column;
        }
    }
    position.offset = offset;
    return position;
}

SourcePosition endPosition(const vector<Token>& tokens, const string* source) {
    if (source != nullptr)
        return positionAt(*source, source->size());
    if (tokens.empty())
        return {0, 1, 1};

    const Token& last = tokens.back();
    const size_t length = last.lexeme.empty() ? last.value.size() : last.lexeme.size();
    return {last.offset + length, last.line, last.column + length};
}

string tokenText(const Token& token) {
    return token.lexeme.empty() ? token.value : token.lexeme;
}

string errorMessage(ErrorKind kind, const string& found) {
    const string actual = found.empty() ? "end of input" : "'" + found + "'";
    switch (kind) {
        case ErrorKind::INVALID_CHARACTER: return "invalid character " + actual;
        case ErrorKind::INVALID_TOKEN: return "invalid token " + actual;
        case ErrorKind::UNEXPECTED_TOKEN: return "unexpected token " + actual;
        case ErrorKind::MISSING_OPERAND: return "expected operand, found " + actual;
        case ErrorKind::MISSING_OPERATOR: return "expected operator, found " + actual;
        case ErrorKind::MISSING_CLOSING_PAREN: return "expected ')', found " + actual;
        case ErrorKind::UNEXPECTED_CLOSING_PAREN: return "unexpected closing parenthesis";
        case ErrorKind::UNEXPECTED_END_OF_INPUT: return "unexpected end of input";
    }
    return "invalid formula";
}

[[noreturn]] void raiseSyntaxError(ErrorKind kind,
                                  const Token* token,
                                  const vector<Token>& tokens,
                                  const string* source,
                                  vector<string> expected = {}) {
    string found;
    SourcePosition position{};
    if (token != nullptr) {
        found = tokenText(*token);
        position = {token->offset, token->line, token->column};
    } else {
        position = endPosition(tokens, source);
    }

    throw ParseError(ErrorCategory::SYNTAX, kind, errorMessage(kind, found),
                     position.offset, position.line, position.column,
                     found, std::move(expected));
}

vector<Token> infixToPostfixImpl(const vector<Token>& tokens, const string* source) {
    vector<Token> output;
    vector<Token> operators;
    bool expects_operand = true;

    for (const Token& token : tokens) {
        if (token.type == VARIABLE) {
            if (!expects_operand) {
                raiseSyntaxError(ErrorKind::MISSING_OPERATOR, &token, tokens, source,
                                 {"AND", "OR", "IMPLIES", "IFF"});
            }
            output.push_back(token);
            expects_operand = false;
        } else if (token.type == LPAREN) {
            if (!expects_operand) {
                raiseSyntaxError(ErrorKind::MISSING_OPERATOR, &token, tokens, source,
                                 {"AND", "OR", "IMPLIES", "IFF"});
            }
            operators.push_back(token);
        } else if (token.type == RPAREN) {
            if (expects_operand) {
                raiseSyntaxError(ErrorKind::MISSING_OPERAND, &token, tokens, source,
                                 {"IDENTIFIER", "~", "("});
            }
            while (!operators.empty() && operators.back().type != LPAREN) {
                output.push_back(operators.back());
                operators.pop_back();
            }
            if (operators.empty()) {
                raiseSyntaxError(ErrorKind::UNEXPECTED_CLOSING_PAREN,
                                 &token, tokens, source,
                                 {"AND", "OR", "IMPLIES", "IFF"});
            }
            operators.pop_back();
            expects_operand = false;
        } else if (isOperator(token.type)) {
            if (token.type == NOT) {
                if (!expects_operand) {
                    raiseSyntaxError(ErrorKind::MISSING_OPERATOR, &token, tokens, source,
                                     {"AND", "OR", "IMPLIES", "IFF"});
                }
            } else {
                if (expects_operand) {
                    raiseSyntaxError(ErrorKind::MISSING_OPERAND, &token, tokens, source,
                                     {"IDENTIFIER", "~", "("});
                }
                expects_operand = true;
            }

            while (!operators.empty() && isOperator(operators.back().type)) {
                const TokenType top = operators.back().type;
                const bool should_pop = isRightAssociative(token.type)
                                            ? precedence(top) > precedence(token.type)
                                            : precedence(top) >= precedence(token.type);
                if (!should_pop)
                    break;
                output.push_back(operators.back());
                operators.pop_back();
            }
            operators.push_back(token);
        } else {
            raiseSyntaxError(ErrorKind::UNEXPECTED_TOKEN, &token, tokens, source);
        }
    }

    if (expects_operand) {
        raiseSyntaxError(ErrorKind::UNEXPECTED_END_OF_INPUT, nullptr, tokens, source,
                         {"IDENTIFIER", "~", "("});
    }

    while (!operators.empty()) {
        if (operators.back().type == LPAREN) {
            raiseSyntaxError(ErrorKind::MISSING_CLOSING_PAREN, nullptr, tokens, source,
                             {")"});
        }
        output.push_back(operators.back());
        operators.pop_back();
    }
    return output;
}

NodePtr buildASTImpl(const vector<Token>& postfix, const string* source) {
    vector<NodePtr> stack;
    for (const Token& token : postfix) {
        if (token.type == VARIABLE) {
            stack.push_back(make_shared<VarNode>(token.value));
        } else if (token.type == NOT) {
            if (stack.empty()) {
                raiseSyntaxError(ErrorKind::MISSING_OPERAND, &token, postfix, source,
                                 {"IDENTIFIER", "~", "("});
            }
            auto operand = stack.back();
            stack.pop_back();
            stack.push_back(make_shared<NotNode>(std::move(operand)));
        } else if (token.type == AND || token.type == OR ||
                   token.type == IMPLIES || token.type == IFF) {
            if (stack.size() < 2) {
                raiseSyntaxError(ErrorKind::MISSING_OPERAND, &token, postfix, source,
                                 {"IDENTIFIER", "~", "("});
            }
            auto right = stack.back();
            stack.pop_back();
            auto left = stack.back();
            stack.pop_back();
            switch (token.type) {
                case AND:
                    stack.push_back(make_shared<AndNode>(std::move(left), std::move(right)));
                    break;
                case OR:
                    stack.push_back(make_shared<OrNode>(std::move(left), std::move(right)));
                    break;
                case IMPLIES:
                    stack.push_back(make_shared<ImpliesNode>(std::move(left), std::move(right)));
                    break;
                case IFF:
                    stack.push_back(make_shared<IffNode>(std::move(left), std::move(right)));
                    break;
                default:
                    break;
            }
        } else {
            raiseSyntaxError(ErrorKind::UNEXPECTED_TOKEN, &token, postfix, source);
        }
    }
    if (stack.size() != 1)
        raiseSyntaxError(ErrorKind::UNEXPECTED_TOKEN, nullptr, postfix, source);
    return stack.back();
}

} // namespace

vector<Token> infixToPostfix(const vector<Token>& tokens) {
    return infixToPostfixImpl(tokens, nullptr);
}

NodePtr buildAST(const vector<Token>& postfix) {
    return buildASTImpl(postfix, nullptr);
}

NodePtr parseFormula(const string& formula) {
    const vector<Token> tokens = tokenize(formula);
    const vector<Token> postfix = infixToPostfixImpl(tokens, &formula);
    return buildASTImpl(postfix, &formula);
}
