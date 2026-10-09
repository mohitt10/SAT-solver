#include "truth_table.h"
#include <set>
#include <stdexcept>
#include <unordered_map>

std::string to_string(Classification c) {
    switch (c) {
        case Classification::TAUTOLOGY:     return "Tautology";
        case Classification::CONTRADICTION: return "Contradiction";
        case Classification::CONTINGENCY:   return "Contingency";
    }
    return "?";
}

Assignment TruthTable::assignment(uint64_t row) const {
    Assignment a;
    size_t n = variables.size();
    for (size_t k = 0; k < n; ++k)
        a[variables[k]] = (row >> (n - 1 - k)) & 1ULL;
    return a;
}

namespace {

// ---- Visitor 1: collect variables in order of first appearance ----
class VarCollector : public Visitor {
public:
    std::vector<std::string> vars;
    void visit(const VarNode& n) override {
        if (seen_.insert(n.name).second) vars.push_back(n.name);
    }
    void visit(const NotNode& n) override     { walk(n.left); }
    void visit(const AndNode& n) override     { binary(n); }
    void visit(const OrNode& n) override      { binary(n); }
    void visit(const ImpliesNode& n) override { binary(n); }
    void visit(const IffNode& n) override     { binary(n); }
private:
    std::set<std::string> seen_;
    void walk(const NodePtr& p) {
        if (!p) throw std::invalid_argument("malformed AST: missing child");
        p->accept(*this);
    }
    void binary(const ASTNode& n) { walk(n.left); walk(n.right); }
};

// ---- Visitor 2: evaluate 64 rows at once (bit j of a word = row j of the block) ----
class BlockEvaluator : public Visitor {
public:
    BlockEvaluator(const std::unordered_map<std::string, size_t>& idx,
                   const std::vector<uint64_t>& patterns)
        : idx_(idx), pats_(patterns) {}

    uint64_t eval(const ASTNode& root) { root.accept(*this); return result_; }

    void visit(const VarNode& n) override {
        auto it = idx_.find(n.name);
        if (it == idx_.end()) throw std::runtime_error("unbound variable: " + n.name);
        result_ = pats_[it->second];
    }
    void visit(const NotNode& n) override     { result_ = ~sub(n.left); }
    void visit(const AndNode& n) override     { uint64_t a = sub(n.left), b = sub(n.right); result_ = a & b; }
    void visit(const OrNode& n) override      { uint64_t a = sub(n.left), b = sub(n.right); result_ = a | b; }
    void visit(const ImpliesNode& n) override { uint64_t a = sub(n.left), b = sub(n.right); result_ = ~a | b; }
    void visit(const IffNode& n) override     { uint64_t a = sub(n.left), b = sub(n.right); result_ = ~(a ^ b); }

private:
    const std::unordered_map<std::string, size_t>& idx_;
    const std::vector<uint64_t>& pats_;
    uint64_t result_ = 0;
    uint64_t sub(const NodePtr& p) {
        if (!p) throw std::invalid_argument("malformed AST: missing child");
        p->accept(*this);
        return result_;
    }
};

// Pattern of a variable whose bit weight is 2^s, across 64 consecutive rows
// starting at a multiple of 64 (valid for s < 6).
const uint64_t kLowPattern[6] = {
    0xAAAAAAAAAAAAAAAAULL, 0xCCCCCCCCCCCCCCCCULL, 0xF0F0F0F0F0F0F0F0ULL,
    0xFF00FF00FF00FF00ULL, 0xFFFF0000FFFF0000ULL, 0xFFFFFFFF00000000ULL};

std::unordered_map<std::string, size_t> index_of(const std::vector<std::string>& vars) {
    std::unordered_map<std::string, size_t> idx;
    for (size_t k = 0; k < vars.size(); ++k) idx[vars[k]] = k;
    return idx;
}

} // namespace

std::vector<std::string> TruthTableEngine::collect_variables(const ASTNode& root) const {
    VarCollector c;
    root.accept(c);
    return c.vars;
}

bool TruthTableEngine::evaluate(const ASTNode& root, const Assignment& a) const {
    std::vector<std::string> vars = collect_variables(root);
    std::vector<uint64_t> pats(vars.size());
    for (size_t k = 0; k < vars.size(); ++k) {
        auto it = a.find(vars[k]);
        if (it == a.end()) throw std::runtime_error("unbound variable: " + vars[k]);
        pats[k] = it->second ? ~0ULL : 0ULL;
    }
    auto idx = index_of(vars);
    BlockEvaluator ev(idx, pats);
    return ev.eval(root) & 1ULL;
}

BitVector TruthTableEngine::build_results(const ASTNode& root,
                                          const std::vector<std::string>& vars) const {
    const size_t n = vars.size();
    if (n > max_vars_ || n > 62)
        throw std::length_error("too many variables (" + std::to_string(n) +
                                "); limit is " + std::to_string(max_vars_) +
                                " because the table has 2^n rows");

    const uint64_t rows   = 1ULL << n;
    const uint64_t blocks = (rows + 63) / 64;
    const uint64_t valid  = rows >= 64 ? ~0ULL : ((1ULL << rows) - 1);  // mask off unused bits

    BitVector out(rows);
    auto idx = index_of(vars);
    std::vector<uint64_t> pats(n);
    BlockEvaluator ev(idx, pats);

    for (uint64_t b = 0; b < blocks; ++b) {
        const uint64_t base = b * 64;
        for (size_t k = 0; k < n; ++k) {
            const size_t s = n - 1 - k;                 // bit weight of variable k in the row index
            pats[k] = (s < 6) ? kLowPattern[s]
                              : (((base >> s) & 1ULL) ? ~0ULL : 0ULL);
        }
        out.set_word(b, ev.eval(root) & valid);
    }
    return out;
}

TruthTable TruthTableEngine::truth_table(const ASTNode& root) const {
    TruthTable t;
    t.variables = collect_variables(root);
    t.results   = build_results(root, t.variables);
    return t;
}

Classification TruthTableEngine::classify(const TruthTable& t) const {
    uint64_t ones = t.results.count_ones();
    if (ones == t.num_rows()) return Classification::TAUTOLOGY;
    if (ones == 0)            return Classification::CONTRADICTION;
    return Classification::CONTINGENCY;
}

bool TruthTableEngine::are_equivalent(const ASTNode& a, const ASTNode& b) const {
    std::vector<std::string> vars = collect_variables(a);
    std::set<std::string> have(vars.begin(), vars.end());
    for (const auto& v : collect_variables(b))
        if (have.insert(v).second) vars.push_back(v);
    return build_results(a, vars) == build_results(b, vars);
}

void print_truth_table(std::ostream& os, const TruthTable& t) {
    const size_t n = t.variables.size();
    for (const auto& v : t.variables) os << v << ' ';
    os << "| F\n";
    for (size_t i = 0; i < n; ++i) os << "--";
    os << "+--\n";
    for (uint64_t r = 0; r < t.num_rows(); ++r) {
        for (size_t k = 0; k < n; ++k) {
            bool bit = (r >> (n - 1 - k)) & 1ULL;
            os << (bit ? 'T' : 'F') << std::string(t.variables[k].size(), ' ');
        }
        os << "| " << (t.value(r) ? 'T' : 'F') << '\n';
    }
}
