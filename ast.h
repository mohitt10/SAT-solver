// ast.h -- shared Worklet 1 AST, using an abstract node and visitor dispatch.
//
// Conventions assumed:
//   VarNode      : name
//   NotNode      : operand stored in `left`  (right == nullptr)
//   And/Or/Implies/Iff : `left` op `right`   (Implies: left -> right)
#pragma once
#include <memory>
#include <string>
#include <utility>

struct VarNode; struct NotNode; struct AndNode;
struct OrNode;  struct ImpliesNode; struct IffNode;

class Visitor {
public:
    virtual ~Visitor() = default;
    virtual void visit(const VarNode&)     = 0;
    virtual void visit(const NotNode&)     = 0;
    virtual void visit(const AndNode&)     = 0;
    virtual void visit(const OrNode&)      = 0;
    virtual void visit(const ImpliesNode&) = 0;
    virtual void visit(const IffNode&)     = 0;
};

class ASTNode;
using NodePtr = std::shared_ptr<ASTNode>;

class ASTNode {
public:
    NodePtr left, right;
    virtual ~ASTNode() = default;
    virtual void accept(Visitor& v) const = 0;
protected:
    explicit ASTNode(NodePtr l = nullptr, NodePtr r = nullptr)
        : left(std::move(l)), right(std::move(r)) {}
};

struct VarNode : ASTNode {
    std::string name;
    explicit VarNode(std::string n) : name(std::move(n)) {}
    void accept(Visitor& v) const override { v.visit(*this); }
};
struct NotNode : ASTNode {
    explicit NotNode(NodePtr c) : ASTNode(std::move(c)) {}
    void accept(Visitor& v) const override { v.visit(*this); }
};
struct AndNode : ASTNode {
    AndNode(NodePtr l, NodePtr r) : ASTNode(std::move(l), std::move(r)) {}
    void accept(Visitor& v) const override { v.visit(*this); }
};
struct OrNode : ASTNode {
    OrNode(NodePtr l, NodePtr r) : ASTNode(std::move(l), std::move(r)) {}
    void accept(Visitor& v) const override { v.visit(*this); }
};
struct ImpliesNode : ASTNode {
    ImpliesNode(NodePtr l, NodePtr r) : ASTNode(std::move(l), std::move(r)) {}
    void accept(Visitor& v) const override { v.visit(*this); }
};
struct IffNode : ASTNode {
    IffNode(NodePtr l, NodePtr r) : ASTNode(std::move(l), std::move(r)) {}
    void accept(Visitor& v) const override { v.visit(*this); }
};

// Convenience builders (handy for tests)
inline NodePtr mkVar(const std::string& n)      { return std::make_shared<VarNode>(n); }
inline NodePtr mkNot(NodePtr a)                 { return std::make_shared<NotNode>(std::move(a)); }
inline NodePtr mkAnd(NodePtr a, NodePtr b)      { return std::make_shared<AndNode>(std::move(a), std::move(b)); }
inline NodePtr mkOr(NodePtr a, NodePtr b)       { return std::make_shared<OrNode>(std::move(a), std::move(b)); }
inline NodePtr mkImplies(NodePtr a, NodePtr b)  { return std::make_shared<ImpliesNode>(std::move(a), std::move(b)); }
inline NodePtr mkIff(NodePtr a, NodePtr b)      { return std::make_shared<IffNode>(std::move(a), std::move(b)); }
