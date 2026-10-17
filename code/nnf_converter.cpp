#include "sat_types.hpp"

#include <stdexcept>
#include <vector>

namespace {

class NnfToCnfVisitor : public Visitor {
    std::vector<Clause> clauses;

public:
    static std::vector<Clause> convert_node(const NodePtr& node) {
        if (!node) {
            throw std::invalid_argument("Null AST node encountered");
        }

        NnfToCnfVisitor visitor;
        node->accept(visitor);
        return visitor.clauses;
    }

    void visit(const VarNode& node) override {
        Clause clause;
        clause.addLiteral(Literal(node.name, false));
        clauses.push_back(clause);
    }

    void visit(const NotNode& node) override {
        const std::shared_ptr<VarNode> child =
            std::dynamic_pointer_cast<VarNode>(node.left);
        if (!child) {
            throw std::invalid_argument("NotNode child must be a VarNode in NNF");
        }

        Clause clause;
        clause.addLiteral(Literal(child->name, true));
        clauses.push_back(clause);
    }

    void visit(const AndNode& node) override {
        std::vector<Clause> left_clauses = convert_node(node.left);
        std::vector<Clause> right_clauses = convert_node(node.right);

        left_clauses.reserve(left_clauses.size() + right_clauses.size());
        left_clauses.insert(
            left_clauses.end(), right_clauses.begin(), right_clauses.end());
        clauses = std::move(left_clauses);
    }

    void visit(const OrNode& node) override {
        std::vector<Clause> left_clauses = convert_node(node.left);
        std::vector<Clause> right_clauses = convert_node(node.right);

        clauses.reserve(left_clauses.size() * right_clauses.size());
        for (Clause& left_clause : left_clauses) {
            for (Clause& right_clause : right_clauses) {
                Clause combined;

                for (const Literal& literal : left_clause.getLiterals()) {
                    combined.addLiteral(literal);
                }
                for (const Literal& literal : right_clause.getLiterals()) {
                    combined.addLiteral(literal);
                }

                if (!combined.isTautology()) {
                    clauses.push_back(combined);
                }
            }
        }
    }

    void visit(const ImpliesNode&) override {
        throw std::invalid_argument("Implication node is not valid in NNF");
    }

    void visit(const IffNode&) override {
        throw std::invalid_argument("Biconditional node is not valid in NNF");
    }

};

}

CNF convert_nnf_to_cnf(const NodePtr& root) {
    CNF cnf;
    const std::vector<Clause> distributed = NnfToCnfVisitor::convert_node(root);
    for (const Clause& clause : distributed) {
        cnf.addClause(clause);
    }
    return cnf;
}
