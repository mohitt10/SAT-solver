#include <iostream>
#include "sat_types.hpp"

using namespace std;

// Link the functions from your other module files
CNF convert_nnf_to_cnf(ASTNode* root);
SolverResult solve(CNF initial_cnf, SolverConfig config);

int main() {
    cout << "=== Propositional Logic & SAT Solver ===" << endl;

    // Formula: (A ^ B) V (~A ^ C)
    VarNode* a1 = new VarNode("A");
    VarNode* b1 = new VarNode("B");
    AndNode* left_and = new AndNode(a1, b1);

    VarNode* a2 = new VarNode("A");
    NotNode* not_a = new NotNode(a2);
    VarNode* c1 = new VarNode("C");
    AndNode* right_and = new AndNode(not_a, c1);

    OrNode* root = new OrNode(left_and, right_and);

    // 2. Convert to CNF
    CNF cnf_formula = convert_nnf_to_cnf(root);
    

    SolverConfig config;
    config.use_unit_prop = true;
    config.use_pure_literal = true;

    SolverResult result = solve(cnf_formula, config);

    cout << "\n--- SOLVER RESULTS ---" << endl;
    
    if (result.is_sat) {
        cout << "Result: SATISFIABLE" << endl;
        cout << "\nFinal Assignments:" << endl;
        
        for (auto entry : result.final_assignments) {
            string var_name = entry.first;
            bool is_true = entry.second;
            
            cout << "  " << var_name << " = ";
            if (is_true) {
                cout << "True" << endl;
            } else {
                cout << "False" << endl;
            }
        }
    } else {
        cout << "Result: UNSATISFIABLE" << endl;
    }

    cout << "\n----------------------" << endl;
    cout << "Execution Time : " << result.total_execution_time_ms << " ms" << endl;
    cout << "Decisions Made : " << result.decision_count << endl;
    cout << "Backtracks     : " << result.backtrack_count << endl;

    // Clean up memory
    delete a1; delete b1; delete left_and;
    delete a2; delete not_a; delete c1; delete right_and;
    delete root;

    return 0;
}