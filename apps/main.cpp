#include "nnf.h"
#include "parser.h"
#include "truth_table.h"
#include "sat_types.hpp"
#include <cctype>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

namespace {

vector<string> tokenize_stdin_input(const string& input) {
    vector<string> tokens;
    string current;
    bool in_quotes = false;

    for (char ch : input) {
        if (ch == '"') {
            in_quotes = !in_quotes;
            continue;
        }

        if (isspace(static_cast<unsigned char>(ch)) && !in_quotes) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            continue;
        }

        current.push_back(ch);
    }

    if (!current.empty()) {
        tokens.push_back(current);
    }

    return tokens;
}

string join_tokens(const vector<string>& tokens, size_t start_index) {
    string result;
    for (size_t index = start_index; index < tokens.size(); ++index) {
        if (index > start_index) {
            result += ' ';
        }
        result += tokens[index];
    }
    return result;
}

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

int main() {
    bool show_truth_table = false;
    CnfPipeline pipeline = CnfPipeline::Nnf;
    string formula;
    bool have_formula = false;

    string raw_input;
    string line;
    while (getline(cin, line)) {
        if (!raw_input.empty()) {
            raw_input += '\n';
        }
        raw_input += line;
    }

    const vector<string> arguments = tokenize_stdin_input(raw_input);
    for (size_t index = 0; index < arguments.size(); ++index) {
        const string& argument = arguments[index];
        if (argument == "--truth-table") {
            show_truth_table = true;
        } else if (argument == "--pipeline") {
            if (index + 1 >= arguments.size()) {
                cerr << "Option --pipeline requires nnf or tseitin.\n";
                return 2;
            }
            const string choice = arguments[++index];
            if (choice == "nnf") {
                pipeline = CnfPipeline::Nnf;
            } else if (choice == "tseitin") {
                pipeline = CnfPipeline::Tseitin;
            } else {
                cerr << "Unknown pipeline '" << choice
                     << "'; choose nnf or tseitin.\n";
                return 2;
            }
        } else if (!have_formula) {
            formula = join_tokens(arguments, index);
            have_formula = true;
            break;
        } else {
            cerr << "Only one formula argument is allowed.\n";
            return 2;
        }
    }

    if (!have_formula) {
        cerr << "Usage: provide the options and formula on stdin, for example:\n"
             << "  --truth-table --pipeline nnf \"(A & ~B) -> C\"\n";
        return 2;
    }

    try {
        NodePtr parsed = parseFormula(formula);

        if (show_truth_table) {
            TruthTableEngine engine;
            TruthTable table = engine.truth_table(*parsed);
            print_truth_table(cout, table);
            cout << "Classification: "
                      << to_string(engine.classify(table)) << '\n';
        }

        if (pipeline == CnfPipeline::Nnf) {
            NNFConverter converter;
            NodePtr nnf = converter.convert(*parsed);
            FormulaFormatter formatter;
            cout << "NNF: " << formatter.format(*nnf) << '\n';
        } else {
            cout << "CNF pipeline: Tseitin\n";
        }

        const CnfEncoding encoding = convert_formula_to_cnf(parsed, pipeline);
        SolverConfig config;
        SolverResult result = solve(encoding.cnf, config);

        cout << "SAT result: " << (result.is_sat ? "SAT" : "UNSAT") << '\n';
        if (result.is_sat) {
            cout << "Assignment:\n";
            for (const auto& assignment : result.final_assignments) {
                if (encoding.original_variables.count(assignment.first) != 0) {
                    cout << "  " << assignment.first << " = "
                         << (assignment.second ? "true" : "false") << '\n';
                }
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
