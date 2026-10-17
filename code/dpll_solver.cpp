#include "sat_types.hpp"

#include <chrono>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

CNF assign_literal(const CNF& cnf, const Literal& assigned_literal) {
    CNF simplified;
    for (const Clause& clause : cnf.getClauses()) {
        if (clause.containsLiteral(assigned_literal)) {
            continue;
        }

        Clause reduced_clause;
        for (const Literal& literal : clause.getLiterals()) {
            if (!literal.isOpposite(assigned_literal)) {
                reduced_clause.addLiteral(literal);
            }
        }
        simplified.addClause(reduced_clause);
    }
    return simplified;
}

bool has_empty_clause(const CNF& cnf) {
    for (const Clause& clause : cnf.getClauses()) {
        if (clause.isEmpty()) {
            return true;
        }
    }
    return false;
}

bool find_unit_literal(const CNF& cnf, Literal& result) {
    for (const Clause& clause : cnf.getClauses()) {
        if (clause.size() == 1) {
            result = clause.getLiterals().front();
            return true;
        }
    }
    return false;
}

bool find_pure_literal(const CNF& cnf, Literal& result) {
    std::unordered_map<std::string, int> polarities;
    for (const Clause& clause : cnf.getClauses()) {
        for (const Literal& literal : clause.getLiterals()) {
            const int polarity = literal.isNegated() ? -1 : 1;
            const auto insertion = polarities.emplace(
                literal.getVariable(), polarity);
            if (!insertion.second && insertion.first->second != polarity) {
                insertion.first->second = 0;
            }
        }
    }

    for (const auto& entry : polarities) {
        if (entry.second != 0) {
            result = Literal(entry.first, entry.second < 0);
            return true;
        }
    }
    return false;
}

bool dpll_recursive(
    CNF cnf,
    std::unordered_map<std::string, bool>& assignments,
    const SolverConfig& config,
    SolverResult& stats) {
    const auto checkpoint = assignments;

    while (true) {
        if (has_empty_clause(cnf)) {
            ++stats.backtrack_count;
            assignments = checkpoint;
            return false;
        }
        if (cnf.isEmpty()) {
            return true;
        }

        Literal forced_literal;
        if (config.use_unit_prop && find_unit_literal(cnf, forced_literal)) {
            assignments[forced_literal.getVariable()] =
                !forced_literal.isNegated();
            cnf = assign_literal(cnf, forced_literal);
            continue;
        }
        if (config.use_pure_literal && find_pure_literal(cnf, forced_literal)) {
            assignments[forced_literal.getVariable()] =
                !forced_literal.isNegated();
            cnf = assign_literal(cnf, forced_literal);
            continue;
        }
        break;
    }

    if (cnf.isEmpty()) {
        return true;
    }

    const Literal branch_literal = cnf.getClauses().front().getLiterals().front();
    const std::string& variable = branch_literal.getVariable();
    ++stats.decision_count;

    for (const bool value : {true, false}) {
        assignments[variable] = value;
        const Literal satisfied_literal(variable, !value);
        if (dpll_recursive(
                assign_literal(cnf, satisfied_literal),
                assignments,
                config,
                stats)) {
            return true;
        }
    }

    assignments = checkpoint;
    return false;
}

} // namespace

SolverResult solve(CNF initial_cnf, SolverConfig config) {
    SolverResult result;
    const auto start = std::chrono::steady_clock::now();
    result.is_sat = dpll_recursive(
        std::move(initial_cnf), result.final_assignments, config, result);
    const auto end = std::chrono::steady_clock::now();
    result.total_execution_time_ms =
        std::chrono::duration<double, std::milli>(end - start).count();

    if (!result.is_sat) {
        result.final_assignments.clear();
    }
    return result;
}
