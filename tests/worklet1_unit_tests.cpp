#include "ast.h"
#include "lexer.h"
#include "nnf.h"
#include "parse_error.h"
#include "parser.h"
#include "truth_table.h"

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

namespace {

int g_total_assertions = 0;
int g_total_suites = 0;

void require(bool condition, const string& message) {
    ++g_total_assertions;
    if (!condition) {
        throw runtime_error(message);
    }
}

// ---------------------------------------------------------------------------
// Helper Visitors for AST Testing
// ---------------------------------------------------------------------------

class NNFValidator : public Visitor {
public:
    void validate(const ASTNode& node) { node.accept(*this); }

    void visit(const VarNode&) override {}

    void visit(const NotNode& node) override {
        require(node.left != nullptr && node.right == nullptr,
                "NNF negation must have left child and null right child");
        require(dynamic_cast<const VarNode*>(node.left.get()) != nullptr,
                "NNF negation must apply directly to a variable");
    }

    void visit(const AndNode& node) override { binary(node); }
    void visit(const OrNode& node) override { binary(node); }

    void visit(const ImpliesNode&) override {
        throw runtime_error("NNF must not contain implication");
    }

    void visit(const IffNode&) override {
        throw runtime_error("NNF must not contain biconditional");
    }

private:
    void binary(const ASTNode& node) {
        require(node.left != nullptr && node.right != nullptr,
                "NNF binary node must have both children");
        node.left->accept(*this);
        node.right->accept(*this);
    }
};

class NodeCountVisitor : public Visitor {
public:
    int var_count = 0;
    int not_count = 0;
    int and_count = 0;
    int or_count = 0;
    int implies_count = 0;
    int iff_count = 0;

    void count(const ASTNode& node) { node.accept(*this); }

    void visit(const VarNode&) override { ++var_count; }
    void visit(const NotNode& n) override {
        ++not_count;
        if (n.left) n.left->accept(*this);
    }
    void visit(const AndNode& n) override {
        ++and_count;
        if (n.left) n.left->accept(*this);
        if (n.right) n.right->accept(*this);
    }
    void visit(const OrNode& n) override {
        ++or_count;
        if (n.left) n.left->accept(*this);
        if (n.right) n.right->accept(*this);
    }
    void visit(const ImpliesNode& n) override {
        ++implies_count;
        if (n.left) n.left->accept(*this);
        if (n.right) n.right->accept(*this);
    }
    void visit(const IffNode& n) override {
        ++iff_count;
        if (n.left) n.left->accept(*this);
        if (n.right) n.right->accept(*this);
    }
};

// ---------------------------------------------------------------------------
// SUITE 1: Lexer (Tokens, Keywords, Whitespace, Positions, Errors)
// ---------------------------------------------------------------------------
void test_suite_lexer() {
    cout << "  [Suite] Lexer (Tokens, Keywords, Positions, Lexical Errors)...\n";

    // 1. Basic tokens, types and values
    {
        const auto tokens = tokenize("A & B | ~C -> D <-> (E)");
        require(tokens.size() == 12, "expected 12 tokens for full operator set");
        require(tokens[0].type == VARIABLE && tokens[0].value == "A", "token 0 must be Var A");
        require(tokens[1].type == AND && tokens[1].value == "&", "token 1 must be AND");
        require(tokens[2].type == VARIABLE && tokens[2].value == "B", "token 2 must be Var B");
        require(tokens[3].type == OR && tokens[3].value == "|", "token 3 must be OR");
        require(tokens[4].type == NOT && tokens[4].value == "~", "token 4 must be NOT");
        require(tokens[5].type == VARIABLE && tokens[5].value == "C", "token 5 must be Var C");
        require(tokens[6].type == IMPLIES && tokens[6].value == "->", "token 6 must be IMPLIES");
        require(tokens[7].type == VARIABLE && tokens[7].value == "D", "token 7 must be Var D");
        require(tokens[8].type == IFF && tokens[8].value == "<->", "token 8 must be IFF");
        require(tokens[9].type == LPAREN && tokens[9].value == "(", "token 9 must be LPAREN");
        require(tokens[10].type == VARIABLE && tokens[10].value == "E", "token 10 must be Var E");
        require(tokens[11].type == RPAREN && tokens[11].value == ")", "token 11 must be RPAREN");
    }

    // 2. Variable naming rules (alphanumeric start, digits, underscores)
    {
        const auto tokens = tokenize("2nd_var & 10 | red_2 var_test_3");
        require(tokens[0].type == VARIABLE && tokens[0].value == "2nd_var", "variable starting with digit");
        require(tokens[2].type == VARIABLE && tokens[2].value == "10", "pure numeric variable name");
        require(tokens[4].type == VARIABLE && tokens[4].value == "red_2", "variable with underscore");
        require(tokens[5].type == VARIABLE && tokens[5].value == "var_test_3", "variable with alphanumeric and underscore");
    }

    // 3. Word-form operators: and, or, implies, iff, not (case-insensitive)
    {
        const auto words = tokenize("p and Q OR r IMPLIES s Iff Not t aNd oR iMpLiEs iFF nOT");
        require(words.size() == 15, "all word operators recognized");
        require(words[1].type == AND && words[1].value == "and", "word operator 'and'");
        require(words[3].type == OR && words[3].value == "OR", "word operator 'OR'");
        require(words[5].type == IMPLIES && words[5].value == "->", "word operator 'IMPLIES'");
        require(words[7].type == IFF && words[7].value == "<->", "word operator 'Iff'");
        require(words[8].type == NOT && words[8].value == "~", "word operator 'Not'");
        require(words[10].type == AND, "word operator 'aNd'");
        require(words[11].type == OR, "word operator 'oR'");
        require(words[12].type == IMPLIES, "word operator 'iMpLiEs'");
        require(words[13].type == IFF, "word operator 'iFF'");
        require(words[14].type == NOT, "word operator 'nOT'");
    }

    // 4. Words embedded in variable names must not be split
    {
        const auto vars = tokenize("candy orange norm notice door band cliff");
        require(vars.size() == 7, "all words with embedded keywords must remain single variables");
        for (const auto& tok : vars) {
            require(tok.type == VARIABLE, "embedded word must be VARIABLE: " + tok.value);
        }
    }

    // 5. Line and Column tracking across whitespace and newlines
    {
        const string multiline = "A &\n  B |\n\tC";
        const auto tokens = tokenize(multiline);
        require(tokens.size() == 5, "5 tokens in multiline formula");

        // A at line 1, col 1, offset 0
        require(tokens[0].line == 1 && tokens[0].column == 1 && tokens[0].offset == 0,
                "A line/col/offset tracking");
        // & at line 1, col 3, offset 2
        require(tokens[1].line == 1 && tokens[1].column == 3 && tokens[1].offset == 2,
                "& line/col/offset tracking");
        // B at line 2, col 3 (after 2 spaces), offset 6
        require(tokens[2].line == 2 && tokens[2].column == 3 && tokens[2].offset == 6,
                "B line/col/offset tracking");
        // | at line 2, col 5, offset 8
        require(tokens[3].line == 2 && tokens[3].column == 5 && tokens[3].offset == 8,
                "| line/col/offset tracking");
        // C at line 3, col 2 (after 1 tab), offset 11
        require(tokens[4].line == 3 && tokens[4].column == 2 && tokens[4].offset == 11,
                "C line/col/offset tracking");
    }

    // 6. Lexical errors: malformed operator '-'
    {
        bool caught = false;
        try {
            (void)tokenize("A - B");
        } catch (const ParseError& err) {
            caught = true;
            require(err.category == ErrorCategory::LEXICAL, "expected lexical error for '-'");
            require(err.kind == ErrorKind::INVALID_TOKEN, "expected INVALID_TOKEN for '-'");
            require(err.offset == 2 && err.line == 1 && err.column == 3, "offset/line/col for '-'");
            require(err.found == "-", "found '-'");
            require(!err.expected.empty() && err.expected[0] == "->", "expected '->'");
        }
        require(caught, "must catch malformed '-' operator");
    }

    // 7. Lexical errors: malformed operator '<' and '<-'
    {
        bool caught1 = false;
        try {
            (void)tokenize("A < B");
        } catch (const ParseError& err) {
            caught1 = true;
            require(err.kind == ErrorKind::INVALID_TOKEN, "expected INVALID_TOKEN for '<'");
            require(err.found == "<", "found '<'");
            require(err.expected == vector<string>{"<->"}, "expected '<->'");
        }
        require(caught1, "must catch malformed '<' operator");

        bool caught2 = false;
        try {
            (void)tokenize("A <- B");
        } catch (const ParseError& err) {
            caught2 = true;
            require(err.kind == ErrorKind::INVALID_TOKEN, "expected INVALID_TOKEN for '<-'");
            require(err.found == "<-", "found '<-'");
            require(err.expected == vector<string>{"<->"}, "expected '<->'");
        }
        require(caught2, "must catch malformed '<-' operator");
    }

    // 8. Lexical errors: invalid characters and multi-byte UTF-8
    {
        for (const string invalid : {"@", "$", "#", "!", "?", "_"}) {
            bool caught = false;
            try {
                (void)tokenize("A & " + invalid);
            } catch (const ParseError& err) {
                caught = true;
                require(err.category == ErrorCategory::LEXICAL, "expected lexical category");
                require(err.kind == ErrorKind::INVALID_CHARACTER, "expected INVALID_CHARACTER");
                require(err.found == invalid, "found invalid character");
            }
            require(caught, "must catch invalid character: " + invalid);
        }

        // Test 2-byte and 3-byte UTF-8 characters (e.g. math symbols)
        bool caught_utf8 = false;
        try {
            (void)tokenize("A & \xc2\xa7"); // Section sign '§' (2 bytes)
        } catch (const ParseError& err) {
            caught_utf8 = true;
            require(err.kind == ErrorKind::INVALID_CHARACTER, "UTF-8 invalid character");
            require(err.found == "\xc2\xa7", "UTF-8 multi-byte found string");
        }
        require(caught_utf8, "must catch UTF-8 invalid character");
    }
}

// ---------------------------------------------------------------------------
// SUITE 2: AST Node Hierarchy & Visitor Pattern
// ---------------------------------------------------------------------------
void test_suite_ast_and_visitor() {
    cout << "  [Suite] AST Hierarchy & Visitor Pattern...\n";

    // 1. Node construction and convenience builders
    NodePtr varA = mkVar("A");
    NodePtr varB = mkVar("B");
    NodePtr notA = mkNot(varA);
    NodePtr andAB = mkAnd(varA, varB);
    NodePtr orAB = mkOr(varA, varB);
    NodePtr impAB = mkImplies(varA, varB);
    NodePtr iffAB = mkIff(varA, varB);

    require(dynamic_cast<VarNode*>(varA.get())->name == "A", "VarNode holds name");
    require(dynamic_cast<NotNode*>(notA.get())->left == varA, "NotNode operand in left");
    require(dynamic_cast<NotNode*>(notA.get())->right == nullptr, "NotNode right child is null");
    require(dynamic_cast<AndNode*>(andAB.get())->left == varA, "AndNode left child");
    require(dynamic_cast<AndNode*>(andAB.get())->right == varB, "AndNode right child");
    require(dynamic_cast<OrNode*>(orAB.get())->left == varA, "OrNode left child");
    require(dynamic_cast<OrNode*>(orAB.get())->right == varB, "OrNode right child");
    require(dynamic_cast<ImpliesNode*>(impAB.get())->left == varA, "ImpliesNode left child");
    require(dynamic_cast<ImpliesNode*>(impAB.get())->right == varB, "ImpliesNode right child");
    require(dynamic_cast<IffNode*>(iffAB.get())->left == varA, "IffNode left child");
    require(dynamic_cast<IffNode*>(iffAB.get())->right == varB, "IffNode right child");

    // 2. Visitor double-dispatch on all 6 node types
    NodePtr root = mkIff(
        mkImplies(mkAnd(mkVar("x"), mkNot(mkVar("y"))), mkOr(mkVar("z"), mkVar("w"))),
        mkVar("v")
    );

    NodeCountVisitor counter;
    counter.count(*root);
    require(counter.var_count == 5, "counted 5 variables");
    require(counter.not_count == 1, "counted 1 not");
    require(counter.and_count == 1, "counted 1 and");
    require(counter.or_count == 1, "counted 1 or");
    require(counter.implies_count == 1, "counted 1 implies");
    require(counter.iff_count == 1, "counted 1 iff");
}

// ---------------------------------------------------------------------------
// SUITE 3: Parser - Infix to Postfix (Shunting-Yard, Precedence & Associativity)
// ---------------------------------------------------------------------------
void test_suite_infix_to_postfix() {
    cout << "  [Suite] Parser - InfixToPostfix (Precedence & Associativity)...\n";

    // Helper to convert token list to simple string representation
    const auto to_postfix_str = [](const string& formula) {
        const auto tokens = tokenize(formula);
        const auto postfix = infixToPostfix(tokens);
        string result;
        for (size_t i = 0; i < postfix.size(); ++i) {
            if (i > 0) result += " ";
            result += postfix[i].value;
        }
        return result;
    };

    // 1. Single variable and simple operators
    require(to_postfix_str("A") == "A", "single variable");
    require(to_postfix_str("A & B") == "A B &", "simple AND");
    require(to_postfix_str("~A") == "A ~", "simple NOT");

    // 2. Precedence: NOT (5) > AND (4) > OR (3) > IMPLIES (2) > IFF (1)
    require(to_postfix_str("~A & B") == "A ~ B &", "NOT binds tighter than AND");
    require(to_postfix_str("A & ~B") == "A B ~ &", "NOT binds tighter than AND (right)");
    require(to_postfix_str("A & B | C") == "A B & C |", "AND binds tighter than OR");
    require(to_postfix_str("A | B & C") == "A B C & |", "AND binds tighter than OR (reverse order)");
    require(to_postfix_str("A | B -> C") == "A B | C ->", "OR binds tighter than IMPLIES");
    require(to_postfix_str("A -> B <-> C") == "A B -> C <->", "IMPLIES binds tighter than IFF");

    // 3. Right-associativity of NOT and IMPLIES
    require(to_postfix_str("~~A") == "A ~ ~", "NOT is right-associative");
    require(to_postfix_str("~~~A") == "A ~ ~ ~", "triple NOT is right-associative");
    require(to_postfix_str("A -> B -> C") == "A B C -> ->", "IMPLIES is right-associative (A -> (B -> C))");

    // 4. Left-associativity of AND, OR, IFF
    require(to_postfix_str("A & B & C") == "A B & C &", "AND is left-associative ((A & B) & C)");
    require(to_postfix_str("A | B | C") == "A B | C |", "OR is left-associative ((A | B) | C)");
    require(to_postfix_str("A <-> B <-> C") == "A B <-> C <->", "IFF is left-associative ((A <-> B) <-> C)");

    // 5. Parentheses overriding precedence and associativity
    require(to_postfix_str("A & (B | C)") == "A B C | &", "parentheses override precedence");
    require(to_postfix_str("(A -> B) -> C") == "A B -> C ->", "parentheses override right-associativity of implies");
    require(to_postfix_str("((A & B) | (~C -> D))") == "A B & C ~ D -> |", "nested complex parentheses");
}

// ---------------------------------------------------------------------------
// SUITE 4: Parser - AST Construction & parseFormula
// ---------------------------------------------------------------------------
void test_suite_ast_construction() {
    cout << "  [Suite] Parser - AST Construction (buildAST & parseFormula)...\n";

    // 1. Standalone buildAST
    const auto tokens = tokenize("A & B");
    const auto postfix = infixToPostfix(tokens);
    NodePtr ast = buildAST(postfix);
    require(dynamic_cast<AndNode*>(ast.get()) != nullptr, "buildAST constructs AndNode root");

    // 2. TC-2 from Slide 1: (A & ~B) -> C produces ImpliesNode with AndNode and VarNode
    NodePtr root = parseFormula("(A & ~B) -> C");
    auto implies = dynamic_cast<ImpliesNode*>(root.get());
    require(implies != nullptr, "root must be ImpliesNode");
    auto left_and = dynamic_cast<AndNode*>(implies->left.get());
    require(left_and != nullptr, "implies left child must be AndNode");
    auto right_var = dynamic_cast<VarNode*>(implies->right.get());
    require(right_var != nullptr && right_var->name == "C", "implies right child must be VarNode C");
    auto and_left_var = dynamic_cast<VarNode*>(left_and->left.get());
    require(and_left_var != nullptr && and_left_var->name == "A", "and left child must be VarNode A");
    auto and_right_not = dynamic_cast<NotNode*>(left_and->right.get());
    require(and_right_not != nullptr, "and right child must be NotNode");
    auto not_var = dynamic_cast<VarNode*>(and_right_not->left.get());
    require(not_var != nullptr && not_var->name == "B", "not child must be VarNode B");

    // 3. Word-form operators creating proper nodes
    NodePtr word_ast = parseFormula("A implies B iff C");
    require(dynamic_cast<IffNode*>(word_ast.get()) != nullptr, "word operators parsed into correct AST");
}

// ---------------------------------------------------------------------------
// SUITE 5: Structured Parse Errors (Syntax and Edge Cases)
// ---------------------------------------------------------------------------
void test_suite_parse_errors() {
    cout << "  [Suite] Structured Parse Errors & Diagnostics...\n";

    const auto capture = [](const string& formula) {
        try {
            (void)parseFormula(formula);
        } catch (const ParseError& err) {
            return err;
        }
        throw runtime_error("Expected ParseError was not thrown for: " + formula);
    };

    // 1. Empty formulas
    {
        const ParseError err = capture("");
        require(err.category == ErrorCategory::SYNTAX, "empty formula syntax category");
        require(err.kind == ErrorKind::UNEXPECTED_END_OF_INPUT, "empty formula UNEXPECTED_END_OF_INPUT");
        require(err.offset == 0, "empty formula offset 0");
        require(err.expected == vector<string>({"IDENTIFIER", "~", "("}), "expected operand starters");
    }
    {
        const ParseError err = capture("   \t  \n  ");
        require(err.kind == ErrorKind::UNEXPECTED_END_OF_INPUT, "whitespace only UNEXPECTED_END_OF_INPUT");
    }

    // 2. Empty parentheses "()"
    {
        const ParseError err = capture("()");
        require(err.kind == ErrorKind::MISSING_OPERAND, "empty parentheses is MISSING_OPERAND");
        require(err.offset == 1, "offset at ')'");
        require(err.found == ")", "found ')'");
    }

    // 3. Missing operands
    {
        const ParseError err1 = capture("A &");
        require(err1.kind == ErrorKind::UNEXPECTED_END_OF_INPUT, "trailing operator is UNEXPECTED_END_OF_INPUT");

        const ParseError err2 = capture("& A");
        require(err2.kind == ErrorKind::MISSING_OPERAND, "leading binary operator is MISSING_OPERAND");
        require(err2.found == "&", "found '&'");

        const ParseError err3 = capture("A & & B");
        require(err3.kind == ErrorKind::MISSING_OPERAND, "double operator is MISSING_OPERAND");
        require(err3.offset == 4 && err3.found == "&", "found second '&'");

        const ParseError err4 = capture("(A & )");
        require(err4.kind == ErrorKind::MISSING_OPERAND, "missing operand before close paren");
        require(err4.found == ")", "found ')'");
    }

    // 4. Missing operators (adjacent tokens)
    {
        const ParseError err1 = capture("A B");
        require(err1.kind == ErrorKind::MISSING_OPERATOR, "adjacent variables is MISSING_OPERATOR");
        require(err1.offset == 2 && err1.found == "B", "found 'B'");

        const ParseError err2 = capture("(A) B");
        require(err2.kind == ErrorKind::MISSING_OPERATOR, "variable after closing paren");
        require(err2.found == "B", "found 'B'");

        const ParseError err3 = capture("A (B)");
        require(err3.kind == ErrorKind::MISSING_OPERATOR, "opening paren after variable");
        require(err3.found == "(", "found '('");

        const ParseError err4 = capture("A ~ B");
        require(err4.kind == ErrorKind::MISSING_OPERATOR, "negation after operand");
        require(err4.found == "~", "found '~'");

        const ParseError err5 = capture("A ~");
        require(err5.kind == ErrorKind::MISSING_OPERATOR, "trailing negation after operand");
        require(err5.found == "~", "found '~'");
    }

    // 5. Parentheses mismatches
    {
        const ParseError err1 = capture("(A | B");
        require(err1.kind == ErrorKind::MISSING_CLOSING_PAREN, "unclosed opening paren");
        require(err1.found.empty(), "found is empty at end of input");
        require(err1.expected == vector<string>{")"}, "expected ')'");

        const ParseError err2 = capture("((A & B)");
        require(err2.kind == ErrorKind::MISSING_CLOSING_PAREN, "nested unclosed opening paren");

        const ParseError err3 = capture("A )");
        require(err3.kind == ErrorKind::UNEXPECTED_CLOSING_PAREN, "unmatched closing paren");
        require(err3.offset == 2, "offset of closing paren");

        const ParseError err4 = capture(")");
        require(err4.kind == ErrorKind::MISSING_OPERAND, "closing paren at start expects operand");
    }

    // 6. Exception what() output
    {
        const ParseError err = capture("A & @");
        const string what_msg = err.what();
        require(what_msg.find("invalid character") != string::npos, "what() contains error description");
    }
}

// ---------------------------------------------------------------------------
// SUITE 6: NNF Converter Unit Tests
// ---------------------------------------------------------------------------
void test_suite_nnf_converter() {
    cout << "  [Suite] NNF Converter (Rules, Double Negation, De Morgan)...\n";

    NNFConverter converter;
    TruthTableEngine engine;
    NNFValidator validator;

    // 1. Literal and simple negation
    {
        NodePtr var = parseFormula("A");
        NodePtr nnf = converter.convert(*var);
        validator.validate(*nnf);
        require(dynamic_cast<VarNode*>(nnf.get()) != nullptr, "literal remains VarNode");

        NodePtr not_var = parseFormula("~A");
        NodePtr nnf_not = converter.convert(*not_var);
        validator.validate(*nnf_not);
        require(dynamic_cast<NotNode*>(nnf_not.get()) != nullptr, "negated literal remains NotNode");
    }

    // 2. Double negation elimination
    {
        NodePtr double_not = parseFormula("~~A");
        NodePtr nnf = converter.convert(*double_not);
        validator.validate(*nnf);
        require(dynamic_cast<VarNode*>(nnf.get()) != nullptr, "~~A simplifies to VarNode A");

        NodePtr triple_not = parseFormula("~~~A");
        NodePtr nnf_triple = converter.convert(*triple_not);
        validator.validate(*nnf_triple);
        require(dynamic_cast<NotNode*>(nnf_triple.get()) != nullptr, "~~~A simplifies to ~A");

        NodePtr quad_not = parseFormula("~~~~A");
        NodePtr nnf_quad = converter.convert(*quad_not);
        validator.validate(*nnf_quad);
        require(dynamic_cast<VarNode*>(nnf_quad.get()) != nullptr, "~~~~A simplifies to A");
    }

    // 3. De Morgan's laws: ~(A & B) => ~A | ~B, ~(A | B) => ~A & ~B
    {
        NodePtr demorgan_and = parseFormula("~(A & B)");
        NodePtr nnf_and = converter.convert(*demorgan_and);
        validator.validate(*nnf_and);
        require(dynamic_cast<OrNode*>(nnf_and.get()) != nullptr, "~(A & B) root is OrNode");
        require(engine.are_equivalent(*demorgan_and, *nnf_and), "De Morgan AND preserves semantics");

        NodePtr demorgan_or = parseFormula("~(A | B)");
        NodePtr nnf_or = converter.convert(*demorgan_or);
        validator.validate(*nnf_or);
        require(dynamic_cast<AndNode*>(nnf_or.get()) != nullptr, "~(A | B) root is AndNode");
        require(engine.are_equivalent(*demorgan_or, *nnf_or), "De Morgan OR preserves semantics");
    }

    // 4. Implication elimination: A -> B => ~A | B, ~(A -> B) => A & ~B
    {
        NodePtr implies = parseFormula("A -> B");
        NodePtr nnf = converter.convert(*implies);
        validator.validate(*nnf);
        require(dynamic_cast<OrNode*>(nnf.get()) != nullptr, "A -> B root is OrNode (~A | B)");
        require(engine.are_equivalent(*implies, *nnf), "Implication NNF is semantically equivalent");

        NodePtr not_implies = parseFormula("~(A -> B)");
        NodePtr nnf_not = converter.convert(*not_implies);
        validator.validate(*nnf_not);
        require(dynamic_cast<AndNode*>(nnf_not.get()) != nullptr, "~(A -> B) root is AndNode (A & ~B)");
        require(engine.are_equivalent(*not_implies, *nnf_not), "Negated implication preserves semantics");
    }

    // 5. Biconditional elimination: A <-> B and ~(A <-> B)
    {
        NodePtr iff = parseFormula("A <-> B");
        NodePtr nnf = converter.convert(*iff);
        validator.validate(*nnf);
        require(dynamic_cast<OrNode*>(nnf.get()) != nullptr, "A <-> B root is OrNode");
        require(engine.are_equivalent(*iff, *nnf), "Biconditional NNF preserves semantics");

        NodePtr not_iff = parseFormula("~(A <-> B)");
        NodePtr nnf_not = converter.convert(*not_iff);
        validator.validate(*nnf_not);
        require(dynamic_cast<OrNode*>(nnf_not.get()) != nullptr, "~(A <-> B) root is OrNode");
        require(engine.are_equivalent(*not_iff, *nnf_not), "Negated biconditional preserves semantics");
    }

    // 6. Complex nested expressions
    {
        const vector<string> test_cases = {
            "(A & B) | (C & D)",
            "~((A & B) | (C & D))",
            "(A -> B) & (B -> C) & (C -> A)",
            "~((A <-> B) -> (C & ~D))",
            "~~(A | ~B) & ~(C -> (D <-> E))"
        };
        for (const auto& formula_str : test_cases) {
            NodePtr orig = parseFormula(formula_str);
            NodePtr nnf = converter.convert(*orig);
            validator.validate(*nnf);
            require(engine.are_equivalent(*orig, *nnf), "Complex NNF equivalence: " + formula_str);
        }
    }

    // 7. Malformed AST exception check
    {
        bool caught = false;
        try {
            // AndNode with null children
            NodePtr malformed = mkAnd(nullptr, nullptr);
            (void)converter.convert(*malformed);
        } catch (const invalid_argument&) {
            caught = true;
        }
        require(caught, "NNF converter must throw invalid_argument on null AST child");
    }
}

// ---------------------------------------------------------------------------
// SUITE 7: Truth Table Engine - BitVector Unit Tests
// ---------------------------------------------------------------------------
void test_suite_bitvector() {
    cout << "  [Suite] Truth Table Engine - BitVector Low-Level Operations...\n";

    // 1. Default constructor
    BitVector bv_default;
    require(bv_default.size() == 0, "default BitVector size is 0");

    // 2. Sized constructor, get and set
    BitVector bv(130);
    require(bv.size() == 130, "BitVector size is 130");
    require(bv.count_ones() == 0, "new BitVector initial count_ones is 0");

    // Set bit 0, bit 63, bit 64 (word 1), bit 129 (word 2)
    bv.set(0, true);
    bv.set(63, true);
    bv.set(64, true);
    bv.set(129, true);

    require(bv.get(0) == true, "bit 0 is true");
    require(bv.get(63) == true, "bit 63 is true");
    require(bv.get(64) == true, "bit 64 is true");
    require(bv.get(129) == true, "bit 129 is true");
    require(bv.get(1) == false, "bit 1 is false");
    require(bv.get(65) == false, "bit 65 is false");
    require(bv.count_ones() == 4, "count_ones is 4");

    // Clear bit 64
    bv.set(64, false);
    require(bv.get(64) == false, "bit 64 cleared to false");
    require(bv.count_ones() == 3, "count_ones after clear is 3");

    // 3. set_word
    BitVector bv_word(64);
    bv_word.set_word(0, 0x0F0F0F0F0F0F0F0FULL);
    require(bv_word.count_ones() == 32, "count_ones after set_word is 32");
    require(bv_word.get(0) == true && bv_word.get(1) == true && bv_word.get(4) == false,
            "bit values after set_word");

    // 4. Equality and Inequality
    BitVector a(64), b(64), c(64), d(32);
    a.set(10, true);
    b.set(10, true);
    c.set(11, true);
    d.set(10, true);

    require(a == b, "identical BitVectors are equal");
    require(a != c, "different bits are not equal");
    require(a != d, "different sizes are not equal");
}

// ---------------------------------------------------------------------------
// SUITE 8: Truth Table Engine - Variables & Single Assignment Evaluation
// ---------------------------------------------------------------------------
void test_suite_variables_and_evaluation() {
    cout << "  [Suite] Truth Table Engine - Variable Collection & Single Evaluation...\n";

    TruthTableEngine engine;

    // 1. collect_variables order of first appearance
    {
        NodePtr ast = parseFormula("Z | (Y & X) -> (Y <-> Z)");
        vector<string> vars = engine.collect_variables(*ast);
        require(vars.size() == 3, "collected 3 unique variables");
        require(vars[0] == "Z", "first variable Z");
        require(vars[1] == "Y", "second variable Y");
        require(vars[2] == "X", "third variable X");
    }

    // 2. evaluate under specific assignments
    {
        NodePtr ast = parseFormula("A & ~B | C");

        Assignment a1 = {{"A", true}, {"B", false}, {"C", false}};
        require(engine.evaluate(*ast, a1) == true, "A=T, B=F, C=F => T");

        Assignment a2 = {{"A", false}, {"B", false}, {"C", false}};
        require(engine.evaluate(*ast, a2) == false, "A=F, B=F, C=F => F");

        Assignment a3 = {{"A", false}, {"B", true}, {"C", true}};
        require(engine.evaluate(*ast, a3) == true, "A=F, B=T, C=T => T");
    }

    // 3. evaluate throws runtime_error for unbound variable
    {
        NodePtr ast = parseFormula("A & B");
        Assignment incomplete = {{"A", true}};
        bool caught = false;
        try {
            (void)engine.evaluate(*ast, incomplete);
        } catch (const runtime_error& err) {
            caught = true;
            require(string(err.what()).find("unbound variable: B") != string::npos,
                    "unbound variable error message");
        }
        require(caught, "must throw runtime_error on unbound variable");
    }

    // 4. collect_variables throws on malformed AST
    {
        bool caught = false;
        try {
            NodePtr malformed = mkOr(mkVar("A"), nullptr);
            (void)engine.collect_variables(*malformed);
        } catch (const invalid_argument&) {
            caught = true;
        }
        require(caught, "collect_variables must throw on null child");
    }
}

// ---------------------------------------------------------------------------
// SUITE 9: Truth Table Engine - Generation, Multi-Block (>6 vars), Classification
// ---------------------------------------------------------------------------
void test_suite_truth_table_generation() {
    cout << "  [Suite] Truth Table Engine - Table Generation & Multi-Block (<=24 vars)...\n";

    TruthTableEngine engine;

    // 1. Row count and assignment mapping (binary counting, MSB first)
    {
        NodePtr ast = parseFormula("A & B");
        TruthTable table = engine.truth_table(*ast);
        require(table.variables == vector<string>({"A", "B"}), "variables order A, B");
        require(table.num_rows() == 4, "2 variables => 4 rows");

        // Row 0: A=F, B=F -> F
        // Row 1: A=F, B=T -> F
        // Row 2: A=T, B=F -> F
        // Row 3: A=T, B=T -> T
        require(table.value(0) == false, "row 0 result is F");
        require(table.value(1) == false, "row 1 result is F");
        require(table.value(2) == false, "row 2 result is F");
        require(table.value(3) == true, "row 3 result is T");

        Assignment r0 = table.assignment(0);
        require(r0["A"] == false && r0["B"] == false, "row 0 assignment A=0, B=0");
        Assignment r3 = table.assignment(3);
        require(r3["A"] == true && r3["B"] == true, "row 3 assignment A=1, B=1");
    }

    // 2. Multi-word block evaluation: test 6 variables (64 rows) and 7 variables (128 rows)
    {
        // 6 variables: 2^6 = 64 rows (fills exactly 1 64-bit word)
        NodePtr ast6 = parseFormula("v1 & v2 & v3 & v4 & v5 & v6");
        TruthTable t6 = engine.truth_table(*ast6);
        require(t6.num_rows() == 64, "6 variables => 64 rows");
        require(t6.results.count_ones() == 1, "conjunction of 6 vars has exactly 1 true row");
        require(t6.value(63) == true, "row 63 (all 1s) is true");
        require(t6.value(62) == false, "row 62 is false");

        // 7 variables: 2^7 = 128 rows (spans across 2 64-bit words, exercises s >= 6 bit patterns)
        NodePtr ast7 = parseFormula("v1 & v2 & v3 & v4 & v5 & v6 & v7");
        TruthTable t7 = engine.truth_table(*ast7);
        require(t7.num_rows() == 128, "7 variables => 128 rows");
        require(t7.results.count_ones() == 1, "conjunction of 7 vars has exactly 1 true row");
        require(t7.value(127) == true, "row 127 (all 1s) is true");
        require(t7.value(0) == false, "row 0 is false");
    }

    // 3. Formula Classification: Tautology, Contradiction, Contingency
    {
        NodePtr tautology = parseFormula("A | ~A");
        NodePtr contradiction = parseFormula("A & ~A");
        NodePtr contingency = parseFormula("A -> B");

        TruthTable t_taut = engine.truth_table(*tautology);
        TruthTable t_contr = engine.truth_table(*contradiction);
        TruthTable t_cont = engine.truth_table(*contingency);

        require(engine.classify(t_taut) == Classification::TAUTOLOGY, "classify TAUTOLOGY");
        require(engine.classify(t_contr) == Classification::CONTRADICTION, "classify CONTRADICTION");
        require(engine.classify(t_cont) == Classification::CONTINGENCY, "classify CONTINGENCY");

        require(to_string(Classification::TAUTOLOGY) == "Tautology", "to_string Tautology");
        require(to_string(Classification::CONTRADICTION) == "Contradiction", "to_string Contradiction");
        require(to_string(Classification::CONTINGENCY) == "Contingency", "to_string Contingency");
    }

    // 4. Logical Equivalence with disjoint or superset variable sets
    {
        NodePtr f1 = parseFormula("A & (B | ~B)");
        NodePtr f2 = parseFormula("A");
        require(engine.are_equivalent(*f1, *f2), "A & (B | ~B) is logically equivalent to A");

        NodePtr f3 = parseFormula("A | B");
        require(!engine.are_equivalent(*f1, *f3), "A & (B | ~B) is not equivalent to A | B");
    }

    // 5. Max variable guard: throws length_error when variables exceed limit
    {
        TruthTableEngine small_engine(3); // limit to 3 vars
        NodePtr four_vars = parseFormula("A & B & C & D");
        bool caught = false;
        try {
            (void)small_engine.truth_table(*four_vars);
        } catch (const length_error& err) {
            caught = true;
            require(string(err.what()).find("too many variables") != string::npos,
                    "length_error message contains 'too many variables'");
        }
        require(caught, "must throw length_error when variable count > max_vars");
    }
}

// ---------------------------------------------------------------------------
// SUITE 10: Truth Table Printing (print_truth_table)
// ---------------------------------------------------------------------------
void test_suite_print_truth_table() {
    cout << "  [Suite] Truth Table Engine - Formatting & Display...\n";

    TruthTableEngine engine;
    NodePtr ast = parseFormula("A & B");
    TruthTable table = engine.truth_table(*ast);

    ostringstream oss;
    print_truth_table(oss, table);
    const string output = oss.str();

    // Check header and separator
    require(output.find("A B | F") != string::npos, "table output contains variable headers");
    require(output.find("+--") != string::npos, "table output contains divider");
    // Check row entries
    require(output.find("F F | F") != string::npos, "row 0 formatted");
    require(output.find("T T | T") != string::npos, "row 3 formatted");
}

// ---------------------------------------------------------------------------
// SUITE 11: Regression & Presentation Slide Test Cases (TC-1, TC-2, TC-3)
// ---------------------------------------------------------------------------
void test_suite_presentation_regressions() {
    cout << "  [Suite] Presentation Regression Verification (TC-1, TC-2, TC-3 & Defect Fixes)...\n";

    // TC-1: tokenize("(red_2 & ~blue) -> green") -> returns exactly 8 tokens
    {
        const auto tokens = tokenize("(red_2 & ~blue) -> green");
        require(tokens.size() == 8, "TC-1: exactly 8 valid tokens");
        require(tokens[0].type == LPAREN, "TC-1: token 0 LPAREN");
        require(tokens[1].type == VARIABLE && tokens[1].value == "red_2", "TC-1: token 1 red_2");
        require(tokens[2].type == AND, "TC-1: token 2 AND");
        require(tokens[3].type == NOT, "TC-1: token 3 NOT");
        require(tokens[4].type == VARIABLE && tokens[4].value == "blue", "TC-1: token 4 blue");
        require(tokens[5].type == RPAREN, "TC-1: token 5 RPAREN");
        require(tokens[6].type == IMPLIES, "TC-1: token 6 IMPLIES");
        require(tokens[7].type == VARIABLE && tokens[7].value == "green", "TC-1: token 7 green");
    }

    // Nasty Defect 1: 'iff' keyword producing '<->' token
    {
        const auto tokens = tokenize("A iff B");
        require(tokens.size() == 3, "Defect 1: exactly 3 tokens");
        require(tokens[1].type == IFF && tokens[1].value == "<->", "Defect 1: 'iff' maps to '<->'");
        NodePtr ast = parseFormula("A iff B");
        require(dynamic_cast<IffNode*>(ast.get()) != nullptr, "Defect 1: parses into IffNode");
    }

    // Nasty Defect 2: variables starting with digits (e.g. 2nd_var, 10)
    {
        const auto tokens = tokenize("2nd_var & 10");
        require(tokens.size() == 3, "Defect 2: numeric variables tokenize cleanly");
        require(tokens[0].type == VARIABLE && tokens[0].value == "2nd_var", "Defect 2: 2nd_var");
        require(tokens[2].type == VARIABLE && tokens[2].value == "10", "Defect 2: 10");
        require(parseFormula("2nd_var & 10") != nullptr, "Defect 2: parses into AST");
    }

    // TC-3: Rejection of invalid formulas ("", "()", "A &", "(A | B", "A B", "A~")
    {
        const vector<string> invalid_inputs = {"", "()", "A &", "(A | B", "A B", "A~", "(A)B"};
        for (const string& invalid : invalid_inputs) {
            bool rejected = false;
            try {
                (void)parseFormula(invalid);
            } catch (const ParseError&) {
                rejected = true;
            }
            require(rejected, "TC-3: invalid formula rejected: " + invalid);
        }
    }
}

} // namespace

int main() {
    cout << "========================================================\n";
    cout << "           WORKLET 1 - UNIT TEST SUITE                  \n";
    cout << "========================================================\n";

    try {
        test_suite_lexer();
        ++g_total_suites;

        test_suite_ast_and_visitor();
        ++g_total_suites;

        test_suite_infix_to_postfix();
        ++g_total_suites;

        test_suite_ast_construction();
        ++g_total_suites;

        test_suite_parse_errors();
        ++g_total_suites;

        test_suite_nnf_converter();
        ++g_total_suites;

        test_suite_bitvector();
        ++g_total_suites;

        test_suite_variables_and_evaluation();
        ++g_total_suites;

        test_suite_truth_table_generation();
        ++g_total_suites;

        test_suite_print_truth_table();
        ++g_total_suites;

        test_suite_presentation_regressions();
        ++g_total_suites;

        cout << "========================================================\n";
        cout << "  SUCCESS: All " << g_total_suites << " test suites passed!\n";
        cout << "  Total assertions verified: " << g_total_assertions << "\n";
        cout << "========================================================\n";
        return 0;
    } catch (const exception& ex) {
        cerr << "\n*** UNIT TEST FAILURE ***\n" << ex.what() << "\n";
        return 1;
    }
}
