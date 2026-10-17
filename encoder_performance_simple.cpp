#include <bits/stdc++.h>
#include "parser.h"
#include "nnf.h"
#include "truth_table.h"
#include "code/sat_types.hpp"
#include "graph_coloring.h"
using namespace std;

class PerformanceTester {
    private:
    int no_of_nodes;
    int no_of_col;
    vector<pair<int, int>> edges;
    double previous_time[4] = {-1, -1, -1, -1};
    double previous_table_time = -1;

    public:
    PerformanceTester(int nodes, int colour, vector<pair<int, int>> v) {
        no_of_nodes = nodes;
        no_of_col = colour;
        edges = v;
    }

    double time_ms(chrono::steady_clock::time_point start, chrono::steady_clock::time_point end) {
        return chrono::duration<double, milli>(end - start).count();
    }

    void run() {
        vector<SolverConfig> configs = {
            {false, false},
            {true, false},
            {false, true},
            {true, true}
        };

        cout << fixed << setprecision(3);
        cout << "\n========== PERFORMANCE TESTING ==========\n";

        for(int n = 1; n <= no_of_nodes; n++) {
            vector<pair<int, int>> current_edges;
            for(auto &[a, b]: edges) {
                if(a <= n && b <= n)    current_edges.push_back({a, b});
            }

            auto start = chrono::steady_clock::now();
            Encoder e(n, no_of_col, current_edges);
            e.build_initial_string();
            e.addedgeconstraint();
            auto end = chrono::steady_clock::now();
            double encoding_time = time_ms(start, end);

            start = chrono::steady_clock::now();
            NodePtr parsed = parseFormula(e.getencodedstring());
            end = chrono::steady_clock::now();
            double parsing_time = time_ms(start, end);

            start = chrono::steady_clock::now();
            NNFConverter converter;
            NodePtr nnf = converter.convert(*parsed);
            CNF cnf = convert_nnf_to_cnf(nnf);
            end = chrono::steady_clock::now();
            double cnf_time = time_ms(start, end);

            int clauses = cnf.size();
            int literals = 0;
            for(auto clause: cnf.getClauses()) {
                literals += clause.size();
            }

            cout << "\n-----------------------------------------\n";
            cout << "Nodes: " << n << "  Colours: " << no_of_col
                 << "  Variables: " << n * no_of_col << "\n";
            cout << "Edges: " << current_edges.size() << "  Clauses: " << clauses
                 << "  Literals: " << literals << "\n";
            cout << "Formula length: " << e.getencodedstring().size() << " characters\n";
            cout << "Encoding time: " << encoding_time << " ms\n";
            cout << "Parsing time: " << parsing_time << " ms\n";
            cout << "CNF conversion time: " << cnf_time << " ms\n";

            cout << "\nDPLL comparison:\n";
            cout << left << setw(12) << "Unit prop"
                 << setw(14) << "Pure literal"
                 << setw(12) << "Time(ms)"
                 << setw(13) << "Change(%)"
                 << setw(9) << "Result"
                 << setw(11) << "Decisions"
                 << "Backtracks\n";

            for(int i = 0; i < 4; i++) {
                SolverResult result = solve(cnf, configs[i]);
                double current_time = result.total_execution_time_ms;

                cout << left << setw(12) << (configs[i].use_unit_prop ? "ON" : "OFF")
                     << setw(14) << (configs[i].use_pure_literal ? "ON" : "OFF")
                     << setw(12) << current_time;

                if(previous_time[i] > 0) {
                    double change = (current_time - previous_time[i]) * 100.0 / previous_time[i];
                    cout << setw(13) << change;
                }
                else {
                    cout << setw(13) << "-";
                }

                cout << setw(9) << (result.is_sat ? "SAT" : "UNSAT")
                     << setw(11) << result.decision_count
                     << result.backtrack_count << "\n";
                previous_time[i] = current_time;

                if(n == no_of_nodes && i == 3) {
                    if(result.is_sat) {
                        vector<pair<string, int>> values;
                        for(auto &[s, v]: result.final_assignments) {
                            values.push_back({s, v});
                        }

                        vector<int> assignment = e.decoderesult(values);
                        cout << "\nDecoded graph colouring:\n";
                        for(size_t j = 0; j < assignment.size(); j++) {
                            cout << "Node " << j + 1 << " -> Colour " << assignment[j] << "\n";
                        }
                        cout << "Valid colouring: " << (e.checkresult(values) ? "YES" : "NO") << "\n";
                    }
                    else {
                        cout << "No Valid colouring\n";
                    }
                }
            }

            try {
                start = chrono::steady_clock::now();
                TruthTableEngine engine;
                TruthTable table = engine.truth_table(*parsed);
                end = chrono::steady_clock::now();
                double table_time = time_ms(start, end);

                cout << "\nTruth table rows: " << table.num_rows() << "\n";
                cout << "Truth table generation time: " << table_time << " ms\n";
                if(previous_table_time > 0) {
                    double change = (table_time - previous_table_time) * 100.0 / previous_table_time;
                    cout << "Truth table time change: " << change << "%\n";
                }
                previous_table_time = table_time;
            }
            catch(const length_error &error) {
                cout << "\nTruth table: " << error.what() << "\n";
            }
        }
    }
};

int main() {
    int no_of_colours, no_of_nodes, no_of_edges;
    cout << "Enter number of colours: ";
    cin >> no_of_colours;
    cout << "Enter number of nodes: ";
    cin >> no_of_nodes;
    cout << "Enter number of edges: ";
    cin >> no_of_edges;

    if(no_of_colours <= 0 || no_of_nodes <= 0 || no_of_edges < 0) {
        cout << "Invalid input\n";
        return 1;
    }

    vector<pair<int, int>> edges(no_of_edges);
    for(auto &[a, b]: edges) {
        cin >> a >> b;
        if(a < 1 || a > no_of_nodes || b < 1 || b > no_of_nodes) {
            cout << "Invalid edge\n";
            return 1;
        }
    }

    try {
        Encoder e(no_of_nodes, no_of_colours, edges);
        e.build_initial_string();
        e.addedgeconstraint();
        e.printencodedstring();

        PerformanceTester tester(no_of_nodes, no_of_colours, edges);
        tester.run();
    }
    catch(const exception &error) {
        cout << "Error: " << error.what() << "\n";
        return 1;
    }
}
