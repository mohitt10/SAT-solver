#include "ast.h"
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

// ---------------------------------------------------------------------------
// Worklet 1 Pipeline & Integration Tests
// (End-to-end integration: String -> Lexer -> Parser -> AST -> NNF -> TruthTable)
// ---------------------------------------------------------------------------

void test_pipeline_walkthrough() {
    cout << "Testing Worklet 1 pipeline walkthrough (Slide 1 Walk-through)...\n";

    // "NodePtr parsed = parseFormula(\"(A & ~B) -> C\");"
    // "NNFConverter converter;"
    // "NodePtr nnf = converter.convert(*parsed);"
    // "// Clean handoff to Worklet 2"
    NodePtr parsed = parseFormula("(A & ~B) -> C");
    require(parsed != nullptr, "parsed formula AST must not be null");

    NNFConverter converter;
    NodePtr nnf = converter.convert(*parsed);
    require(nnf != nullptr, "NNF converted AST must not be null");

    TruthTableEngine engine;
    require(engine.are_equivalent(*parsed, *nnf),
            "handoff NNF must be semantically equivalent to parsed AST");
}

void test_pipeline_roundtrip() {
    cout << "Testing Worklet 1 pipeline roundtrip on complex formulas...\n";

    const vector<string> formulas = {
        "P -> Q",
        "(P | Q) & (P -> R) & (Q -> R)",
        "(A & B) | (A & ~B) | (~A & B) | (~A & ~B)",
        "~(A <-> B) <-> (A <-> ~B)",
        "((p1 -> p2) & (p2 -> p3)) -> (p1 -> p3)"
    };

    TruthTableEngine engine;
    NNFConverter converter;

    for (const string& formula : formulas) {
        NodePtr parsed = parseFormula(formula);
        TruthTable table = engine.truth_table(*parsed);
        Classification c = engine.classify(table);

        NodePtr nnf = converter.convert(*parsed);
        TruthTable nnf_table = engine.truth_table(*nnf);
        Classification nnf_c = engine.classify(nnf_table);

        require(c == nnf_c, "classification must match after NNF conversion for: " + formula);
        require(engine.are_equivalent(*parsed, *nnf),
                "semantic equivalence must hold for: " + formula);
    }
}

} // namespace

int main() {
    cout << "========================================================\n";
    cout << "        WORKLET 1 - PIPELINE INTEGRATION TESTS          \n";
    cout << "========================================================\n";

    try {
        test_pipeline_walkthrough();
        test_pipeline_roundtrip();
    } catch (const exception& error) {
        cerr << "\n*** INTEGRATION TEST FAILURE ***\n" << error.what() << '\n';
        return 1;
    }

    cout << "All Worklet 1 integration tests passed successfully.\n";
    return 0;
}
