#pragma once

#include "../ast.h"

#include <string>
#include <vector>
#include <unordered_map>

using namespace std;

//CNF Data Structures
class Literal {
    string variable;
    bool negated;

public:
    Literal() {} 
    
    Literal(string var, bool is_negated) {
        variable = var;
        negated = is_negated;
    }

    string getVariable() { return variable; }
    bool isNegated() { return negated; }

    // Replaces operator==
    bool isEqual(Literal other) {
        if(variable == other.variable && negated == other.negated) {
            return true;
        }
        return false;
    }

    // Helper to easily check if A is opposite of ~A
    bool isOpposite(Literal other) {
        if(variable == other.variable && negated != other.negated) {
            return true;
        }
        return false;
    }
};

class Clause {
    vector<Literal> literals;

public:
    void addLiteral(Literal lit) {
        // Only add if it doesn't already exist in our vector
        for (int i = 0; i < literals.size(); i++) {
            if (literals[i].isEqual(lit)) {
                return; 
            }
        }
        literals.push_back(lit);
    }

    vector<Literal> getLiterals() { return literals; }

    bool containsLiteral(Literal lit) {
        for (int i = 0; i < literals.size(); i++) {
            if (literals[i].isEqual(lit)) {
                return true;
            }
        }
        return false;
    }

    bool isTautology() {
        for (int i = 0; i < literals.size(); ++i) {
            for (int j = i + 1; j < literals.size(); ++j) {
                if (literals[i].isOpposite(literals[j])) {
                    return true;
                }
            }
        }
        return false;
    }

    bool isEmpty() { 
        return literals.empty(); 
    }
    
    int size() { 
        return literals.size(); 
    }
};

class CNF {
    vector<Clause> clauses;

public:
    void addClause(Clause clause) {
        clauses.push_back(clause);
    }

    vector<Clause> getClauses() { 
        return clauses; 
    }

    bool isEmpty() { 
        return clauses.empty(); 
    }
    
    int size() { 
        return clauses.size(); 
    }
};

//Solver API
struct SolverConfig {
    bool use_unit_prop = true;
    bool use_pure_literal = true;
};

struct SolverResult {
    bool is_sat = false;
    unordered_map<string, bool> final_assignments;
    double total_execution_time_ms = 0.0;
    int backtrack_count = 0;
    int decision_count = 0;
};


// Function declarations
CNF convert_nnf_to_cnf(const NodePtr& root);
SolverResult solve(CNF initial_cnf, SolverConfig config);