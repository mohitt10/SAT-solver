#include "graph_coloring.h"

#include "code/sat_types.hpp"
#include "nnf.h"
#include "parser.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

const std::string OR_TEXT = " OR ";
const std::string AND_TEXT = " AND ";
const std::string NOT_TEXT = "NOT ";

void validate_graph(
    int vertex_count,
    int color_count,
    const std::vector<std::pair<int, int>>& edges) {
    if (vertex_count <= 0) {
        throw std::invalid_argument("The graph must contain at least one vertex.");
    }
    if (color_count <= 0) {
        throw std::invalid_argument("The number of colours must be positive.");
    }
    for (const auto& edge : edges) {
        if (edge.first < 1 || edge.first > vertex_count ||
            edge.second < 1 || edge.second > vertex_count) {
            throw std::invalid_argument("An edge refers to a vertex outside the graph.");
        }
    }
}

} // namespace

Encoder::Encoder(int nodes, int colours)
    : no_of_col(colours), no_of_nodes(nodes) {
    validate_graph(no_of_nodes, no_of_col, edges);
}

Encoder::Encoder(int nodes, int colours, std::vector<std::pair<int, int>> graph_edges)
    : no_of_col(colours), no_of_nodes(nodes), edges(std::move(graph_edges)) {
    validate_graph(no_of_nodes, no_of_col, edges);
}

void Encoder::build_initial_string() {
    encoded_string.clear();
    encoded_string.push_back('(');
    for (int vertex = 1; vertex <= no_of_nodes; ++vertex) {
        encoded_string.push_back('(');
        for (int color = 1; color <= no_of_col; ++color) {
            encoded_string += std::to_string(vertex) + "_" + std::to_string(color);
            if (color != no_of_col) {
                encoded_string += OR_TEXT;
            }
        }
        encoded_string.push_back(')');

        for (int first_color = 1; first_color <= no_of_col; ++first_color) {
            for (int second_color = first_color + 1;
                 second_color <= no_of_col;
                 ++second_color) {
                encoded_string += AND_TEXT + "(" + NOT_TEXT +
                    std::to_string(vertex) + "_" + std::to_string(first_color) +
                    OR_TEXT + NOT_TEXT + std::to_string(vertex) + "_" +
                    std::to_string(second_color) + ")";
            }
        }
        if (vertex != no_of_nodes) {
            encoded_string += AND_TEXT;
        }
    }
    encoded_string.push_back(')');
}

void Encoder::printencodedstring() {
    std::cout << encoded_string << '\n';
}

std::string Encoder::getencodedstring() const {
    return encoded_string;
}

void Encoder::init_edge_list(std::vector<std::pair<int, int>>& graph_edges) {
    validate_graph(no_of_nodes, no_of_col, graph_edges);
    edges = graph_edges;
}

void Encoder::addedgeconstraint() {
    for (const auto& edge : edges) {
        for (int color = 1; color <= no_of_col; ++color) {
            encoded_string += AND_TEXT + "(" + NOT_TEXT +
                std::to_string(edge.first) + "_" + std::to_string(color) +
                OR_TEXT + NOT_TEXT + std::to_string(edge.second) + "_" +
                std::to_string(color) + ")";
        }
    }
}

std::vector<int> Encoder::decoderesult(
    const std::vector<std::pair<std::string, int>>& result) const {
    std::vector<int> assignment(no_of_nodes, -1);
    for (const auto& value : result) {
        if (!value.second) {
            continue;
        }

        const std::size_t separator = value.first.find('_');
        if (separator == std::string::npos) {
            return {};
        }

        const int vertex = std::stoi(value.first.substr(0, separator));
        const int color = std::stoi(value.first.substr(separator + 1));
        if (vertex < 1 || vertex > no_of_nodes ||
            color < 1 || color > no_of_col ||
            assignment[vertex - 1] != -1) {
            return {};
        }
        assignment[vertex - 1] = color;
    }
    return assignment;
}

bool Encoder::checkresult(
    const std::vector<std::pair<std::string, int>>& result) const {
    const std::vector<int> assignment = decoderesult(result);
    if (assignment.empty()) {
        return false;
    }

    for (int color : assignment) {
        if (color == -1) {
            return false;
        }
    }
    for (const auto& edge : edges) {
        if (assignment[edge.first - 1] == assignment[edge.second - 1]) {
            return false;
        }
    }
    return true;
}

GraphColoringResult solve_graph_coloring(
    int vertex_count,
    int color_count,
    const std::vector<std::pair<int, int>>& edges,
    CnfPipeline pipeline) {
    Encoder encoder(vertex_count, color_count, edges);
    encoder.build_initial_string();
    encoder.addedgeconstraint();

    const NodePtr parsed = parseFormula(encoder.getencodedstring());
    const CnfEncoding encoding = convert_formula_to_cnf(parsed, pipeline);
    const SolverResult solver_result = solve(encoding.cnf, SolverConfig{});

    GraphColoringResult result;
    result.is_satisfiable = solver_result.is_sat;
    result.decision_count = solver_result.decision_count;
    result.backtrack_count = solver_result.backtrack_count;
    result.solver_time_ms = solver_result.total_execution_time_ms;

    if (solver_result.is_sat) {
        std::vector<std::pair<std::string, int>> assignments;
        assignments.reserve(encoding.original_variables.size());
        for (const auto& assignment : solver_result.final_assignments) {
            if (encoding.original_variables.count(assignment.first) != 0) {
                assignments.emplace_back(
                    assignment.first, assignment.second ? 1 : 0);
            }
        }
        if (!encoder.checkresult(assignments)) {
            throw std::runtime_error(
                "The SAT solver returned an invalid graph colouring.");
        }
        result.vertex_colors = encoder.decoderesult(assignments);
    }
    return result;
}
