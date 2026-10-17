#include "sat_types.hpp"

#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class AstBuilder {
public:
    NodePtr variable(const std::string& name) {
        return std::make_shared<VarNode>(name);
    }

    NodePtr negate(NodePtr child) {
        return std::make_shared<NotNode>(std::move(child));
    }

    NodePtr conjunction(NodePtr left, NodePtr right) {
        return std::make_shared<AndNode>(std::move(left), std::move(right));
    }

    NodePtr disjunction(NodePtr left, NodePtr right) {
        return std::make_shared<OrNode>(std::move(left), std::move(right));
    }
};

struct TestCase {
    std::string name;
    NodePtr root;
    std::vector<std::vector<Literal>> expected;
};

bool clause_matches(Clause& actual, const std::vector<Literal>& expected) {
    std::vector<Literal> actual_literals = actual.getLiterals();
    if (actual_literals.size() != expected.size()) {
        return false;
    }

    for (const Literal& expected_literal : expected) {
        if (!actual.containsLiteral(expected_literal)) {
            return false;
        }
    }

    return true;
}

bool cnf_matches(CNF& actual, const std::vector<std::vector<Literal>>& expected) {
    std::vector<Clause> actual_clauses = actual.getClauses();
    if (actual_clauses.size() != expected.size()) {
        return false;
    }

    std::vector<bool> matched(actual_clauses.size(), false);
    for (const std::vector<Literal>& expected_clause : expected) {
        bool found_match = false;
        for (std::size_t i = 0; i < actual_clauses.size(); ++i) {
            if (!matched[i] && clause_matches(actual_clauses[i], expected_clause)) {
                matched[i] = true;
                found_match = true;
                break;
            }
        }
        if (!found_match) {
            return false;
        }
    }

    return true;
}

void print_cnf(CNF& cnf) {
    std::vector<Clause> clauses = cnf.getClauses();
    if (clauses.empty()) {
        std::cout << "TRUE";
        return;
    }

    for (std::size_t i = 0; i < clauses.size(); ++i) {
        if (i > 0) {
            std::cout << " AND ";
        }

        std::vector<Literal> literals = clauses[i].getLiterals();
        if (literals.empty()) {
            std::cout << "FALSE";
            continue;
        }

        std::cout << "(";
        for (std::size_t j = 0; j < literals.size(); ++j) {
            if (j > 0) {
                std::cout << " OR ";
            }
            if (literals[j].isNegated()) {
                std::cout << "~";
            }
            std::cout << literals[j].getVariable();
        }
        std::cout << ")";
    }
}

int main() {
    AstBuilder ast;
    std::vector<TestCase> tests;

    NodePtr a = ast.variable("A");
    NodePtr b = ast.variable("B");
    NodePtr c = ast.variable("C");
    NodePtr d = ast.variable("D");
    NodePtr not_a = ast.negate(a);
    NodePtr not_b = ast.negate(b);
    NodePtr not_c = ast.negate(c);
    NodePtr not_d = ast.negate(d);

    tests.push_back({"positive literal", a, {
        {Literal("A", false)}
    }});
    tests.push_back({"negative literal", not_a, {
        {Literal("A", true)}
    }});
    tests.push_back({"conjunction of literals", ast.conjunction(a, b), {
        {Literal("A", false)},
        {Literal("B", false)}
    }});
    tests.push_back({"disjunction of literals", ast.disjunction(a, b), {
        {Literal("A", false), Literal("B", false)}
    }});
    tests.push_back({"conjunction distributed over disjunction", ast.disjunction(
        ast.conjunction(a, b), c), {
        {Literal("A", false), Literal("C", false)},
        {Literal("B", false), Literal("C", false)}
    }});
    tests.push_back({"disjunction distributed over conjunction", ast.disjunction(
        a, ast.conjunction(b, c)), {
        {Literal("A", false), Literal("B", false)},
        {Literal("A", false), Literal("C", false)}
    }});
    tests.push_back({"cross product of two conjunctions", ast.disjunction(
        ast.conjunction(a, b), ast.conjunction(c, d)), {
        {Literal("A", false), Literal("C", false)},
        {Literal("A", false), Literal("D", false)},
        {Literal("B", false), Literal("C", false)},
        {Literal("B", false), Literal("D", false)}
    }});
    tests.push_back({"tautological distributed clause is removed", ast.disjunction(
        ast.conjunction(a, not_b),
        ast.conjunction(b, c)), {
        {Literal("A", false), Literal("B", false)},
        {Literal("A", false), Literal("C", false)},
        {Literal("B", true), Literal("C", false)}
    }});
    tests.push_back({"already-CNF expression", ast.conjunction(
        ast.disjunction(a, b),
        ast.disjunction(not_c, d)), {
        {Literal("A", false), Literal("B", false)},
        {Literal("C", true), Literal("D", false)}
    }});
    tests.push_back({"distribution followed by conjunction", ast.conjunction(
        ast.disjunction(ast.conjunction(a, b), c), not_d), {
        {Literal("A", false), Literal("C", false)},
        {Literal("B", false), Literal("C", false)},
        {Literal("D", true)}
    }});
    tests.push_back({"duplicate literal in a clause", ast.disjunction(a, a), {
        {Literal("A", false)}
    }});
    tests.push_back({"contradictory unit clauses remain", ast.conjunction(a, not_a), {
        {Literal("A", false)},
        {Literal("A", true)}
    }});
    tests.push_back({"tautology converts to true CNF", ast.disjunction(a, not_a), {}});
    tests.push_back({"contradictory conjunction distributed with a literal",
        ast.disjunction(ast.conjunction(a, not_a), b), {
        {Literal("A", false), Literal("B", false)},
        {Literal("A", true), Literal("B", false)}
    }});

    int failures = 0;
    for (std::size_t i = 0; i < tests.size(); ++i) {
        std::cout << "Example " << (i + 1) << ": " << tests[i].name << "\n";
        try {
            CNF actual = convert_nnf_to_cnf(tests[i].root);
            std::cout << "  CNF: ";
            print_cnf(actual);
            const bool passed = cnf_matches(actual, tests[i].expected);
            std::cout << "\n  " << (passed ? "PASS" : "FAIL") << "\n";
            if (!passed) {
                ++failures;
            }
        } catch (const std::exception& error) {
            std::cout << "  CNF: <conversion threw an exception>\n"
                      << "  FAIL: " << error.what() << "\n";
            ++failures;
        }
    }

    std::cout << "\n" << (failures == 0 ? "ALL PASS" : "FAILURES: ")
              << (failures == 0 ? "" : std::to_string(failures)) << "\n";
    return failures == 0 ? 0 : 1;
}
