#include "sat_types.hpp"
#include <stdexcept>
#include <vector>

// ============================================================================
// NNF to CNF Conversion (Distributive Law)
// ============================================================================

std::vector<Clause> distribute_to_clauses(ASTNode* node) {
    if (node == nullptr) {
        throw std::invalid_argument("Null AST node encountered");
    }

    if (VarNode* var = dynamic_cast<VarNode*>(node)) {
        Clause c;
        c.addLiteral(Literal(var->name, false));
        return std::vector<Clause>{c};
    }

    if (NotNode* neg = dynamic_cast<NotNode*>(node)) {
        VarNode* child = dynamic_cast<VarNode*>(neg->left);
        if (child == nullptr) {
            throw std::invalid_argument("NotNode child must be a VarNode in NNF");
        }
        Clause c;
        c.addLiteral(Literal(child->name, true));
        return std::vector<Clause>{c};
    }

    if (AndNode* conj = dynamic_cast<AndNode*>(node)) {
        std::vector<Clause> left_clauses = distribute_to_clauses(conj->left);
        std::vector<Clause> right_clauses = distribute_to_clauses(conj->right);
        
        left_clauses.reserve(left_clauses.size() + right_clauses.size());
        left_clauses.insert(left_clauses.end(), right_clauses.begin(), right_clauses.end());
        return left_clauses;
    }

    if (OrNode* disj = dynamic_cast<OrNode*>(node)) {
        std::vector<Clause> left_clauses = distribute_to_clauses(disj->left);
        std::vector<Clause> right_clauses = distribute_to_clauses(disj->right);

        std::vector<Clause> result;
        result.reserve(left_clauses.size() * right_clauses.size());

        for (const Clause& c_left : left_clauses) {
            for (const Clause& c_right : right_clauses) {
                Clause c_combined;
                
                for (const Literal& lit : c_left.getLiterals()) {
                    c_combined.addLiteral(lit);
                }
                for (const Literal& lit : c_right.getLiterals()) {
                    c_combined.addLiteral(lit);
                }

                if (!c_combined.isTautology()) {
                    result.push_back(c_combined);
                }
            }
        }
        return result;
    }

    throw std::invalid_argument("Unsupported node type in NNF tree");
}

CNF convert_nnf_to_cnf(ASTNode* root) {
    CNF cnf;
    std::vector<Clause> distributed = distribute_to_clauses(root);
    for (const Clause& clause : distributed) {
        cnf.addClause(clause);
    }
    return cnf;
}