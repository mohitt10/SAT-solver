#pragma once

#include "ast.h" // Includes Visitor, ASTNode, etc. from Worklet 1

#include <string>
#include <utility>
#include <vector>
#include <unordered_map>
#include <unordered_set>

using namespace std;

// --- CNF Data Structures ---

class Literal {
    string variable;
    bool negated = false;

public:
    Literal() = default; 
    
    Literal(string var, bool is_negated) {
        variable = move(var);
        negated = is_negated;
    }

    // Marked const so tseitin_converter can read from const references
    const string& getVariable() const { 
        return variable; 
    }
    
    // Marked const so tseitin_converter can read from const references
    bool isNegated() const { 
        return negated; 
    }

    bool isEqual(const Literal& other) const {
        return variable == other.variable && negated == other.negated;
    }

    bool isOpposite(const Literal& other) const {
        return variable == other.variable && negated != other.negated;
    }
};

class Clause {
    vector<Literal> literals;

public:
    void addLiteral(const Literal& lit) {
        for (int i = 0; i < literals.size(); i++) {
            if (literals[i].isEqual(lit)) {
                return; 
            }
        }
        literals.push_back(lit);
    }

    const vector<Literal>& getLiterals() const { 
        return literals; 
    }

    bool containsLiteral(const Literal& lit) const {
        for (int i = 0; i < literals.size(); i++) {
            if (literals[i].isEqual(lit)) {
                return true;
            }
        }
        return false;
    }

    bool isEmpty() const { 
        return literals.empty(); 
    }
    
    int size() const { 
        return literals.size(); 
    }
};

class CNF {
    vector<Clause> clauses;

public:
    void addClause(const Clause& clause) {
        clauses.push_back(clause);
    }

    const vector<Clause>& getClauses() const { 
        return clauses; 
    }

    bool isEmpty() const { 
        return clauses.empty(); 
    }
    
    int size() const { 
        return clauses.size(); 
    }
};

// --- Solver API & Configurations ---

struct SolverConfig {
    bool use_unit_prop = true;
    bool use_pure_literal = true;
    bool record_trace = false;
    size_t trace_limit = 10000;
};

enum class SolverEventType {
    Decision,
    UnitPropagation,
    PureLiteral,
    Conflict,
    Backtrack,
    Result
};

struct SolverTraceEvent {
    SolverEventType type = SolverEventType::Result;
    std::string variable;
    bool value = false;
    int decision_level = 0;
    std::vector<Literal> clause;
    std::vector<std::pair<std::string, bool>> undone_assignments;
};

struct SolverResult {
    bool is_sat = false;
    unordered_map<string, bool> final_assignments;
    double total_execution_time_ms = 0.0;
    int backtrack_count = 0;
    int decision_count = 0;
    std::vector<SolverTraceEvent> trace;
    bool trace_truncated = false;
};

// --- Integration & Pipeline Structs ---

enum class CnfPipeline {
    Nnf,
    Tseitin
};

struct CnfEncoding {
    CNF cnf;
    unordered_set<string> original_variables;
};

// --- Function Declarations (Must be at the very bottom) ---

CNF convert_nnf_to_cnf(const NodePtr& root);
CnfEncoding convert_formula_to_cnf(const NodePtr& root, CnfPipeline pipeline);
SolverResult solve(CNF initial_cnf, SolverConfig config);