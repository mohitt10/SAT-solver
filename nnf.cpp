#include "nnf.h"
#include <stdexcept>

using namespace std;

namespace {

class NNFBuilder : public Visitor {
public:
    NodePtr build(const ASTNode& root, bool negate = false) {
        negate_ = negate;
        root.accept(*this);
        return result_;
    }

    void visit(const VarNode& node) override {
        NodePtr variable = make_shared<VarNode>(node.name);
        result_ = negate_ ? make_shared<NotNode>(move(variable))
                          : move(variable);
    }

    void visit(const NotNode& node) override {
        result_ = child(node.left, !negate_);
    }

    void visit(const AndNode& node) override {
        if (negate_) {
            result_ = make_shared<OrNode>(
                child(node.left, true), child(node.right, true));
        } else {
            result_ = make_shared<AndNode>(
                child(node.left, false), child(node.right, false));
        }
    }

    void visit(const OrNode& node) override {
        if (negate_) {
            result_ = make_shared<AndNode>(
                child(node.left, true), child(node.right, true));
        } else {
            result_ = make_shared<OrNode>(
                child(node.left, false), child(node.right, false));
        }
    }

    void visit(const ImpliesNode& node) override {
        if (negate_) {
            result_ = make_shared<AndNode>(
                child(node.left, false), child(node.right, true));
        } else {
            result_ = make_shared<OrNode>(
                child(node.left, true), child(node.right, false));
        }
    }

    void visit(const IffNode& node) override {
        NodePtr left_same = child(node.left, false);
        NodePtr right_same = child(node.right, false);
        NodePtr left_different = child(node.left, true);
        NodePtr right_different = child(node.right, true);

        if (negate_) {
            result_ = make_shared<OrNode>(
                make_shared<AndNode>(move(left_same), move(right_different)),
                make_shared<AndNode>(move(left_different), move(right_same)));
        } else {
            result_ = make_shared<OrNode>(
                make_shared<AndNode>(move(left_same), move(right_same)),
                make_shared<AndNode>(move(left_different), move(right_different)));
        }
    }

private:
    bool negate_ = false;
    NodePtr result_;

    static NodePtr child(const NodePtr& node, bool negate) {
        if (!node) {
            throw invalid_argument("malformed AST: missing child");
        }
        NNFBuilder builder;
        return builder.build(*node, negate);
    }
};

} // namespace

NodePtr NNFConverter::convert(const ASTNode& root) const {
    NNFBuilder builder;
    return builder.build(root);
}
