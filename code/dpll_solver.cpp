#include "sat_types.hpp"
#include <chrono>
#include <iostream>

using namespace std;
//v : Total number of unique variables in the formula.
//c : Number of clauses


// O(C+V) 
//assigned_lit is assumed to be marked true
CNF assign_literal(CNF current_cnf, Literal assigned_lit) {
    CNF simplified;

    vector<Clause> current_clauses = current_cnf.getClauses();
    
    for (int i = 0; i < current_clauses.size(); i++) {
        Clause clause = current_clauses[i];
        
        //True variable found
        if (clause.containsLiteral(assigned_lit)) {
            continue; 
        }

        // Otherwise remove the negated variable present in the clause
        Clause shrunk;
        vector<Literal> lits = clause.getLiterals();
        
        for (int j = 0; j < lits.size(); j++) {
            Literal lit = lits[j];
            if (!lit.isOpposite(assigned_lit)) {
                shrunk.addLiteral(lit);
            }
        }
        simplified.addClause(shrunk);
    }
    return simplified;
}

//O(C)
bool find_unit_literal(CNF cnf, Literal& out_literal) {
    vector<Clause> clauses = cnf.getClauses();
    
    for (int i = 0; i < clauses.size(); i++) {
        if (clauses[i].size() == 1) {
            out_literal = clauses[i].getLiterals()[0];
            return true;
        }
    }
    return false;
}

//O(C * L + V)
bool find_pure_literal(CNF cnf, Literal& out_literal) {
    unordered_map<string, int> polarity_map;
    
    vector<Clause> clauses = cnf.getClauses();
    for (int i = 0; i < clauses.size(); i++) {
        vector<Literal> lits = clauses[i].getLiterals();
        
        for (int j = 0; j < lits.size(); j++) {
            Literal lit = lits[j];
            string var_name = lit.getVariable();
            
            int current_polarity = 1;
            if (lit.isNegated()) {
                current_polarity = -1;
            }
            
            if (polarity_map.count(var_name) == 0) {
                polarity_map[var_name] = current_polarity;
            } else {
                if (polarity_map[var_name] != current_polarity) {
                    polarity_map[var_name] = 0; // Mixed polarity
                }
            }
        }
    }
    
    for (auto entry : polarity_map) {
        string var_name = entry.first;
        int polarity = entry.second;

        if (polarity != 0) {
            bool is_negated = false;
            if (polarity == -1) {
                is_negated = true;
            }
            out_literal = Literal(var_name, is_negated);
            return true; 
        }
    }
    
    return false;
}


// Recursive DPLL 
//O((2^V)*C)
bool dpll_recursive(CNF cnf, unordered_map<string, bool>& assignments, SolverConfig config, SolverResult& stats) {

    // 1.Base Case: Satisfied
    if (cnf.isEmpty()) {
        return true;
    }

    // 2.Base Case: Conflict
    vector<Clause> clauses = cnf.getClauses();
    for (int i = 0; i < clauses.size(); i++) {
        if (clauses[i].isEmpty()) {
            stats.backtrack_count++;
            return false; // Backtrack
        }
    }
a
    // current assignments
    unordered_map<string, bool> checkpoint = assignments;

    //Unit Propagation
    //O(V*C)
    Literal unit_lit;
    while (config.use_unit_prop && find_unit_literal(cnf, unit_lit)) {
        assignments[unit_lit.getVariable()] = !unit_lit.isNegated();
        cnf = assign_literal(cnf, unit_lit);
        
        if (cnf.isEmpty()) return true;
        
        vector<Clause> new_clauses = cnf.getClauses();
        for (int i = 0; i < new_clauses.size(); i++) {
            if (new_clauses[i].isEmpty()) { //If any clause is empty it means that all the literals are guesses false
                stats.backtrack_count++;
                assignments = checkpoint; // Undo
                return false; 
            }
        }
    }

    // Pure literal Elimination
    //O(V*C)
    Literal pure_lit;
    while (config.use_pure_literal && find_pure_literal(cnf, pure_lit)) {
        assignments[pure_lit.getVariable()] = !pure_lit.isNegated();
        cnf = assign_literal(cnf, pure_lit);
        
        if (cnf.isEmpty()) return true;
        
        vector<Clause> new_clauses = cnf.getClauses();
        for (int i = 0; i < new_clauses.size(); i++) {
            if (new_clauses[i].isEmpty()) {
                stats.backtrack_count++;
                assignments = checkpoint; //undo
                return false; 
            }
        }
    }

    //branching(Guess)
    stats.decision_count++;
    
    //Choose the first clause and first literal
    Clause first_clause = cnf.getClauses()[0];
    Literal branch_lit = first_clause.getLiterals()[0];
    string var_name = branch_lit.getVariable();

    // Guess True
    assignments[var_name] = true;
    Literal guess_true(var_name, false); 
    CNF cnf_if_true = assign_literal(cnf, guess_true);
    
    if (dpll_recursive(cnf_if_true, assignments, config, stats)) {
        return true;
    }

    // Guess False (Implicit Backtrack)
    assignments[var_name] = false;
    Literal guess_false(var_name, true);
    CNF cnf_if_false = assign_literal(cnf, guess_false);
    
    if (dpll_recursive(cnf_if_false, assignments, config, stats)) {
        return true;
    }

    // Total Failure: Undo and backtrack
    assignments = checkpoint;
    return false;
}


// solve function for Worklet 3
SolverResult solve(CNF initial_cnf, SolverConfig config) {
    SolverResult result;
    
    chrono::time_point<chrono::high_resolution_clock> start = chrono::high_resolution_clock::now();

    result.is_sat = dpll_recursive(initial_cnf, result.final_assignments, config, result);

    chrono::time_point<chrono::high_resolution_clock> end = chrono::high_resolution_clock::now();
    chrono::duration<double, milli> duration = end - start;
    result.total_execution_time_ms = duration.count();

    if (result.is_sat == false) {
        result.final_assignments.clear();
    }
    
    return result;
}