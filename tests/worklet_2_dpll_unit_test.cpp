#include "sat_types.hpp"

#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace std;

namespace {
Clause make_clause(const vector<pair<string, bool>>& literals) {
    Clause clause;
    for (const auto& item : literals) {
        clause.addLiteral(Literal(item.first, item.second));
    }
    return clause;
}

struct DpllTestCase {
    string name;
    CNF cnf;
    bool expected_sat;
    function<bool(const unordered_map<string, bool>&)> verify_assignment;
};

bool run_single_test(
    const DpllTestCase& test,
    bool use_unit_prop,
    bool use_pure_literal) {
    
    SolverConfig config;
    config.use_unit_prop = use_unit_prop;
    config.use_pure_literal = use_pure_literal;
    config.record_trace = false;

    try {
        SolverResult result = solve(test.cnf, config);

        if (result.is_sat != test.expected_sat) {
            cerr << "    Mismatch on is_sat! Expected: " 
                 << test.expected_sat << ", Got: " << result.is_sat << "\n";
            return false;
        }

        if (result.is_sat && test.verify_assignment) {
            if (!test.verify_assignment(result.final_assignments)) {
                cerr << "    Assignment validation failed!\n";
                return false;
            }
        }

        if (!result.is_sat && !result.final_assignments.empty()) {
            cerr << "    UNSAT result returned non-empty assignments map!\n";
            return false;
        }

        return true;
    } catch (const exception& error) {
        cerr << "    Exception caught: " << error.what() << "\n";
        return false;
    }
}

} // namespace

int main() {
    cout << "=== Pure DPLL Solver Unit Tests ===\n\n";

    vector<DpllTestCase> tests;

    // 1. Boundary Case: Empty CNF (Tautology / Trivially Satisfied)
    {
        CNF cnf;
        tests.push_back({
            "Boundary: Empty CNF formula",
            cnf,
            true,
            [](const auto&) { return true; }
        });
    }

    // 2. Boundary Case: Explicit Empty Clause (Immediate Conflict)
    {
        CNF cnf;
        cnf.addClause(Clause()); // Empty clause: []
        tests.push_back({
            "Boundary: Explicit empty clause",
            cnf,
            false,
            nullptr
        });
    }

    // 3. Single Positive Unit Clause: (A)
    {
        CNF cnf;
        cnf.addClause(make_clause({{"A", false}}));
        tests.push_back({
            "Unit Clause: Positive (A)",
            cnf,
            true,
            [](const auto& assign) { return assign.at("A") == true; }
        });
    }

    // 4. Single Negative Unit Clause: (~A)
    {
        CNF cnf;
        cnf.addClause(make_clause({{"A", true}}));
        tests.push_back({
            "Unit Clause: Negative (~A)",
            cnf,
            true,
            [](const auto& assign) { return assign.at("A") == false; }
        });
    }

    // 5. Direct Unit Contradiction: (A) AND (~A)
    {
        CNF cnf;
        cnf.addClause(make_clause({{"A", false}}));
        cnf.addClause(make_clause({{"A", true}}));
        tests.push_back({
            "Direct Contradiction: (A) AND (~A)",
            cnf,
            false,
            nullptr
        });
    }

    // 6. Unit Propagation Domino Cascade: (A) AND (~A v B) AND (~B v C) AND (~C)
    {
        CNF cnf;
        cnf.addClause(make_clause({{"A", false}}));                // A
        cnf.addClause(make_clause({{"A", true}, {"B", false}}));  // ~A v B
        cnf.addClause(make_clause({{"B", true}, {"C", false}}));  // ~B v C
        cnf.addClause(make_clause({{"C", true}}));                 // ~C
        tests.push_back({
            "Cascade: Multi-step unit propagation conflict",
            cnf,
            false,
            nullptr
        });
    }

    // 7. Pure Literal Elimination: (A v B) AND (A v ~B) -> A is pure positive
    {
        CNF cnf;
        cnf.addClause(make_clause({{"A", false}, {"B", false}})); // A v B
        cnf.addClause(make_clause({{"A", false}, {"B", true}}));  // A v ~B
        tests.push_back({
            "Pure Literal: Positive literal A",
            cnf,
            true,
            [](const auto& assign) { return assign.at("A") == true; }
        });
    }

    // 8. Full 2-Variable Exhaustive Search (UNSAT):
    // (A v B) AND (A v ~B) AND (~A v B) AND (~A v ~B)
    {
        CNF cnf;
        cnf.addClause(make_clause({{"A", false}, {"B", false}}));
        cnf.addClause(make_clause({{"A", false}, {"B", true}}));
        cnf.addClause(make_clause({{"A", true}, {"B", false}}));
        cnf.addClause(make_clause({{"A", true}, {"B", true}}));
        tests.push_back({
            "Branching: All 2-variable assignments blocked",
            cnf,
            false,
            nullptr
        });
    }

    // 9. Forced Decision & Backtrack:
    // (~A v B) AND (~A v ~B) AND (A v C)
    // Guessing A=True fails on B, forces backtrack to A=False, requiring C=True
    {
        CNF cnf;
        cnf.addClause(make_clause({{"A", true}, {"B", false}}));  // ~A v B
        cnf.addClause(make_clause({{"A", true}, {"B", true}}));   // ~A v ~B
        cnf.addClause(make_clause({{"A", false}, {"C", false}})); // A v C
        tests.push_back({
            "Backtrack: Recovery after initial decision failure",
            cnf,
            true,
            [](const auto& assign) { 
                return assign.at("A") == false && assign.at("C") == true; 
            }
        });
    }

    const vector<pair<bool, bool>> configs = {
        {false, false},
        {false, true},
        {true, false},
        {true, true}
    };

    const vector<string> config_labels = {
        "unit=off, pure=off",
        "unit=off, pure=on",
        "unit=on, pure=off",
        "unit=on, pure=on"
    };

    size_t total_checks = 0;
    size_t failures = 0;

    for (const auto& test : tests) {
        for (size_t i = 0; i < configs.size(); ++i) {
            bool passed = run_single_test(test, configs[i].first, configs[i].second);
            
            cout << (passed ? "[PASS] " : "[FAIL] ") 
                 << test.name << " (" << config_labels[i] << ")\n";
            
            total_checks++;
            if (!passed) {
                failures++;
            }
        }
    }

    cout << "Total Checks: " << total_checks << " , Failures: " << failures << "\n";
    if (failures == 0) {
        cout << "ALL PURE DPLL UNIT TESTS PASSED SUCCESSFULLY!\n";
    } else {
        cout << "SOME DPLL UNIT TESTS FAILED!\n";
    }

    return failures == 0 ? 0 : 1;
}