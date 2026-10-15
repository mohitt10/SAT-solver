// truth_table.h -- Truth table generation, classification, equivalence (Module 1, FR-1.3..1.5)
//
// Row order  : binary counting, first variable = most significant bit
//              (row 0 = all FALSE ... row 2^n-1 = all TRUE).
// Var order  : order of first appearance in the formula (left-to-right).
// Evaluation : bit-parallel -- the AST is walked once per block of 64 rows.
#pragma once
#include "ast.h"
#include <cstdint>
#include <map>
#include <ostream>
#include <string>
#include <vector>

using Assignment = std::map<std::string, bool>;

enum class Classification { TAUTOLOGY, CONTRADICTION, CONTINGENCY };
std::string to_string(Classification c);

// Fixed-size packed bit array (bit i = truth value of row i).
// Invariant: unused high bits of the last word are always 0.
class BitVector {
public:
    BitVector() = default;
    explicit BitVector(uint64_t nbits) : nbits_(nbits), words_((nbits + 63) / 64, 0) {}

    uint64_t size() const { return nbits_; }
    bool get(uint64_t i) const { return (words_[i >> 6] >> (i & 63)) & 1ULL; }
    void set(uint64_t i, bool v) {
        if (v) words_[i >> 6] |=  (1ULL << (i & 63));
        else   words_[i >> 6] &= ~(1ULL << (i & 63));
    }
    void set_word(size_t w, uint64_t value) { words_[w] = value; }
    uint64_t count_ones() const {
        uint64_t total = 0;
        for (uint64_t w : words_) while (w) { w &= (w - 1); ++total; }
        return total;
    }
    bool operator==(const BitVector& o) const { return nbits_ == o.nbits_ && words_ == o.words_; }
    bool operator!=(const BitVector& o) const { return !(*this == o); }

private:
    uint64_t nbits_ = 0;
    std::vector<uint64_t> words_;
};

struct TruthTable {
    std::vector<std::string> variables;  // column order
    BitVector results;                   // one bit per row

    uint64_t num_rows() const { return results.size(); }
    bool value(uint64_t row) const { return results.get(row); }
    // Variable assignment represented by `row`.
    Assignment assignment(uint64_t row) const;
};

class TruthTableEngine {
public:
    // max_vars guards against the 2^n blow-up (default 24 => ~16.7M rows, 2 MB of bits).
    explicit TruthTableEngine(unsigned max_vars = 24) : max_vars_(max_vars) {}

    // Variables in order of first appearance.
    std::vector<std::string> collect_variables(const ASTNode& root) const;

    // Evaluate under one assignment. Throws std::runtime_error if a variable is unbound.
    bool evaluate(const ASTNode& root, const Assignment& a) const;

    // Full truth table. Throws std::length_error if #vars > max_vars.
    TruthTable truth_table(const ASTNode& root) const;

    Classification classify(const TruthTable& t) const;

    // Logical equivalence over the UNION of both formulas' variables.
    bool are_equivalent(const ASTNode& a, const ASTNode& b) const;

private:
    unsigned max_vars_;
    BitVector build_results(const ASTNode& root, const std::vector<std::string>& vars) const;
};

// Pretty-print as a T/F grid. Intended for small tables (GUI/CLI display).
void print_truth_table(std::ostream& os, const TruthTable& t);
