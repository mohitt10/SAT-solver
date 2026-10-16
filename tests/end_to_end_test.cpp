#include "sat_types.hpp"

#include <array>
#include <exception>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

CNF convert_nnf_to_cnf(const NodePtr& root);
SolverResult solve(CNF initial_cnf, SolverConfig config);

namespace {

using Assignment = std::unordered_map<std::string, bool>;

struct TestCase {
    std::string name;
    NodePtr root;
    bool expected_sat;
};

bool evaluate(const NodePtr& node, const Assignment& assignment) {
    if (!node) {
        throw std::invalid_argument("Null AST node in test formula");
    }

    if (const auto variable = std::dynamic_pointer_cast<VarNode>(node)) {
        return assignment.at(variable->name);
    }

    if (const auto negation = std::dynamic_pointer_cast<NotNode>(node)) {
        const auto variable = std::dynamic_pointer_cast<VarNode>(negation->left);
        if (!variable) {
            throw std::invalid_argument("Test AST is not in NNF");
        }
        return !assignment.at(variable->name);
    }

    if (const auto conjunction = std::dynamic_pointer_cast<AndNode>(node)) {
        return evaluate(conjunction->left, assignment) &&
               evaluate(conjunction->right, assignment);
    }

    if (const auto disjunction = std::dynamic_pointer_cast<OrNode>(node)) {
        return evaluate(disjunction->left, assignment) ||
               evaluate(disjunction->right, assignment);
    }

    throw std::invalid_argument("Unsupported AST node in test formula");
}

bool has_satisfying_extension(
    const NodePtr& root,
    const Assignment& partial_assignment,
    const std::array<std::string, 4>& variables) {
    for (unsigned int mask = 0; mask < (1u << variables.size()); ++mask) {
        Assignment candidate;
        for (std::size_t i = 0; i < variables.size(); ++i) {
            candidate[variables[i]] = (mask & (1u << i)) != 0;
        }

        bool matches_partial = true;
        for (const auto& entry : partial_assignment) {
            const auto value = candidate.find(entry.first);
            if (value == candidate.end() || value->second != entry.second) {
                matches_partial = false;
                break;
            }
        }

        if (matches_partial && evaluate(root, candidate)) {
            return true;
        }
    }
    return false;
}

bool truth_table_says_sat(
    const NodePtr& root,
    const std::array<std::string, 4>& variables) {
    return has_satisfying_extension(root, Assignment{}, variables);
}

bool run_test_case(
    const TestCase& test,
    const std::array<std::string, 4>& variables,
    bool use_unit_prop,
    bool use_pure_literal) {
    try {
        const CNF cnf = convert_nnf_to_cnf(test.root);
        SolverConfig config;
        config.use_unit_prop = use_unit_prop;
        config.use_pure_literal = use_pure_literal;

        const SolverResult result = solve(cnf, config);
        const bool oracle_sat = truth_table_says_sat(test.root, variables);
        if (oracle_sat != test.expected_sat || result.is_sat != oracle_sat) {
            return false;
        }

        if (result.is_sat) {
            return has_satisfying_extension(
                test.root, result.final_assignments, variables);
        }
        return result.final_assignments.empty();
    } catch (const std::exception& error) {
        std::cerr << "    Exception: " << error.what() << "\n";
        return false;
    }
}

} // namespace

int main() {
    const NodePtr a = mkVar("A");
    const NodePtr b = mkVar("B");
    const NodePtr c = mkVar("C");
    const NodePtr d = mkVar("D");
    const NodePtr not_a = mkNot(a);
    const NodePtr not_b = mkNot(b);
    const NodePtr not_c = mkNot(c);
    const NodePtr not_d = mkNot(d);

    const std::vector<TestCase> tests = {
        {"positive literal", a, true},
        {"negative literal", not_a, true},
        {"conjunction of positive literals", mkAnd(a, b), true},
        {"contradictory unit clauses", mkAnd(a, not_a), false},
        {"disjunction of literals", mkOr(a, b), true},
        {"all assignments blocked by unit clauses",
         mkAnd(mkAnd(mkOr(a, b), mkOr(a, not_b)),
               mkAnd(mkOr(not_a, b), mkOr(not_a, not_b))),
         false},
        {"OR distributed over left conjunction", mkOr(mkAnd(a, b), c), true},
        {"OR distributed over right conjunction", mkOr(a, mkAnd(b, c)), true},
        {"cross product distribution",
         mkOr(mkAnd(a, b), mkAnd(c, d)),
         true},
        {"equivalent variables",
         mkOr(mkAnd(a, b), mkAnd(not_a, not_b)),
         true},
        {"forced assignment through clauses",
         mkAnd(mkAnd(mkOr(a, b), mkOr(not_a, b)),
               mkOr(a, not_b)),
         true},
        {"satisfiable three-variable constraint chain",
         mkAnd(mkAnd(mkOr(a, b), mkOr(not_a, c)),
               mkAnd(mkOr(not_b, c), mkOr(not_c, d))),
         true},
        {"tautology becomes empty CNF", mkOr(a, not_a), true},
        {"contradictory distributed formula",
         mkOr(mkAnd(a, not_a), b),
         true},
        {"negative literals with unit propagation",
         mkAnd(mkAnd(mkOr(not_a, b), mkOr(not_b, c)),
               mkAnd(not_c, not_d)),
         true}
    };

    const std::array<std::string, 4> variables = {"A", "B", "C", "D"};
    const std::array<std::pair<bool, bool>, 4> configurations = {{
        {false, false},
        {false, true},
        {true, false},
        {true, true}
    }};
    const std::array<std::string, 4> config_names = {
        "unit=off, pure=off",
        "unit=off, pure=on",
        "unit=on, pure=off",
        "unit=on, pure=on"
    };

    std::size_t failures = 0;
    std::size_t checks = 0;
    for (std::size_t test_index = 0; test_index < tests.size(); ++test_index) {
        for (std::size_t config_index = 0;
             config_index < configurations.size();
             ++config_index) {
            const bool passed = run_test_case(
                tests[test_index],
                variables,
                configurations[config_index].first,
                configurations[config_index].second);

            std::cout << (passed ? "[PASS] " : "[FAIL] ")
                      << tests[test_index].name << " ("
                      << config_names[config_index] << ")\n";
            ++checks;
            if (!passed) {
                ++failures;
            }
        }
    }

    std::cout << "\n" << checks << " checks, " << failures << " failures\n";
    if (failures == 0) {
        std::cout << "ALL TESTS PASSED\n";
        return 0;
    }

    std::cout << "SOME TESTS FAILED\n";
    return 1;
}
