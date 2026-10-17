#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <unordered_map>

using namespace std;

struct VarNode;
struct NotNode;
struct AndNode;
struct OrNode;

class Visitor {
public:
    virtual ~Visitor() = default;
    virtual void visit(const VarNode&) = 0;
    virtual void visit(const NotNode&) = 0;
    virtual void visit(const AndNode&) = 0;
    virtual void visit(const OrNode&) = 0;
};

class ASTNode;
using NodePtr = shared_ptr<ASTNode>;

class ASTNode {
public:
    NodePtr left, right;
    virtual ~ASTNode() = default;
    virtual void accept(Visitor& v) const = 0;

protected:
    explicit ASTNode(NodePtr l = nullptr, NodePtr r = nullptr)
        : left(move(l)), right(move(r)) {}
};

struct VarNode : ASTNode {
    string name;
    explicit VarNode(string n) : name(move(n)) {}
    void accept(Visitor& v) const override { v.visit(*this); }
};

struct NotNode : ASTNode {
    explicit NotNode(NodePtr c) : ASTNode(move(c)) {}
    void accept(Visitor& v) const override { v.visit(*this); }
};

struct AndNode : ASTNode {
    AndNode(NodePtr l, NodePtr r) : ASTNode(move(l), move(r)) {}
    void accept(Visitor& v) const override { v.visit(*this); }
};

struct OrNode : ASTNode {
    OrNode(NodePtr l, NodePtr r) : ASTNode(move(l), move(r)) {}
    void accept(Visitor& v) const override { v.visit(*this); }
};

inline NodePtr mkVar(const string& n) {
    return make_shared<VarNode>(n);
}

inline NodePtr mkNot(NodePtr a) {
    return make_shared<NotNode>(move(a));
}

inline NodePtr mkAnd(NodePtr a, NodePtr b) {
    return make_shared<AndNode>(move(a), move(b));
}

inline NodePtr mkOr(NodePtr a, NodePtr b) {
    return make_shared<OrNode>(move(a), move(b));
}


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

    string getVariable() { 
        return variable; 
    }
    bool isNegated() { 
        return negated; 
    }

    bool isEqual(Literal other) {
        if(variable == other.variable && negated == other.negated) {
            return true;
        }
        return false;
    }

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

    vector<Literal> getLiterals() { 
        return literals; 
    }

    bool containsLiteral(Literal lit) {
        for (int i = 0; i < literals.size(); i++) {
            if (literals[i].isEqual(lit)) {
                return true;
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