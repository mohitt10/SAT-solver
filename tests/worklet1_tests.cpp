#include "nnf.h"
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

    NodePtr root = parseFormula("(A & ~B) -> C");
    require(dynamic_cast<ImpliesNode*>(root.get()) != nullptr,
            "parser should produce the shared visitor-based AST");
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

} // namespace

int main() {
    try {
        test_lexer_and_parser();
        test_truth_table();
        test_nnf();
        test_invalid_formulas();
    } catch (const exception& error) {
        cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }

    cout << "All Worklet 1 tests passed.\n";
    return 0;
}
