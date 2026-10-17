#include "nnf.h"
#include "parser.h"
#include "truth_table.h"
#include "code/sat_types.hpp"
#include <iostream>
#include <stdexcept>
#include <string>

using namespace std;

namespace {

class FormulaFormatter : public Visitor {
public:
    string format(const ASTNode& node) {
        node.accept(*this);
        return result_;
    }

    void visit(const VarNode& node) override { result_ = node.name; }

    void visit(const NotNode& node) override {
        result_ = "~(" + child(node.left) + ")";
    }

    void visit(const AndNode& node) override { binary(node, " & "); }
    void visit(const OrNode& node) override { binary(node, " | "); }
    void visit(const ImpliesNode& node) override { binary(node, " -> "); }
    void visit(const IffNode& node) override { binary(node, " <-> "); }

private:
    string result_;

    static string child(const NodePtr& node) {
        if (!node) {
            throw invalid_argument("malformed AST: missing child");
        }
        FormulaFormatter formatter;
        return formatter.format(*node);
    }

    void binary(const ASTNode& node, const string& op) {
        result_ = "(" + child(node.left) + op + child(node.right) + ")";
    }
};

} // namespace

int main(int argc, char* argv[]) {
    bool show_truth_table = false;
    int formula_index = 1;

    if (argc > 1 && string(argv[1]) == "--truth-table") {
        show_truth_table = true;
        formula_index = 2;
    }

    if (argc != formula_index + 1) {
        cerr << "Usage: " << argv[0]
                  << " [--truth-table] \"FORMULA\"\n";
        return 2;
    }

    try {
        NodePtr parsed = parseFormula(argv[formula_index]);

        if (show_truth_table) {
            TruthTableEngine engine;
            TruthTable table = engine.truth_table(*parsed);
            print_truth_table(cout, table);
            cout << "Classification: "
                      << to_string(engine.classify(table)) << '\n';
        }

        NNFConverter converter;
        NodePtr nnf = converter.convert(*parsed);
        FormulaFormatter formatter;
        cout << "NNF: " << formatter.format(*nnf) << '\n';

        CNF cnf = convert_nnf_to_cnf(nnf);
        SolverConfig config;
        SolverResult result = solve(cnf, config);

        cout << "SAT result: " << (result.is_sat ? "SAT" : "UNSAT") << '\n';
        if (result.is_sat) {
            cout << "Assignment:\n";
            for (const auto& assignment : result.final_assignments) {
                cout << "  " << assignment.first << " = "
                     << (assignment.second ? "true" : "false") << '\n';
            }
        }
        cout << "Decisions: " << result.decision_count << '\n'
             << "Backtracks: " << result.backtrack_count << '\n'
             << "Solver time: " << result.total_execution_time_ms << " ms\n";
    } catch (const exception& error) {
        cerr << "Error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
