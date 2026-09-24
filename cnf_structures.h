#ifndef CNF_STRUCTURES_H
#define CNF_STRUCTURES_H

#include <string>
#include <vector>
#include <unordered_set>
#include <functional>

// ============================================================
// Literal
// Represents a variable or its negation.
//
// Examples:
//   A   -> Literal("A", false)
//   ~A  -> Literal("A", true)
// ============================================================

class Literal {
private:
    std::string variable;
    bool negated;

public:
    Literal(const std::string& variable, bool negated = false)
        : variable(variable), negated(negated) {}

    // -------------------------
    // Getters
    // -------------------------

    const std::string& getVariable() const {
        return variable;
    }

    bool isNegated() const {
        return negated;
    }

    // -------------------------
    // Equality
    // -------------------------

    bool operator==(const Literal& other) const {
        return variable == other.variable &&
               negated == other.negated;
    }

    bool operator!=(const Literal& other) const {
        return !(*this == other);
    }
};


// ============================================================
// Hash function for Literal
//
// Required because Literal is stored inside unordered_set.
// ============================================================

struct LiteralHash {
    std::size_t operator()(const Literal& literal) const {

        std::size_t variableHash =
            std::hash<std::string>{}(literal.getVariable());

        std::size_t negationHash =
            std::hash<bool>{}(literal.isNegated());

        // Combine the two hashes
        return variableHash ^ (negationHash << 1);
    }
};


// ============================================================
// Clause
//
// Represents an OR of literals.
//
// Example:
//     (A OR ~B OR C)
//
// Internally:
//     unordered_set { A, ~B, C }
// ============================================================

class Clause {
private:
    std::unordered_set<Literal, LiteralHash> literals;

public:

    // -------------------------
    // Add a literal
    // -------------------------

    void addLiteral(const Literal& literal) {
        literals.insert(literal);
    }

    // -------------------------
    // Get all literals
    // -------------------------

    const std::unordered_set<Literal, LiteralHash>&
    getLiterals() const {
        return literals;
    }

    // -------------------------
    // Check if literal exists
    //
    // O(1) average
    // -------------------------

    bool containsLiteral(const Literal& literal) const {
        return literals.find(literal) != literals.end();
    }

    // -------------------------
    // Check if clause is tautological
    //
    // Example:
    //     (A OR ~A OR B)
    //
    // This clause is always TRUE.
    // -------------------------

    bool isTautology() const {

        for (const Literal& literal : literals) {

            // Create the opposite literal
            Literal opposite(
                literal.getVariable(),
                !literal.isNegated()
            );

            if (containsLiteral(opposite)) {
                return true;
            }
        }

        return false;
    }

    // -------------------------
    // Check if clause is empty
    //
    // Empty clause = FALSE
    // Important for DPLL.
    // -------------------------

    bool isEmpty() const {
        return literals.empty();
    }

    // -------------------------
    // Number of literals
    // -------------------------

    std::size_t size() const {
        return literals.size();
    }
};


// ============================================================
// CNF
//
// Represents an AND of clauses.
//
// Example:
//
//     (A OR B) AND (~C) AND (D OR ~E)
//
// Internally:
//
//     vector<Clause>
// ============================================================

class CNF {
private:
    std::vector<Clause> clauses;

public:

    // -------------------------
    // Add a clause
    // -------------------------

    void addClause(const Clause& clause) {
        clauses.push_back(clause);
    }

    // -------------------------
    // Get all clauses
    // -------------------------

    const std::vector<Clause>& getClauses() const {
        return clauses;
    }

    // -------------------------
    // Number of clauses
    // -------------------------

    std::size_t size() const {
        return clauses.size();
    }

    // -------------------------
    // Check if CNF has no clauses
    //
    // Empty CNF = TRUE
    // -------------------------

    bool isEmpty() const {
        return clauses.empty();
    }
};

#endif