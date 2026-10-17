#include <bits/stdc++.h>
#include "parser.h"
#include "nnf.h"
#include "truth_table.h"
#include "code/cnf_structures.hpp"
using namespace std;

// OR, AND and NOT are already used as token names in lexer.h.
const string OR_TEXT = " OR ";
const string AND_TEXT = " AND ";
const string NOT_TEXT = "NOT ";

class Encoder {
    private:
    int no_of_col;
    int no_of_nodes;

    string encoded_string;
    vector<pair<int, int>> edges;

    public:
    Encoder(int nodes, int colour) {
        no_of_col = colour;
        no_of_nodes = nodes;
    }

    Encoder(int nodes, int colour, vector<pair<int, int>> v) {
        no_of_col = colour;
        no_of_nodes = nodes;
        edges = v;
    }

    void build_initial_string() {
        encoded_string.clear();
        encoded_string.push_back('(');
        for(int i = 1; i <= no_of_nodes; i++) {
            encoded_string.push_back('(');
            for(int c = 1; c <= no_of_col; c++) {
                encoded_string += to_string(i) + "_" + to_string(c);
                if(c != no_of_col)    encoded_string += OR_TEXT;
            }
            encoded_string.push_back(')');

            // A node cannot have two colours at once.
            for(int c1 = 1; c1 <= no_of_col; c1++) {
                for(int c2 = c1 + 1; c2 <= no_of_col; c2++) {
                    encoded_string += AND_TEXT + "(" + NOT_TEXT + to_string(i) + "_" + to_string(c1)
                                    + OR_TEXT + NOT_TEXT + to_string(i) + "_" + to_string(c2) + ")";
                }
            }

            if(i != no_of_nodes)    encoded_string += AND_TEXT;
        }
        encoded_string.push_back(')');
    }

    void printencodedstring() {
        cout << encoded_string << endl;
    }

    string getencodedstring() {
        return encoded_string;
    }

    void init_edge_list(vector<pair<int, int>>& v) {
        edges = v;
    }

    void addedgeconstraint() {
        for(auto &[a, b]: edges) {
            for(int i = 1; i <= no_of_col; i++) {
                encoded_string += AND_TEXT + "(" + NOT_TEXT + to_string(a) + "_" + to_string(i)
                                + OR_TEXT + NOT_TEXT + to_string(b) + "_" + to_string(i) + ")";
            }
        }
    }

    vector<int> decoderesult(vector<pair<string, int>> result) {
        vector<int> assignment(no_of_nodes, -1);
        for(auto &[s, v]: result) {
            if(!v)    continue;

            int pos = s.find('_');
            if(pos == string::npos)    return {};

            int a = stoi(s.substr(0, pos));
            int b = stoi(s.substr(pos + 1));
            if(a < 1 || a > no_of_nodes || b < 1 || b > no_of_col)    return {};
            if(assignment[a - 1] != -1)    return {};
            assignment[a - 1] = b;
        }
        return assignment;
    }

    bool checkresult(vector<pair<string, int>> result) {
        vector<int> assignment = decoderesult(result);
        if(assignment.empty())    return false;

        for(int &x: assignment) {
            if(x == -1)    return false;
        }

        for(auto &[a, b]: edges) {
            if(assignment[a - 1] == assignment[b - 1])    return false;
        }
        return true;
    }
};

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
                        for(int j = 0; j < assignment.size(); j++) {
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
