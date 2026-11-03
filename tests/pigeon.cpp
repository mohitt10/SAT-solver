#include "sat_types.hpp"
#include <iostream>
#include <string>

using namespace std;

// Generates a Pigeonhole Principle (PHP) CNF formula
CNF generate_php_cnf(int pigeons, int holes) {
    CNF cnf;

    // Constraint 1: Every pigeon must be placed in at least one hole.
    for (int p = 1; p <= pigeons; ++p) {
        Clause c;
        for (int h = 1; h <= holes; ++h) {
            string var_name = "P_" + to_string(p) + "_" + to_string(h);
            c.addLiteral(Literal(var_name, false)); 
        }
        cnf.addClause(c);
    }

    // Constraint 2: No two pigeons can occupy the same hole.
    for (int h = 1; h <= holes; ++h) {
        for (int p1 = 1; p1 <= pigeons; ++p1) {
            for (int p2 = p1 + 1; p2 <= pigeons; ++p2) {
                Clause c;
                string var1 = "P_" + to_string(p1) + "_" + to_string(h);
                string var2 = "P_" + to_string(p2) + "_" + to_string(h);
                c.addLiteral(Literal(var1, true)); // negated
                c.addLiteral(Literal(var2, true)); // negated
                cnf.addClause(c);
            }
        }
    }

    return cnf;
}

int main() {
    int pigeons, holes;
    cout << "Enter number of pigeons: ";
    if (!(cin >> pigeons) || pigeons <= 0) {
        cerr << "Error: Pigeons must be a positive integer." << endl;
        return 1;
    }

    cout << "Enter number of holes: ";
    if (!(cin >> holes) || holes <= 0) {
        cerr << "Error: Holes must be a positive integer." << endl;
        return 1;
    }

    if (pigeons > 9 && pigeons > holes) {
        cout << "\n[WARNING]: Running pure DPLL on PHP with > 9 pigeons can take minutes or hours to finish." << endl;
    }

    cout << "\nGenerating PHP(" << pigeons << ", " << holes << ") CNF formula" << endl;
    CNF php_cnf = generate_php_cnf(pigeons, holes);

    cout << "Formula Generated:" << endl;
    cout << "Total Variables: " << (pigeons * holes) << endl;
    cout << "Total Clauses: " << php_cnf.size() << endl;

    // Configure solver options
    SolverConfig config;
    config.use_unit_prop = true;
    config.use_pure_literal = false; // Disabled to eliminate heap allocation overhead
    config.record_trace = false;    // Disabled to prevent memory overhead

    cout << "\nStarting DPLL Solver..." << endl;
    SolverResult result = solve(php_cnf, config);

    cout << "\nSOLVER RESULTS:-" << endl;
    cout << "Satisfiable: " << (result.is_sat ? "True" : "False (UNSAT)") << endl;
    cout << "Execution Time: " << result.total_execution_time_ms << " ms" << endl;
    cout << "Decisions: " << result.decision_count << endl;
    cout << "Backtracks: " << result.backtrack_count << endl;

    return 0;
}