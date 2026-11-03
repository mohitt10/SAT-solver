#include <bits/stdc++.h>
using namespace std;

// Global Sudoku size
const int N = 9;

const string AND = " AND ";
const string OR  = " OR ";
const string NOT = "NOT ";

string getVar(int cell, int digit) {
    return to_string(cell) + "_" + to_string(digit);
}

void addExactlyOne(string &formula, vector<string> vars) {

    // At least one
    formula += "(";

    for (int i = 0; i < N; i++) {
        if (i) formula += OR;
        formula += vars[i];
    }

    formula += ")";

    // At most one
    for (int i = 0; i < N; i++) {
        for (int j = i + 1; j < N; j++) {
            formula += AND;
            formula += "(" + NOT + vars[i] + OR
                       + NOT + vars[j] + ")";
        }
    }
}

int main() {

    int B = sqrt(N);

    if (B * B != N) {
        cout << "Sudoku size must be a perfect square\n";
        return 0;
    }

    string formula = "";

    int val;
    cout << "Enter number of given values: ";
    cin >> val;
    cout << "Enter square_number value for each given value:\n";
    while(val--) {
        int a, b;
        cin >> a >> b;

        formula += to_string(a) + '_' + to_string(b) + AND;
    }
    for (int cell = 1; cell <= N * N; cell++) {

        vector<string> vars;

        for (int digit = 1; digit <= N; digit++)
            vars.push_back(getVar(cell, digit));

        addExactlyOne(formula, vars);
        formula += AND;
    }

    // 2. Every digit occurs exactly once in each row
    for (int row = 0; row < N; row++) {
        for (int digit = 1; digit <= N; digit++) {

            vector<string> vars;

            for (int col = 0; col < N; col++) {
                int cell = row * N + col + 1;
                vars.push_back(getVar(cell, digit));
            }

            addExactlyOne(formula, vars);
            formula += AND;
        }
    }

    // 3. Every digit occurs exactly once in each column
    for (int col = 0; col < N; col++) {
        for (int digit = 1; digit <= N; digit++) {

            vector<string> vars;

            for (int row = 0; row < N; row++) {
                int cell = row * N + col + 1;
                vars.push_back(getVar(cell, digit));
            }

            addExactlyOne(formula, vars);
            formula += AND;
        }
    }

    // 4. Every digit occurs exactly once in each box
    for (int br = 0; br < N; br += B) {
        for (int bc = 0; bc < N; bc += B) {
            for (int digit = 1; digit <= N; digit++) {

                vector<string> vars;

                for (int r = 0; r < B; r++) {
                    for (int c = 0; c < B; c++) {

                        int row = br + r;
                        int col = bc + c;

                        int cell = row * N + col + 1;

                        vars.push_back(getVar(cell, digit));
                    }
                }

                addExactlyOne(formula, vars);
                formula += AND;
            }
        }
    }

    if (!formula.empty())
        formula.resize(formula.size() - AND.size());

    cout << formula << '\n';

    return 0;
}