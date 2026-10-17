#pragma once

#include <string>
#include <utility>
#include <vector>

class Encoder {
public:
    Encoder(int nodes, int colours);
    Encoder(int nodes, int colours, std::vector<std::pair<int, int>> edges);

    void build_initial_string();
    void printencodedstring();
    std::string getencodedstring() const;
    void init_edge_list(std::vector<std::pair<int, int>>& edges);
    void addedgeconstraint();
    std::vector<int> decoderesult(
        const std::vector<std::pair<std::string, int>>& result) const;
    bool checkresult(
        const std::vector<std::pair<std::string, int>>& result) const;

private:
    int no_of_col;
    int no_of_nodes;
    std::string encoded_string;
    std::vector<std::pair<int, int>> edges;
};

struct GraphColoringResult {
    bool is_satisfiable = false;
    std::vector<int> vertex_colors;
    int decision_count = 0;
    int backtrack_count = 0;
    double solver_time_ms = 0.0;
};

GraphColoringResult solve_graph_coloring(
    int vertex_count,
    int color_count,
    const std::vector<std::pair<int, int>>& edges);
