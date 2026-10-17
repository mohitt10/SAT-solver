#pragma once

#include "ast.h"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class Literal {
public:
    Literal() = default;
    Literal(std::string variable, bool negated)
        : variable_(std::move(variable)), negated_(negated) {}

    const std::string& getVariable() const {
        return variable_;
    }

    bool isNegated() const {
        return negated_;
    }

    bool isEqual(const Literal& other) const {
        return variable_ == other.variable_ && negated_ == other.negated_;
    }

    bool isOpposite(const Literal& other) const {
        return variable_ == other.variable_ && negated_ != other.negated_;
    }

private:
    std::string variable_;
    bool negated_ = false;
};

class Clause {
public:
    void addLiteral(const Literal& literal) {
        for (const Literal& current : literals_) {
            if (current.isEqual(literal)) {
                return;
            }
        }
        literals_.push_back(literal);
    }

    const std::vector<Literal>& getLiterals() const {
        return literals_;
    }

    bool containsLiteral(const Literal& literal) const {
        for (const Literal& current : literals_) {
            if (current.isEqual(literal)) {
                return true;
            }
        }
        return false;
    }

    bool isEmpty() const {
        return literals_.empty();
    }

    std::size_t size() const {
        return literals_.size();
    }

    bool isTautology() const {
        for (std::size_t i = 0; i < literals_.size(); ++i) {
            for (std::size_t j = i + 1; j < literals_.size(); ++j) {
                if (literals_[i].isOpposite(literals_[j])) {
                    return true;
                }
            }
        }
        return false;
    }

private:
    std::vector<Literal> literals_;
};

class CNF {
public:
    void addClause(const Clause& clause) {
        clauses_.push_back(clause);
    }

    const std::vector<Clause>& getClauses() const {
        return clauses_;
    }

    bool isEmpty() const {
        return clauses_.empty();
    }

    std::size_t size() const {
        return clauses_.size();
    }

private:
    std::vector<Clause> clauses_;
};

struct SolverConfig {
    bool use_unit_prop = true;
    bool use_pure_literal = true;
};

struct SolverResult {
    bool is_sat = false;
    std::unordered_map<std::string, bool> final_assignments;
    double total_execution_time_ms = 0.0;
    int backtrack_count = 0;
    int decision_count = 0;
};

CNF convert_nnf_to_cnf(const NodePtr& root);
SolverResult solve(CNF initial_cnf, SolverConfig config);
