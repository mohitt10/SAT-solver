#include "sat_types.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>

using namespace std;
//v : Total number of unique variables in the formula.
//c : Number of clauses

namespace {

void record_event(
    SolverResult& result,
    const SolverConfig& config,
    SolverTraceEvent event) {
    if (!config.record_trace) {
        return;
    }
    if (result.trace.size() >= config.trace_limit) {
        result.trace_truncated = true;
        return;
    }
    result.trace.push_back(std::move(event));
}

vector<pair<string, bool>> assignments_to_undo(
    const unordered_map<string, bool>& current,
    const unordered_map<string, bool>& checkpoint) {
    vector<pair<string, bool>> undone;
    for (const auto& assignment : current) {
        const auto previous = checkpoint.find(assignment.first);
        if (previous == checkpoint.end() || previous->second != assignment.second) {
            undone.push_back(assignment);
        }
    }
    sort(undone.begin(), undone.end(),
         [](const auto& left, const auto& right) {
             return left.first < right.first;
         });
    return undone;
}

vector<Literal> find_clause_for_unit(const CNF& cnf, const Literal& literal) {
    for (const Clause& clause : cnf.getClauses()) {
        if (clause.size() == 1 && clause.getLiterals()[0].isEqual(literal)) {
            return clause.getLiterals();
        }
    }
    return {};
}

void record_conflicts(
    SolverResult& result,
    const SolverConfig& config,
    const vector<vector<Literal>>& clauses,
    int decision_level) {
    if (!config.record_trace) {
        return;
    }
    for (const vector<Literal>& clause : clauses) {
        record_event(result, config,
                     {SolverEventType::Conflict, {}, false, decision_level,
                      clause, {}});
    }
}

} // namespace

// O(C+V) 
//assigned_lit is assumed to be marked true
CNF assign_literal(
    CNF current_cnf,
    Literal assigned_lit,
    vector<vector<Literal>>* conflict_clauses = nullptr) {
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
        if (shrunk.isEmpty() && conflict_clauses != nullptr) {
            conflict_clauses->push_back(lits);
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
bool dpll_recursive(
    CNF cnf,
    unordered_map<string, bool>& assignments,
    SolverConfig config,
    SolverResult& stats,
    int decision_level = 0,
    bool conflict_previously_recorded = false) {

    // 1.Base Case: Satisfied
    if (cnf.isEmpty()) {
        return true;
    }

    // 2.Base Case: Conflict
    vector<Clause> clauses = cnf.getClauses();
    for (int i = 0; i < clauses.size(); i++) {
        if (clauses[i].isEmpty()) {
            if (!conflict_previously_recorded) {
                record_event(stats, config,
                             {SolverEventType::Conflict, {}, false, decision_level,
                              clauses[i].getLiterals(), {}});
            }
            stats.backtrack_count++;
            return false; // Backtrack
        }
    }

    // current assignments
    unordered_map<string, bool> checkpoint = assignments;

    //Unit Propagation
    //O(V*C)
    Literal unit_lit;
    while (config.use_unit_prop && find_unit_literal(cnf, unit_lit)) {
        if (config.record_trace) {
            record_event(stats, config,
                         {SolverEventType::UnitPropagation,
                          unit_lit.getVariable(), !unit_lit.isNegated(),
                          decision_level, find_clause_for_unit(cnf, unit_lit), {}});
        }
        assignments[unit_lit.getVariable()] = !unit_lit.isNegated();
        vector<vector<Literal>> conflicts;
        cnf = assign_literal(
            cnf, unit_lit, config.record_trace ? &conflicts : nullptr);
        record_conflicts(stats, config, conflicts, decision_level);
        
        if (cnf.isEmpty()) return true;
        
        vector<Clause> new_clauses = cnf.getClauses();
        for (int i = 0; i < new_clauses.size(); i++) {
            if (new_clauses[i].isEmpty()) { //If any clause is empty it means that all the literals are guesses false
                stats.backtrack_count++;
                const auto undone = config.record_trace
                    ? assignments_to_undo(assignments, checkpoint)
                    : vector<pair<string, bool>>{};
                assignments = checkpoint; // Undo
                record_event(stats, config,
                             {SolverEventType::Backtrack, {}, false,
                              decision_level, {}, undone});
                return false; 
            }
        }
    }

    // Pure literal Elimination
    //O(V*C)
    Literal pure_lit;
    while (config.use_pure_literal && find_pure_literal(cnf, pure_lit)) {
        record_event(stats, config,
                     {SolverEventType::PureLiteral,
                      pure_lit.getVariable(), !pure_lit.isNegated(),
                      decision_level, {}, {}});
        assignments[pure_lit.getVariable()] = !pure_lit.isNegated();
        vector<vector<Literal>> conflicts;
        cnf = assign_literal(
            cnf, pure_lit, config.record_trace ? &conflicts : nullptr);
        record_conflicts(stats, config, conflicts, decision_level);
        
        if (cnf.isEmpty()) return true;
        
        vector<Clause> new_clauses = cnf.getClauses();
        for (int i = 0; i < new_clauses.size(); i++) {
            if (new_clauses[i].isEmpty()) {
                stats.backtrack_count++;
                const auto undone = config.record_trace
                    ? assignments_to_undo(assignments, checkpoint)
                    : vector<pair<string, bool>>{};
                assignments = checkpoint; //undo
                record_event(stats, config,
                             {SolverEventType::Backtrack, {}, false,
                              decision_level, {}, undone});
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
    unordered_map<string, bool> branch_checkpoint;
    if (config.record_trace) {
        branch_checkpoint = assignments;
    }

    // Guess True
    record_event(stats, config,
                 {SolverEventType::Decision, var_name, true,
                  decision_level + 1, {}, {}});
    assignments[var_name] = true;
    Literal guess_true(var_name, false); 
    vector<vector<Literal>> true_conflicts;
    CNF cnf_if_true = assign_literal(
        cnf, guess_true, config.record_trace ? &true_conflicts : nullptr);
    record_conflicts(stats, config, true_conflicts, decision_level + 1);
    
    if (dpll_recursive(cnf_if_true, assignments, config, stats,
                       decision_level + 1, !true_conflicts.empty())) {
        return true;
    }

    // Guess False (Implicit Backtrack)
    if (config.record_trace) {
        record_event(stats, config,
                     {SolverEventType::Backtrack, {}, false, decision_level,
                      {}, assignments_to_undo(assignments, branch_checkpoint)});
    }
    record_event(stats, config,
                 {SolverEventType::Decision, var_name, false,
                  decision_level + 1, {}, {}});
    assignments[var_name] = false;
    Literal guess_false(var_name, true);
    vector<vector<Literal>> false_conflicts;
    CNF cnf_if_false = assign_literal(
        cnf, guess_false, config.record_trace ? &false_conflicts : nullptr);
    record_conflicts(stats, config, false_conflicts, decision_level + 1);
    
    if (dpll_recursive(cnf_if_false, assignments, config, stats,
                       decision_level + 1, !false_conflicts.empty())) {
        return true;
    }

    // Total Failure: Undo and backtrack
    const auto undone = config.record_trace
        ? assignments_to_undo(assignments, checkpoint)
        : vector<pair<string, bool>>{};
    assignments = checkpoint;
    record_event(stats, config,
                 {SolverEventType::Backtrack, {}, false, decision_level,
                  {}, undone});
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

    record_event(result, config,
                 {SolverEventType::Result, {}, result.is_sat, 0, {}, {}});

    if (result.is_sat == false) {
        result.final_assignments.clear();
    }
    
    return result;
}