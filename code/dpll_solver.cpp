#include "sat_types.hpp"
#include <chrono>
#include <iostream>

using namespace std;

// ============================================================================
// DPLL State Simplification
// ============================================================================

CNF assign_literal(CNF current_cnf, Literal assigned_lit) {
    CNF simplified;

    vector<Clause> current_clauses = current_cnf.getClauses();
    
    for (int i = 0; i < current_clauses.size(); i++) {
        Clause clause = current_clauses[i];
        
        // If the clause contains our true literal, the whole clause is true. Skip it.
        if (clause.containsLiteral(assigned_lit)) {
            continue; 
        }

        // Otherwise, copy the clause but remove the opposite (false) literal.
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

bool find_unit_literal(CNF cnf, Literal& out_literal) {
    vector<Clause> clauses = cnf.getClauses();
    
    for (int i = 0; i < clauses.size(); i++) {
        if (clauses[i].size() == 1) {
            // Get the 0th index directly instead of iterators
            out_literal = clauses[i].getLiterals()[0];
            return true;
        }
    }
    return false;
}

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
            
            // Using .count() just like in your datamodel.h file
            if (polarity_map.count(var_name) == 0) {
                polarity_map[var_name] = current_polarity;
            } else {
                if (polarity_map[var_name] != current_polarity) {
                    polarity_map[var_name] = 0; // Mixed polarity
                }
            }
        }
    }
    
    // Auto loop used here just like cart_main.cpp uses it for unordered_maps
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

// ============================================================================
// Recursive DPLL Engine
// ============================================================================

bool dpll_recursive(CNF cnf, unordered_map<string, bool>& assignments, SolverConfig config, SolverResult& stats) {

    // 1. Base Case: Satisfied
    if (cnf.isEmpty()) {
        return true;
    }

    // 2. Base Case: Conflict
    vector<Clause> clauses = cnf.getClauses();
    for (int i = 0; i < clauses.size(); i++) {
        if (clauses[i].isEmpty()) {
            stats.backtrack_count++;
            return false; // Backtrack!
        }
    }

    // Save our current assignments so we can undo them if a branch fails
    unordered_map<string, bool> checkpoint = assignments;

    // --- UNIT PROPAGATION ---
    Literal unit_lit;
    while (config.use_unit_prop && find_unit_literal(cnf, unit_lit)) {
        assignments[unit_lit.getVariable()] = !unit_lit.isNegated();
        cnf = assign_literal(cnf, unit_lit);
        
        if (cnf.isEmpty()) return true;
        
        vector<Clause> new_clauses = cnf.getClauses();
        for (int i = 0; i < new_clauses.size(); i++) {
            if (new_clauses[i].isEmpty()) {
                stats.backtrack_count++;
                assignments = checkpoint; // Undo
                return false; 
            }
        }
    }

    // --- PURE LITERAL ELIMINATION ---
    Literal pure_lit;
    while (config.use_pure_literal && find_pure_literal(cnf, pure_lit)) {
        assignments[pure_lit.getVariable()] = !pure_lit.isNegated();
        cnf = assign_literal(cnf, pure_lit);
        
        if (cnf.isEmpty()) return true;
        
        vector<Clause> new_clauses = cnf.getClauses();
        for (int i = 0; i < new_clauses.size(); i++) {
            if (new_clauses[i].isEmpty()) {
                stats.backtrack_count++;
                assignments = checkpoint; // Undo
                return false; 
            }
        }
    }

    // --- BRANCHING (GUESSING) ---
    stats.decision_count++;
    
    // Simplest logic possible: get first clause at index 0, get first literal at index 0
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

// ============================================================================
// Public Entry Point
// ============================================================================

SolverResult solve(CNF initial_cnf, SolverConfig config) {
    SolverResult result;
    
    auto start = chrono::high_resolution_clock::now();

    result.is_sat = dpll_recursive(initial_cnf, result.final_assignments, config, result);

    auto end = chrono::high_resolution_clock::now();
    chrono::duration<double, milli> duration = end - start;
    result.total_execution_time_ms = duration.count();

    if (result.is_sat == false) {
        result.final_assignments.clear();
    }
    
    return result;
}