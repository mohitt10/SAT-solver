#include "ast.h"
#include "lexer.h"
#include "nnf.h"
#include "parse_error.h"
#include "parser.h"
#include "truth_table.h"

#include <algorithm>
#include <array>
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

namespace {

// Helper to validate that an AST strictly complies with Negation Normal Form (NNF):
// 1. Only VarNode, NotNode, AndNode, OrNode are present.
// 2. NotNode can only wrap a VarNode.
// 3. Binary nodes have both left and right children non-null.
class StrictNNFValidator : public Visitor {
public:
    void validate(const ASTNode& node) {
        node.accept(*this);
    }

    void visit(const VarNode&) override {}

    void visit(const NotNode& node) override {
        if (!node.left || node.right != nullptr) {
            throw runtime_error("NNF NotNode must have left child and null right child");
        }
        if (dynamic_cast<const VarNode*>(node.left.get()) == nullptr) {
            throw runtime_error("NNF NotNode operand must be a VarNode (negation pushed to literals)");
        }
    }

    void visit(const AndNode& node) override {
        validate_binary(node);
    }

    void visit(const OrNode& node) override {
        validate_binary(node);
    }

    void visit(const ImpliesNode&) override {
        throw runtime_error("NNF AST must not contain ImpliesNode");
    }

    void visit(const IffNode&) override {
        throw runtime_error("NNF AST must not contain IffNode");
    }

private:
    void validate_binary(const ASTNode& node) {
        if (!node.left || !node.right) {
            throw runtime_error("Binary AST node missing left or right child");
        }
        node.left->accept(*this);
        node.right->accept(*this);
    }
};

// -----------------------------------------------------------------------------
// Test Suite 1: Full Parse -> NNF -> Truth Table Semantic Equivalence Pipeline
// -----------------------------------------------------------------------------
struct EquivalenceTestCase {
    string name;
    string formula;
};

bool test_nnf_equivalence_pipeline(size_t& checks, size_t& failures) {
    cout << "\n=== Suite 1: Parse -> NNF -> Truth Table Semantic Equivalence ===\n";

    const vector<EquivalenceTestCase> test_cases = {
        {"Simple Implication", "A -> B"},
        {"Negated Implication", "~(A -> B)"},
        {"Simple Biconditional", "A <-> B"},
        {"Negated Biconditional (XOR)", "~(A <-> B)"},
        {"De Morgan Conjunction", "~(A & B)"},
        {"De Morgan Disjunction", "~(A | B)"},
        {"Nested De Morgan", "~((A & B) | (~C & D))"},
        {"Double Negation Elimination", "~~(A | B)"},
        {"Quadruple Negation", "~~~~(A & ~B)"},
        {"Hypothetical Syllogism Antecedent", "(A -> B) & (B -> C)"},
        {"Complex Distributive Form", "(A | B) & (C | D)"},
        {"Nested Biconditionals", "(A <-> B) <-> (C <-> D)"},
        {"Mixed Word Operators & Alphanumerics", "(var1_in and 2nd_var) or (~out_3 and var4)"},
        {"Implication with Disjunction", "(A | B) -> (C & D)"},
        {"Exportation Law Component", "((A & B) -> C) <-> (A -> (B -> C))"}
    };

    TruthTableEngine engine;
    NNFConverter converter;
    StrictNNFValidator validator;

    bool suite_passed = true;
    for (const auto& tc : test_cases) {
        ++checks;
        try {
            // 1. Text -> Lexer -> Parser -> AST
            NodePtr parsed = parseFormula(tc.formula);
            if (!parsed) {
                throw runtime_error("parseFormula returned null AST");
            }

            // 2. AST -> NNFConverter -> NNF AST
            NodePtr nnf = converter.convert(*parsed);
            if (!nnf) {
                throw runtime_error("NNFConverter returned null AST");
            }

            // 3. Strict NNF structural verification
            validator.validate(*nnf);

            // 4. Semantic equivalence verification over truth tables
            if (!engine.are_equivalent(*parsed, *nnf)) {
                throw runtime_error("Original AST and NNF AST are not semantically equivalent");
            }

            // 5. Compare truth table results bit by bit
            TruthTable tt_orig = engine.truth_table(*parsed);
            TruthTable tt_nnf = engine.truth_table(*nnf);
            if (tt_orig.results != tt_nnf.results) {
                throw runtime_error("TruthTable bitvectors differ between parsed and NNF AST");
            }

            cout << "  [PASS] " << tc.name << ": " << tc.formula << "\n";
        } catch (const exception& e) {
            cout << "  [FAIL] " << tc.name << " (" << tc.formula << "): " << e.what() << "\n";
            ++failures;
            suite_passed = false;
        }
    }
    return suite_passed;
}

// -----------------------------------------------------------------------------
// Test Suite 2: Formula -> AST -> Truth Table Classification
// -----------------------------------------------------------------------------
struct ClassificationTestCase {
    string name;
    string formula;
    Classification expected;
};

bool test_classification_pipeline(size_t& checks, size_t& failures) {
    cout << "\n=== Suite 2: End-to-End Classification (Tautology / Contradiction / Contingency) ===\n";

    const vector<ClassificationTestCase> test_cases = {
        // Tautologies
        {"Law of Excluded Middle", "A | ~A", Classification::TAUTOLOGY},
        {"Modus Ponens", "((P -> Q) & P) -> Q", Classification::TAUTOLOGY},
        {"Modus Tollens", "((P -> Q) & ~Q) -> ~P", Classification::TAUTOLOGY},
        {"Hypothetical Syllogism", "((P -> Q) & (Q -> R)) -> (P -> R)", Classification::TAUTOLOGY},
        {"Peirce's Law", "((P -> Q) -> P) -> P", Classification::TAUTOLOGY},
        {"De Morgan Tautology 1", "~(P & Q) <-> (~P | ~Q)", Classification::TAUTOLOGY},
        {"De Morgan Tautology 2", "~(P | Q) <-> (~P & ~Q)", Classification::TAUTOLOGY},
        {"Contraposition", "(P -> Q) <-> (~Q -> ~P)", Classification::TAUTOLOGY},
        {"Double Negation Law", "~~P <-> P", Classification::TAUTOLOGY},
        {"Material Implication Equivalence", "(P -> Q) <-> (~P | Q)", Classification::TAUTOLOGY},

        // Contradictions
        {"Direct Contradiction", "A & ~A", Classification::CONTRADICTION},
        {"Implication Violation", "(A -> B) & (A & ~B)", Classification::CONTRADICTION},
        {"Inconsistent Triad", "(A | B) & (~A | B) & ~B", Classification::CONTRADICTION},
        {"Contradictory Biconditionals", "(A <-> B) & (A <-> ~B)", Classification::CONTRADICTION},
        {"Negated Tautology", "~(((P -> Q) & P) -> Q)", Classification::CONTRADICTION},

        // Contingencies
        {"Single Variable", "A", Classification::CONTINGENCY},
        {"Single Implication", "A -> B", Classification::CONTINGENCY},
        {"Single Conjunction", "A & B", Classification::CONTINGENCY},
        {"Single Disjunction", "A | B", Classification::CONTINGENCY},
        {"Exclusive OR (XOR)", "(A | B) & ~(A & B)", Classification::CONTINGENCY},
        {"Majority Vote (3 vars)", "(A & B) | (B & C) | (A & C)", Classification::CONTINGENCY},
        {"Four-variable Contingency", "(A & B) -> (C | D)", Classification::CONTINGENCY}
    };

    TruthTableEngine engine;
    bool suite_passed = true;

    for (const auto& tc : test_cases) {
        ++checks;
        try {
            NodePtr ast = parseFormula(tc.formula);
            TruthTable tt = engine.truth_table(*ast);
            Classification actual = engine.classify(tt);

            if (actual != tc.expected) {
                throw runtime_error("Expected " + to_string(tc.expected) + ", got " + to_string(actual));
            }

            cout << "  [PASS] " << tc.name << " (" << to_string(actual) << "): " << tc.formula << "\n";
        } catch (const exception& e) {
            cout << "  [FAIL] " << tc.name << " (" << tc.formula << "): " << e.what() << "\n";
            ++failures;
            suite_passed = false;
        }
    }
    return suite_passed;
}

// -----------------------------------------------------------------------------
// Test Suite 3: Operator Precedence & Associativity Integration
// -----------------------------------------------------------------------------
struct PrecedenceTestCase {
    string name;
    string unparenthesized;
    string parenthesized_equivalent;
    string incorrect_grouping;
};

bool test_precedence_and_associativity(size_t& checks, size_t& failures) {
    cout << "\n=== Suite 3: Operator Precedence & Associativity Integration ===\n";

    const vector<PrecedenceTestCase> test_cases = {
        {
            "NOT binds tighter than AND",
            "~A & B",
            "(~A) & B",
            "~(A & B)"
        },
        {
            "NOT binds tighter than OR",
            "~A | B",
            "(~A) | B",
            "~(A | B)"
        },
        {
            "AND binds tighter than OR",
            "A & B | C",
            "(A & B) | C",
            "A & (B | C)"
        },
        {
            "OR binds tighter than IMPLIES",
            "A | B -> C",
            "(A | B) -> C",
            "A | (B -> C)"
        },
        {
            "IMPLIES binds tighter than IFF",
            "A -> B <-> C",
            "(A -> B) <-> C",
            "A -> (B <-> C)"
        },
        {
            "IMPLIES is right-associative",
            "A -> B -> C",
            "A -> (B -> C)",
            "(A -> B) -> C"
        },
        {
            "Word operators respect precedence",
            "A and B or C",
            "(A & B) | C",
            "A & (B | C)"
        }
    };

    TruthTableEngine engine;
    bool suite_passed = true;

    for (const auto& tc : test_cases) {
        ++checks;
        try {
            NodePtr ast_unparenthesized = parseFormula(tc.unparenthesized);
            NodePtr ast_correct = parseFormula(tc.parenthesized_equivalent);
            NodePtr ast_incorrect = parseFormula(tc.incorrect_grouping);

            // The unparenthesized formula must be semantically equivalent to the expected grouping
            if (!engine.are_equivalent(*ast_unparenthesized, *ast_correct)) {
                throw runtime_error("'" + tc.unparenthesized + "' is not equivalent to '" +
                                   tc.parenthesized_equivalent + "'");
            }

            // The unparenthesized formula must NOT be equivalent to the incorrect grouping
            if (engine.are_equivalent(*ast_unparenthesized, *ast_incorrect)) {
                throw runtime_error("Precedence ambiguity: '" + tc.unparenthesized +
                                   "' unexpectedly matched '" + tc.incorrect_grouping + "'");
            }

            cout << "  [PASS] " << tc.name << ": '" << tc.unparenthesized << "' == '"
                 << tc.parenthesized_equivalent << "'\n";
        } catch (const exception& e) {
            cout << "  [FAIL] " << tc.name << ": " << e.what() << "\n";
            ++failures;
            suite_passed = false;
        }
    }
    return suite_passed;
}

// -----------------------------------------------------------------------------
// Test Suite 4: Stepwise Pipeline Handoff (Tokenize -> InfixToPostfix -> BuildAST -> Evaluate)
// -----------------------------------------------------------------------------
bool test_stepwise_pipeline_handoff(size_t& checks, size_t& failures) {
    cout << "\n=== Suite 4: Stepwise Subsystem Pipeline Handoff ===\n";

    bool suite_passed = true;
    ++checks;
    try {
        const string formula = "(A & B) -> C";

        // Step 1: Lexer tokenize
        vector<Token> tokens = tokenize(formula);
        if (tokens.size() != 7) {
            throw runtime_error("Expected 7 tokens for '(A & B) -> C', got " + to_string(tokens.size()));
        }

        // Step 2: Parser infixToPostfix (Shunting-Yard)
        vector<Token> postfix = infixToPostfix(tokens);
        // Postfix order should be: A, B, &, C, ->
        if (postfix.size() != 5) {
            throw runtime_error("Expected 5 postfix tokens, got " + to_string(postfix.size()));
        }
        if (postfix[0].value != "A" || postfix[1].value != "B" ||
            postfix[2].type != AND || postfix[3].value != "C" || postfix[4].type != IMPLIES) {
            throw runtime_error("Postfix tokens order mismatch for '(A & B) -> C'");
        }

        // Step 3: buildAST from postfix
        NodePtr ast = buildAST(postfix);
        if (!ast) {
            throw runtime_error("buildAST produced null AST");
        }

        // Step 4: Evaluate with TruthTableEngine on specific assignments
        TruthTableEngine engine;
        // Case 1: A=T, B=T, C=F => (T & T) -> F => False
        Assignment a1 = {{"A", true}, {"B", true}, {"C", false}};
        if (engine.evaluate(*ast, a1) != false) {
            throw runtime_error("Evaluation failed: A=T, B=T, C=F should yield false");
        }

        // Case 2: A=T, B=F, C=F => (T & F) -> F => True
        Assignment a2 = {{"A", true}, {"B", false}, {"C", false}};
        if (engine.evaluate(*ast, a2) != true) {
            throw runtime_error("Evaluation failed: A=T, B=F, C=F should yield true");
        }

        // Case 3: A=T, B=T, C=T => True
        Assignment a3 = {{"A", true}, {"B", true}, {"C", true}};
        if (engine.evaluate(*ast, a3) != true) {
            throw runtime_error("Evaluation failed: A=T, B=T, C=T should yield true");
        }

        cout << "  [PASS] Stepwise handoff verified: tokenize -> infixToPostfix -> buildAST -> evaluate\n";
    } catch (const exception& e) {
        cout << "  [FAIL] Stepwise handoff: " << e.what() << "\n";
        ++failures;
        suite_passed = false;
    }
    return suite_passed;
}

// -----------------------------------------------------------------------------
// Test Suite 5: Error Propagation and Exception Boundary Integration
// -----------------------------------------------------------------------------
struct ErrorTestCase {
    string name;
    string formula;
    ErrorCategory expected_category;
    ErrorKind expected_kind;
};

bool test_error_handling_integration(size_t& checks, size_t& failures) {
    cout << "\n=== Suite 5: Error Propagation and Boundary Integration ===\n";

    const vector<ErrorTestCase> test_cases = {
        {"Invalid Character", "A & #", ErrorCategory::LEXICAL, ErrorKind::INVALID_CHARACTER},
        {"Invalid Token Operator", "A - B", ErrorCategory::LEXICAL, ErrorKind::INVALID_TOKEN},
        {"Missing Operand At Operator", "A & | B", ErrorCategory::SYNTAX, ErrorKind::MISSING_OPERAND},
        {"Missing Operator Between Variables", "A B", ErrorCategory::SYNTAX, ErrorKind::MISSING_OPERATOR},
        {"Missing Closing Parenthesis", "(A | B", ErrorCategory::SYNTAX, ErrorKind::MISSING_CLOSING_PAREN},
        {"Unexpected Closing Parenthesis", "A | B)", ErrorCategory::SYNTAX, ErrorKind::UNEXPECTED_CLOSING_PAREN},
        {"Unexpected End of Input", "A &", ErrorCategory::SYNTAX, ErrorKind::UNEXPECTED_END_OF_INPUT},
        {"Empty Formula String", "", ErrorCategory::SYNTAX, ErrorKind::UNEXPECTED_END_OF_INPUT}
    };

    bool suite_passed = true;
    for (const auto& tc : test_cases) {
        ++checks;
        try {
            (void)parseFormula(tc.formula);
            cout << "  [FAIL] " << tc.name << ": Expected ParseError for '" << tc.formula << "' but succeeded\n";
            ++failures;
            suite_passed = false;
        } catch (const ParseError& err) {
            if (err.category == tc.expected_category && err.kind == tc.expected_kind) {
                cout << "  [PASS] " << tc.name << " correctly caught ParseError (line="
                     << err.line << ", col=" << err.column << "): " << err.what() << "\n";
            } else {
                cout << "  [FAIL] " << tc.name << ": Wrong error category/kind\n";
                ++failures;
                suite_passed = false;
            }
        } catch (const exception& e) {
            cout << "  [FAIL] " << tc.name << ": Caught non-ParseError exception: " << e.what() << "\n";
            ++failures;
            suite_passed = false;
        }
    }

    // Boundary Test: Unbound variable evaluation throws runtime_error
    ++checks;
    try {
        NodePtr ast = parseFormula("A & B");
        TruthTableEngine engine;
        Assignment partial = {{"A", true}}; // "B" is missing
        (void)engine.evaluate(*ast, partial);
        cout << "  [FAIL] Unbound variable evaluation should have thrown runtime_error\n";
        ++failures;
        suite_passed = false;
    } catch (const runtime_error&) {
        cout << "  [PASS] Unbound variable access correctly caught runtime_error\n";
    } catch (...) {
        cout << "  [FAIL] Unbound variable threw unexpected exception type\n";
        ++failures;
        suite_passed = false;
    }

    // Boundary Test: Max variables guard in TruthTableEngine throws length_error
    ++checks;
    try {
        TruthTableEngine small_engine(3); // allow max 3 vars
        NodePtr four_var_ast = parseFormula("A & B & C & D");
        (void)small_engine.truth_table(*four_var_ast);
        cout << "  [FAIL] Exceeding max_vars should have thrown std::length_error\n";
        ++failures;
        suite_passed = false;
    } catch (const length_error&) {
        cout << "  [PASS] Exceeding max_vars correctly threw std::length_error\n";
    } catch (...) {
        cout << "  [FAIL] Exceeding max_vars threw unexpected exception type\n";
        ++failures;
        suite_passed = false;
    }

    return suite_passed;
}

// -----------------------------------------------------------------------------
// Test Suite 6: Multi-Variable Bit-Parallel Engine Scaling Integration
// -----------------------------------------------------------------------------
bool test_multi_variable_scaling(size_t& checks, size_t& failures) {
    cout << "\n=== Suite 6: Multi-Variable Bit-Parallel Scaling (64-bit boundaries) ===\n";

    bool suite_passed = true;
    TruthTableEngine engine;
    NNFConverter converter;
    StrictNNFValidator validator;

    // 8 variables => 256 rows = 4 64-bit words
    ++checks;
    try {
        const string eight_vars = "(v1 | v2) & (v3 | v4) & (v5 | v6) & (v7 | v8)";
        NodePtr parsed = parseFormula(eight_vars);
        vector<string> vars = engine.collect_variables(*parsed);

        if (vars.size() != 8) {
            throw runtime_error("Expected 8 variables collected, got " + to_string(vars.size()));
        }

        // Verify left-to-right variable ordering
        vector<string> expected_order = {"v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8"};
        if (vars != expected_order) {
            throw runtime_error("Variable ordering does not match appearance order");
        }

        TruthTable tt = engine.truth_table(*parsed);
        if (tt.num_rows() != 256) {
            throw runtime_error("Expected 256 rows, got " + to_string(tt.num_rows()));
        }

        // In (v1|v2) & (v3|v4) & (v5|v6) & (v7|v8), each pair is true in 3 out of 4 cases.
        // Therefore, satisfying rows = 3^4 = 81.
        uint64_t ones = tt.results.count_ones();
        if (ones != 81) {
            throw runtime_error("Expected exactly 81 satisfying rows (3^4), got " + to_string(ones));
        }

        // NNF conversion and equivalence across all 256 rows
        NodePtr nnf = converter.convert(*parsed);
        validator.validate(*nnf);
        if (!engine.are_equivalent(*parsed, *nnf)) {
            throw runtime_error("NNF equivalence failed on 8-variable formula");
        }

        cout << "  [PASS] 8-variable formula (256 rows / 4 words): exactly 81/256 satisfying rows, NNF verified\n";
    } catch (const exception& e) {
        cout << "  [FAIL] 8-variable scaling test: " << e.what() << "\n";
        ++failures;
        suite_passed = false;
    }

    // 10 variables => 1024 rows = 16 64-bit words
    ++checks;
    try {
        const string ten_vars = "(a & b & c & d & e) -> (f | g | h | i | j)";
        NodePtr parsed = parseFormula(ten_vars);
        TruthTable tt = engine.truth_table(*parsed);

        if (tt.num_rows() != 1024) {
            throw runtime_error("Expected 1024 rows, got " + to_string(tt.num_rows()));
        }

        // Implication is false ONLY when antecedent is true and consequent is false.
        // Antecedent (a&b&c&d&e) is true in 2^5 = 32 rows (when all 5 are 1).
        // Consequent (f|g|h|i|j) is false in exactly 1 row out of 32 (when all 5 are 0).
        // So antecedent=T and consequent=F occurs in exactly 1 row!
        // Therefore satisfying rows = 1024 - 1 = 1023 rows.
        uint64_t ones = tt.results.count_ones();
        if (ones != 1023) {
            throw runtime_error("Expected exactly 1023 satisfying rows (1024 - 1), got " + to_string(ones));
        }

        NodePtr nnf = converter.convert(*parsed);
        validator.validate(*nnf);
        if (!engine.are_equivalent(*parsed, *nnf)) {
            throw runtime_error("NNF equivalence failed on 10-variable formula");
        }

        cout << "  [PASS] 10-variable formula (1024 rows / 16 words): exactly 1023/1024 satisfying rows, NNF verified\n";
    } catch (const exception& e) {
        cout << "  [FAIL] 10-variable scaling test: " << e.what() << "\n";
        ++failures;
        suite_passed = false;
    }

    return suite_passed;
}

} // namespace

int main() {
    cout << "========================================================\n";
    cout << "       WORKLET 1 INTEGRATION TEST SUITE\n";
    cout << "========================================================\n";

    size_t checks = 0;
    size_t failures = 0;

    test_nnf_equivalence_pipeline(checks, failures);
    test_classification_pipeline(checks, failures);
    test_precedence_and_associativity(checks, failures);
    test_stepwise_pipeline_handoff(checks, failures);
    test_error_handling_integration(checks, failures);
    test_multi_variable_scaling(checks, failures);

    cout << "\n========================================================\n";
    cout << "Summary: " << checks << " checks executed, " << failures << " failures.\n";
    if (failures == 0) {
        cout << "STATUS: ALL WORKLET 1 INTEGRATION TESTS PASSED!\n";
        cout << "========================================================\n";
        return 0;
    } else {
        cout << "STATUS: SOME WORKLET 1 INTEGRATION TESTS FAILED!\n";
        cout << "========================================================\n";
        return 1;
    }
}

