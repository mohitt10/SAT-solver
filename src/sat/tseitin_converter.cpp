#include "sat_types.hpp"

#include "nnf.h"

#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>

namespace {

class VariableCollector : public Visitor {
public:
    std::unordered_set<std::string> collect(const ASTNode& root) {
        root.accept(*this);
        return variables_;
    }

    void visit(const VarNode& node) override {
        variables_.insert(node.name);
    }

    void visit(const NotNode& node) override {
        visit_child(node.left);
    }

    void visit(const AndNode& node) override {
        visit_children(node);
    }

    void visit(const OrNode& node) override {
        visit_children(node);
    }

    void visit(const ImpliesNode& node) override {
        visit_children(node);
    }

    void visit(const IffNode& node) override {
        visit_children(node);
    }

private:
    std::unordered_set<std::string> variables_;

    void visit_child(const NodePtr& child) {
        if (!child) {
            throw std::invalid_argument("malformed AST: missing child");
        }
        child->accept(*this);
    }

    void visit_children(const ASTNode& node) {
        visit_child(node.left);
        visit_child(node.right);
    }
};

Literal negate(const Literal& literal) {
    return Literal(literal.getVariable(), !literal.isNegated());
}

class TseitinEncoder : public Visitor {
public:
    explicit TseitinEncoder(std::unordered_set<std::string> used_names)
        : used_names_(std::move(used_names)) {}

    CnfEncoding encode(const ASTNode& root) {
        const Literal root_literal = encode_node(root);
        Clause root_clause;
        root_clause.addLiteral(root_literal);
        cnf_.addClause(root_clause);
        return {std::move(cnf_), std::move(original_variables_)};
    }

    void visit(const VarNode& node) override {
        original_variables_.insert(node.name);
        result_ = Literal(node.name, false);
    }

    void visit(const NotNode& node) override {
        result_ = negate(encode_child(node.left));
    }

    void visit(const AndNode& node) override {
        const Literal left = encode_child(node.left);
        const Literal right = encode_child(node.right);
        const Literal output = new_auxiliary();

        add_clause({negate(output), left});
        add_clause({negate(output), right});
        add_clause({output, negate(left), negate(right)});
        result_ = output;
    }

    void visit(const OrNode& node) override {
        const Literal left = encode_child(node.left);
        const Literal right = encode_child(node.right);
        const Literal output = new_auxiliary();

        add_clause({output, negate(left)});
        add_clause({output, negate(right)});
        add_clause({negate(output), left, right});
        result_ = output;
    }

    void visit(const ImpliesNode& node) override {
        const Literal left = negate(encode_child(node.left));
        const Literal right = encode_child(node.right);
        encode_or(left, right);
    }

    void visit(const IffNode& node) override {
        const Literal left = encode_child(node.left);
        const Literal right = encode_child(node.right);
        const Literal output = new_auxiliary();

        add_clause({negate(output), negate(left), right});
        add_clause({negate(output), negate(right), left});
        add_clause({output, negate(left), negate(right)});
        add_clause({output, left, right});
        result_ = output;
    }

private:
    CNF cnf_;
    Literal result_;
    std::unordered_set<std::string> used_names_;
    std::unordered_set<std::string> original_variables_;
    std::size_t next_auxiliary_ = 0;

    Literal encode_node(const ASTNode& node) {
        node.accept(*this);
        return result_;
    }

    Literal encode_child(const NodePtr& child) {
        if (!child) {
            throw std::invalid_argument("malformed AST: missing child");
        }
        return encode_node(*child);
    }

    Literal new_auxiliary() {
        std::string name;
        do {
            name = "__tseitin_" + std::to_string(next_auxiliary_++);
        } while (!used_names_.insert(name).second);
        return Literal(std::move(name), false);
    }

    void add_clause(std::initializer_list<Literal> literals) {
        Clause clause;
        for (const Literal& literal : literals) {
            clause.addLiteral(literal);
        }
        cnf_.addClause(clause);
    }

    void encode_or(const Literal& left, const Literal& right) {
        const Literal output = new_auxiliary();
        add_clause({output, negate(left)});
        add_clause({output, negate(right)});
        add_clause({negate(output), left, right});
        result_ = output;
    }
};

} // namespace

CnfEncoding convert_formula_to_cnf(
    const NodePtr& root,
    CnfPipeline pipeline) {
    if (!root) {
        throw std::invalid_argument("Cannot convert a null AST");
    }

    VariableCollector collector;
    const auto original_variables = collector.collect(*root);

    if (pipeline == CnfPipeline::Tseitin) {
        TseitinEncoder encoder(original_variables);
        return encoder.encode(*root);
    }

    NNFConverter nnf_converter;
    const NodePtr nnf = nnf_converter.convert(*root);
    return {convert_nnf_to_cnf(nnf), original_variables};
}
