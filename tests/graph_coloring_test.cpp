#include "graph_coloring.h"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

bool has_valid_colors(
    const GraphColoringResult& result,
    int vertex_count,
    int color_count,
    const std::vector<std::pair<int, int>>& edges) {
    if (!result.is_satisfiable ||
        result.vertex_colors.size() != static_cast<std::size_t>(vertex_count)) {
        return false;
    }
    for (int color : result.vertex_colors) {
        if (color < 1 || color > color_count) {
            return false;
        }
    }
    for (const auto& edge : edges) {
        if (result.vertex_colors[edge.first - 1] ==
            result.vertex_colors[edge.second - 1]) {
            return false;
        }
    }
    return true;
}

bool run_case(
    const std::string& name,
    int vertex_count,
    int color_count,
    const std::vector<std::pair<int, int>>& edges,
    bool expected_satisfiable,
    CnfPipeline pipeline) {
    try {
        const GraphColoringResult result =
            solve_graph_coloring(vertex_count, color_count, edges, pipeline);
        const bool passed = result.is_satisfiable == expected_satisfiable &&
            (!expected_satisfiable ||
             has_valid_colors(result, vertex_count, color_count, edges));
        std::cout << (passed ? "[PASS] " : "[FAIL] ") << name
                  << (pipeline == CnfPipeline::Tseitin ? " (Tseitin)" : " (NNF)")
                  << '\n';
        return passed;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] " << name << ": " << error.what() << '\n';
        return false;
    }
}

bool run_both_pipelines(
    const std::string& name,
    int vertex_count,
    int color_count,
    const std::vector<std::pair<int, int>>& edges,
    bool expected_satisfiable) {
    const bool nnf_passed = run_case(
        name, vertex_count, color_count, edges, expected_satisfiable,
        CnfPipeline::Nnf);
    const bool tseitin_passed = run_case(
        name, vertex_count, color_count, edges, expected_satisfiable,
        CnfPipeline::Tseitin);
    return nnf_passed && tseitin_passed;
}

bool rejects_invalid_edge() {
    try {
        (void)solve_graph_coloring(3, 2, {{1, 4}});
        std::cerr << "[FAIL] invalid edge is rejected\n";
        return false;
    } catch (const std::invalid_argument&) {
        std::cout << "[PASS] invalid edge is rejected\n";
        return true;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] invalid edge is rejected: " << error.what() << '\n';
        return false;
    }
}

} // namespace

int main() {
    int failures = 0;
    failures += !run_both_pipelines("single vertex, one colour", 1, 1, {}, true);
    failures += !run_both_pipelines(
        "path is two-colourable", 4, 2, {{1, 2}, {2, 3}, {3, 4}}, true);
    failures += !run_both_pipelines(
        "triangle is three-colourable", 3, 3, {{1, 2}, {2, 3}, {3, 1}}, true);
    failures += !run_both_pipelines(
        "triangle is not two-colourable", 3, 2,
        {{1, 2}, {2, 3}, {3, 1}}, false);
    failures += !run_both_pipelines(
        "self-loop is not colorable", 1, 3, {{1, 1}}, false);
    failures += !rejects_invalid_edge();

    std::cout << '\n' << (failures == 0 ? "ALL TESTS PASSED" : "Failures: ")
              << (failures == 0 ? "" : std::to_string(failures)) << '\n';
    return failures == 0 ? 0 : 1;
}
