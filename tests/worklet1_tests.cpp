#include "nnf.h"
#include "parse_error.h"
#include "parser.h"
#include "truth_table.h"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

namespace {

void require(bool condition, const string& message) {
    if (!condition) {
        throw runtime_error(message);
    }
}

class NNFValidator : public Visitor {
public:
    void validate(const ASTNode& node) { node.accept(*this); }

    void visit(const VarNode&) override {}

    void visit(const NotNode& node) override {
        require(node.left != nullptr && node.right == nullptr,
                "NNF negation must have exactly one child");
        require(dynamic_cast<const VarNode*>(node.left.get()) != nullptr,
                "NNF negation must apply directly to a variable");
    }

    void visit(const AndNode& node) override { binary(node); }
    void visit(const OrNode& node) override { binary(node); }

    void visit(const ImpliesNode&) override {
        throw runtime_error("NNF must not contain implication");
    }

    void visit(const IffNode&) override {
        throw runtime_error("NNF must not contain biconditional");
    }

private:
    void binary(const ASTNode& node) {
        require(node.left != nullptr && node.right != nullptr,
                "NNF binary node must have both children");
        node.left->accept(*this);
        node.right->accept(*this);
    }
};

void test_lexer_and_parser() {
    const auto tokens = tokenize("(red_2 & ~blue) -> green");
    require(tokens.size() == 8, "lexer should return all formula tokens");
    require(tokens.front().type == LPAREN, "lexer should recognize opening parenthesis");
    require(tokens[1].type == VARIABLE && tokens[1].value == "red_2",
            "lexer should retain full variable names");
    require(tokens[5].type == RPAREN && tokens[6].type == IMPLIES,
            "lexer should recognize parentheses and implication");

    const auto numeric_variables = tokenize("2nd_var & 10");
    require(numeric_variables.size() == 3 &&
                numeric_variables[0].type == VARIABLE &&
                numeric_variables[0].value == "2nd_var" &&
                numeric_variables[2].type == VARIABLE &&
                numeric_variables[2].value == "10",
            "lexer should allow variables to start with digits");

    const auto word_operators = tokenize("red and blue AND green AnD cyan or yellow Or magenta candy orange");
    require(word_operators.size() == 13 &&
                word_operators[1].type == AND &&
                word_operators[1].value == "and" &&
                word_operators[3].type == AND &&
                word_operators[3].value == "AND" &&
                word_operators[5].type == AND &&
                word_operators[5].value == "AnD" &&
                word_operators[7].type == OR &&
                word_operators[7].value == "or" &&
                word_operators[9].type == OR &&
                word_operators[9].value == "Or" &&
                word_operators[11].type == VARIABLE &&
                word_operators[11].value == "candy" &&
                word_operators[12].type == VARIABLE &&
                word_operators[12].value == "orange",
            "lexer should recognize standalone and/or words case-insensitively");
    require(parseFormula("A AnD B Or C") != nullptr,
            "parser should accept case-insensitive word-form operators");

    NodePtr root = parseFormula("(A & ~B) -> C");
    require(dynamic_cast<ImpliesNode*>(root.get()) != nullptr,
            "parser should produce the shared visitor-based AST");
    require(parseFormula("2nd_var & 10") != nullptr,
            "parser should accept variables that start with digits");
}

void test_truth_table() {
    TruthTableEngine engine;
    const TruthTable tautology = engine.truth_table(*parseFormula("A | ~A"));
    const TruthTable contradiction = engine.truth_table(*parseFormula("A & ~A"));
    const TruthTable contingency = engine.truth_table(*parseFormula("A -> B"));

    require(engine.classify(tautology) == Classification::TAUTOLOGY,
            "tautology should be classified correctly");
    require(engine.classify(contradiction) == Classification::CONTRADICTION,
            "contradiction should be classified correctly");
    require(engine.classify(contingency) == Classification::CONTINGENCY,
            "contingency should be classified correctly");
}

void test_nnf() {
    const vector<string> formulas = {
        "~(A & B)",
        "~(A | B)",
        "A -> B",
        "~(A -> B)",
        "A <-> B",
        "~(A <-> B)",
        "~~(A | B)",
        "(A -> B) & (C <-> ~D)"
    };

    TruthTableEngine engine;
    NNFConverter converter;
    NNFValidator validator;

    for (const string& formula : formulas) {
        NodePtr original = parseFormula(formula);
        NodePtr nnf = converter.convert(*original);
        validator.validate(*nnf);
        require(engine.are_equivalent(*original, *nnf),
                "NNF conversion must preserve formula semantics: " + formula);
    }
}

void test_invalid_formulas() {
    for (const string formula : {"", "()", "A &", "(A | B", "A B", "A~", "(A)B"}) {
        bool rejected = false;
        try {
            (void)parseFormula(formula);
        } catch (const exception&) {
            rejected = true;
        }
        require(rejected, "parser should reject invalid formula: " + formula);
    }
}

void test_structured_parse_errors() {
    const auto capture_error = [](const string& formula) {
        try {
            (void)parseFormula(formula);
        } catch (const ParseError& error) {
            return error;
        }
        throw runtime_error("expected ParseError for formula: " + formula);
    };

    const ParseError invalid_character = capture_error("A & @");
    require(invalid_character.category == ErrorCategory::LEXICAL &&
                invalid_character.kind == ErrorKind::INVALID_CHARACTER &&
                invalid_character.offset == 4 &&
                invalid_character.line == 1 &&
                invalid_character.column == 5 &&
                invalid_character.found == "@",
            "lexer errors should provide their category and source location");

    const ParseError invalid_operator = capture_error("A - B");
    require(invalid_operator.category == ErrorCategory::LEXICAL &&
                invalid_operator.kind == ErrorKind::INVALID_TOKEN &&
                invalid_operator.offset == 2 &&
                invalid_operator.found == "-" &&
                !invalid_operator.expected.empty(),
            "malformed operators should be reported as lexical token errors");

    const ParseError missing_operand = capture_error("A & )");
    require(missing_operand.category == ErrorCategory::SYNTAX &&
                missing_operand.kind == ErrorKind::MISSING_OPERAND &&
                missing_operand.offset == 4 &&
                missing_operand.found == ")" &&
                !missing_operand.expected.empty(),
            "missing operands should identify the unexpected token");

    const ParseError missing_operator = capture_error("A B");
    require(missing_operator.kind == ErrorKind::MISSING_OPERATOR &&
                missing_operator.offset == 2 &&
                missing_operator.found == "B",
            "adjacent operands should report a missing operator");

    const ParseError unexpected_close = capture_error("A )");
    require(unexpected_close.kind == ErrorKind::UNEXPECTED_CLOSING_PAREN &&
                unexpected_close.offset == 2,
            "a top-level closing parenthesis should have its own error kind");

    const ParseError missing_close = capture_error("(A | B");
    require(missing_close.kind == ErrorKind::MISSING_CLOSING_PAREN &&
                missing_close.offset == 6 &&
                missing_close.found.empty() &&
                missing_close.expected == vector<string>{")"},
            "an unclosed parenthesis should report end-of-input and expected ')'");

    const ParseError unexpected_end = capture_error("A &\n");
    require(unexpected_end.kind == ErrorKind::UNEXPECTED_END_OF_INPUT &&
                unexpected_end.offset == 4 &&
                unexpected_end.line == 2 &&
                unexpected_end.column == 1 &&
                unexpected_end.found.empty(),
            "end-of-input errors should report the end location");

    const ParseError empty_input = capture_error("");
    require(empty_input.kind == ErrorKind::UNEXPECTED_END_OF_INPUT &&
                empty_input.expected == vector<string>({"IDENTIFIER", "~", "("}),
            "empty formulas should report expected operand starters");
}

} // namespace

int main() {
    try {
        test_lexer_and_parser();
        test_truth_table();
        test_nnf();
        test_invalid_formulas();
        test_structured_parse_errors();
    } catch (const exception& error) {
        cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }

    cout << "All Worklet 1 tests passed.\n";
    return 0;
}
