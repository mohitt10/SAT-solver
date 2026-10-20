#include "sat_types.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

using Assignment = std::unordered_map<std::string, bool>;

struct TestCase {
    std::string name;
    NodePtr formula;
};

bool evaluate_formula(const NodePtr& node, const Assignment& assignment) {
    if (const auto variable = std::dynamic_pointer_cast<VarNode>(node)) {
        return assignment.at(variable->name);
    }
    if (const auto negation = std::dynamic_pointer_cast<NotNode>(node)) {
        return !evaluate_formula(negation->left, assignment);
    }
    if (const auto conjunction = std::dynamic_pointer_cast<AndNode>(node)) {
        return evaluate_formula(conjunction->left, assignment) &&
               evaluate_formula(conjunction->right, assignment);
    }
    if (const auto disjunction = std::dynamic_pointer_cast<OrNode>(node)) {
        return evaluate_formula(disjunction->left, assignment) ||
               evaluate_formula(disjunction->right, assignment);
    }
    if (const auto implication = std::dynamic_pointer_cast<ImpliesNode>(node)) {
        return !evaluate_formula(implication->left, assignment) ||
               evaluate_formula(implication->right, assignment);
    }
    if (const auto biconditional = std::dynamic_pointer_cast<IffNode>(node)) {
        return evaluate_formula(biconditional->left, assignment) ==
               evaluate_formula(biconditional->right, assignment);
    }
    throw std::invalid_argument("Unsupported or null AST node");
}

bool clause_is_satisfied(const Clause& clause, const Assignment& assignment) {
    for (const Literal& literal : clause.getLiterals()) {
        const bool value = assignment.at(literal.getVariable());
        if (value != literal.isNegated()) {
            return true;
        }
    }
    return false;
}

bool cnf_is_satisfied(const CNF& cnf, const Assignment& assignment) {
    for (const Clause& clause : cnf.getClauses()) {
        if (!clause_is_satisfied(clause, assignment)) {
            return false;
        }
    }
    return true;
}

bool has_satisfying_auxiliary_extension(
    const CnfEncoding& encoding,
    const Assignment& source_assignment) {
    std::vector<std::string> auxiliary_variables;
    for (const Clause& clause : encoding.cnf.getClauses()) {
        for (const Literal& literal : clause.getLiterals()) {
            const std::string& variable = literal.getVariable();
            if (encoding.original_variables.count(variable) == 0 &&
                std::find(auxiliary_variables.begin(), auxiliary_variables.end(),
                          variable) == auxiliary_variables.end()) {
                auxiliary_variables.push_back(variable);
            }
        }
    }

    const unsigned int extension_count =
        1u << static_cast<unsigned int>(auxiliary_variables.size());
    for (unsigned int mask = 0; mask < extension_count; ++mask) {
        Assignment candidate = source_assignment;
        for (std::size_t index = 0; index < auxiliary_variables.size(); ++index) {
            candidate[auxiliary_variables[index]] =
                (mask & (1u << static_cast<unsigned int>(index))) != 0;
        }
        if (cnf_is_satisfied(encoding.cnf, candidate)) {
            return true;
        }
    }
    return false;
}

bool run_case(const TestCase& test) {
    const CnfEncoding encoding =
        convert_formula_to_cnf(test.formula, CnfPipeline::Tseitin);
    const std::vector<std::string> variables(
        encoding.original_variables.begin(), encoding.original_variables.end());
    bool expected_sat = false;
    for (unsigned int mask = 0; mask < (1u << variables.size()); ++mask) {
        Assignment assignment;
        for (std::size_t index = 0; index < variables.size(); ++index) {
            assignment[variables[index]] =
                (mask & (1u << static_cast<unsigned int>(index))) != 0;
        }
        const bool expected = evaluate_formula(test.formula, assignment);
        expected_sat = expected_sat || expected;
        if (has_satisfying_auxiliary_extension(encoding, assignment) != expected) {
            std::cerr << "[FAIL] " << test.name
                      << " does not preserve semantics for an assignment\n";
            return false;
        }
    }

    const SolverResult result = solve(encoding.cnf, SolverConfig{});
    if (result.is_sat != expected_sat) {
        std::cerr << "[FAIL] " << test.name << " has incorrect SAT result\n";
        return false;
    }
    std::cout << "[PASS] " << test.name << '\n';
    return true;
}

} // namespace

int main() {
    const NodePtr a = mkVar("A");
    const NodePtr b = mkVar("B");
    const NodePtr c = mkVar("C");
    const std::vector<TestCase> tests = {
        {"variable", a},
        {"negation", mkNot(mkNot(a))},
        {"conjunction", mkAnd(a, b)},
        {"disjunction", mkOr(a, b)},
        {"implication", mkImplies(a, b)},
        {"biconditional", mkIff(a, b)},
        {"contradictory formula", mkAnd(a, mkNot(a))},
        {"nested operators", mkIff(mkImplies(a, b), mkOr(mkNot(b), c))},
        {"non-colliding auxiliary names",
         mkAnd(mkVar("__tseitin_0"), mkOr(a, b))}
    };

    int failures = 0;
    for (const TestCase& test : tests) {
        try {
            failures += !run_case(test);
        } catch (const std::exception& error) {
            std::cerr << "[FAIL] " << test.name << ": " << error.what() << '\n';
            ++failures;
        }
    }
    std::cout << (failures == 0 ? "ALL TESTS PASSED" : "Failures: ")
              << (failures == 0 ? "" : std::to_string(failures)) << '\n';
    return failures == 0 ? 0 : 1;
}
