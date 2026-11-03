#include "sat_types.hpp"
#include "truth_table.h"
#include "parser.h"

#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

struct TestCase {
    std::string name;
    std::string formula;
    bool expected_sat;
};

struct Configuration {
    bool use_unit_prop;
    bool use_pure_literal;
    const char* name;
};

bool assignment_satisfies_formula(
    const TruthTable& table,
    const CnfEncoding& encoding,
    const SolverResult& result) {
    for (std::uint64_t row = 0; row < table.num_rows(); ++row) {
        if (!table.value(row)) {
            continue;
        }

        const Assignment assignment = table.assignment(row);
        bool matches = true;
        for (const auto& entry : result.final_assignments) {
            if (encoding.original_variables.count(entry.first) == 0) {
                continue;
            }
            const auto value = assignment.find(entry.first);
            if (value == assignment.end() || value->second != entry.second) {
                matches = false;
                break;
            }
        }
        if (matches) {
            return true;
        }
    }
    return false;
}

bool run_case(
    const TestCase& test,
    CnfPipeline pipeline,
    const Configuration& configuration) {
    try {
        const NodePtr formula = parseFormula(test.formula);
        const CnfEncoding encoding = convert_formula_to_cnf(formula, pipeline);
        SolverConfig config;
        config.use_unit_prop = configuration.use_unit_prop;
        config.use_pure_literal = configuration.use_pure_literal;
        config.record_trace = true;

        const SolverResult result = solve(encoding.cnf, config);
        const TruthTable table = TruthTableEngine().truth_table(*formula);
        const bool oracle_sat = table.results.count_ones() != 0;
        if (oracle_sat != test.expected_sat || result.is_sat != oracle_sat) {
            std::cerr << "    SAT result disagrees with truth-table oracle\n";
            return false;
        }
        if (result.is_sat && !assignment_satisfies_formula(table, encoding, result)) {
            std::cerr << "    Solver assignment does not satisfy the formula\n";
            return false;
        }
        if (result.trace.empty()) {
            std::cerr << "    Solver did not return the requested trace\n";
            return false;
        }
        return true;
    } catch (const std::exception& error) {
        std::cerr << "    Exception: " << error.what() << '\n';
        return false;
    }
}

} // namespace

int main() {
    const std::vector<TestCase> tests = {
        {"single variable", "A", true},
        {"negation", "~A", true},
        {"conjunction", "A & B", true},
        {"contradiction", "A & ~A", false},
        {"tautology", "A | ~A", true},
        {"implication", "A -> B", true},
        {"biconditional", "A <-> B", true},
        {"nested mixed operators", "(A -> B) & (B <-> ~C)", true},
        {"unsatisfiable constraint", "(A | B) & (A | ~B) & (~A | B) & (~A | ~B)", false},
        {"distributed contradiction", "(A & ~A) | (B & ~B)", false}
    };
    const std::array<CnfPipeline, 2> pipelines = {
        CnfPipeline::Nnf,
        CnfPipeline::Tseitin
    };
    const std::array<const char*, 2> pipeline_names = {"NNF", "Tseitin"};
    const std::array<Configuration, 4> configurations = {{
        {false, false, "unit=off, pure=off"},
        {false, true, "unit=off, pure=on"},
        {true, false, "unit=on, pure=off"},
        {true, true, "unit=on, pure=on"}
    }};

    std::size_t checks = 0;
    std::size_t failures = 0;
    for (const TestCase& test : tests) {
        for (std::size_t pipeline_index = 0; pipeline_index < pipelines.size();
             ++pipeline_index) {
            for (const Configuration& configuration : configurations) {
                const bool passed =
                    run_case(test, pipelines[pipeline_index], configuration);
                std::cout << (passed ? "[PASS] " : "[FAIL] ") << test.name
                          << " (" << pipeline_names[pipeline_index] << ", "
                          << configuration.name << ")\n";
                ++checks;
                failures += !passed;
            }
        }
    }

    std::cout << '\n' << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
