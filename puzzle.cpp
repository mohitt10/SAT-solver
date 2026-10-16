#include <bits/stdc++.h>
using namespace std;

const string OR = " OR ";
const string AND = " AND ";
const string NOT = " NOT ";

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
                if(c != no_of_col)  encoded_string += OR;
            }
            encoded_string.push_back(')');
            
            for(int c1 = 1; c1 <= no_of_col; c1++) {
                encoded_string += AND;
                encoded_string.push_back('(');
                bool flag = 0;
                for(int c2 = 1; c2 <= no_of_col; c2++) {
                    if(c1 == c2)    continue;
                    if(flag)    encoded_string += OR;
                    flag = 1;
                    encoded_string += NOT + to_string(i) + "_" + to_string(c2);
                }
                encoded_string.push_back(')');
            }
            
            if(i != no_of_nodes)    encoded_string += AND;
        }
        encoded_string.push_back(')');
    }
    
    void printencodedstring() {
        cout << encoded_string << endl;
    }
    
    void init_edge_list(vector<pair<int, int>>& v) {
        edges = v;
    }
    
    void addedgeconstraint() {
        for(auto &[a, b]: edges) {
            for(int i = 1; i <= no_of_col; i++) {
                encoded_string += AND + "(" + NOT + to_string(a) + "_" + to_string(i) + OR + NOT + to_string(b) + "_" + to_string(i) + ")";
            }   
        }
    }
    
    bool checkresult(vector<pair<string, int>> result) {
        vector<int> assignment(no_of_nodes, -1);
        for (auto &[s, v] : result) {
            if (!v)
                continue;
            
            int a = 0, b = 0;
            bool u = false;
            
            for (char &c : s) {
                if (u) {
                    a = a * 10 + c - '0';
                }
                else if (c == '_') {
                    u = true;
                }
                else {
                    b = b * 10 + c - '0';
                }
            }
            
            if (assignment[a - 1] != -1)
                return false;
            assignment[a - 1] = b;
        }
        
        for (int &x : assignment) {
            if (x == -1)
                return false;
        }
        
        for (auto &[a, b] : edges) {
            if (assignment[a - 1] == assignment[b - 1])
                return false;
        }
        
        return true;
    }
};

int main() {
    
    vector<pair<int, int>> v = {{1, 2}, {1, 3}, {2, 3}};
    Encoder e(3, 3, v);
    e.build_initial_string();
    e.addedgeconstraint();
    e.printencodedstring();
    cout << e.checkresult({ { "1_1", true }, { "2_2", true }  });
}
