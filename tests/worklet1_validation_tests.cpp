// =============================================================================
//  worklet1_validation_tests.cpp
//  VALIDATION test-suite for Worklet 1 (propositional-logic front end)
//
//  Covers : Lexer -> Parser (Shunting-Yard + AST) -> Truth table -> NNF
//  Style  : every check has an auto-generated ID (LEX-01, PAR-07, ...), a plain
//           English description, and prints PASS/FAIL.  The suite does NOT stop
//           at the first failure, so you always see the whole picture.
//  Oracle : an independent evaluator (class Oracle below) is used to cross-check
//           the project's own bit-parallel truth table and equivalence engine.
//
//  Run    : see docs/worklet1_validation_guide.md   (or ./worklet1_validation_tests --quiet)
// =============================================================================
#include "lexer.h"
#include "nnf.h"
#include "parse_error.h"
#include "parser.h"
#include "truth_table.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

namespace {

// ----------------------------------------------------------------------------
// Tiny test framework
// ----------------------------------------------------------------------------
int g_passed = 0;
int g_failed = 0;
bool g_quiet = false;
vector<string> g_failures;
string g_prefix;
int g_counter = 0;

void section(const string& prefix, const string& title) {
    g_prefix = prefix;
    g_counter = 0;
    if (!g_quiet) cout << "\n=== " << title << " ===\n";
}

void check(const string& what, bool ok, const string& detail = "") {
    ++g_counter;
    char num[8];
    snprintf(num, sizeof num, "%02d", g_counter);
    const string id = g_prefix + "-" + num;
    if (ok) {
        ++g_passed;
        if (!g_quiet) cout << "  [PASS] " << id << "  " << what << "\n";
    } else {
        ++g_failed;
        g_failures.push_back(id + "  " + what);
        cout << "  [FAIL] " << id << "  " << what << "\n";
        if (!detail.empty()) cout << "         -> " << detail << "\n";
    }
}

void check_eq(const string& what, const string& actual, const string& expected) {
    check(what, actual == expected, "expected [" + expected + "] but got [" + actual + "]");
}

void info(const string& text) {
    if (!g_quiet) cout << "  [INFO] " << text << "\n";
}

// ----------------------------------------------------------------------------
// Helpers: AST printer, independent evaluator, NNF-shape checker
// ----------------------------------------------------------------------------

// Prints an AST fully parenthesised, e.g.  A | B & C  ->  (A | (B & C))
class Printer : public Visitor {
public:
    string out;
    void visit(const VarNode& n) override { out += n.name; }
    void visit(const NotNode& n) override { out += "~"; n.left->accept(*this); }
    void visit(const AndNode& n) override { bin(n, " & "); }
    void visit(const OrNode& n) override { bin(n, " | "); }
    void visit(const ImpliesNode& n) override { bin(n, " -> "); }
    void visit(const IffNode& n) override { bin(n, " <-> "); }
private:
    void bin(const ASTNode& n, const char* op) {
        out += "(";
        n.left->accept(*this);
        out += op;
        n.right->accept(*this);
        out += ")";
    }
};

string show(const NodePtr& n) {
    Printer p;
    n->accept(p);
    return p.out;
}

string showFormula(const string& formula) {
    try {
        return show(parseFormula(formula));
    } catch (const exception& e) {
        return string("<exception: ") + e.what() + ">";
    }
}

size_t nodeCount(const NodePtr& n) {
    return n ? 1 + nodeCount(n->left) + nodeCount(n->right) : 0;
}

// Independent evaluator -- written separately from the project's TruthTableEngine
// so the two can be compared against each other.
class Oracle : public Visitor {
public:
    explicit Oracle(const Assignment& a) : a_(a) {}
    bool run(const ASTNode& n) { n.accept(*this); return v_; }
    void visit(const VarNode& n) override { v_ = a_.at(n.name); }
    void visit(const NotNode& n) override { v_ = !run(*n.left); }
    void visit(const AndNode& n) override { const bool l = run(*n.left); const bool r = run(*n.right); v_ = l && r; }
    void visit(const OrNode& n) override { const bool l = run(*n.left); const bool r = run(*n.right); v_ = l || r; }
    void visit(const ImpliesNode& n) override { const bool l = run(*n.left); const bool r = run(*n.right); v_ = !l || r; }
    void visit(const IffNode& n) override { const bool l = run(*n.left); const bool r = run(*n.right); v_ = (l == r); }
private:
    const Assignment& a_;
    bool v_ = false;
};

// first variable = most significant bit (same convention as truth_table.h)
Assignment assignmentFor(const vector<string>& vars, uint64_t row) {
    Assignment a;
    const size_t n = vars.size();
    for (size_t i = 0; i < n; ++i) a[vars[i]] = (row >> (n - 1 - i)) & 1ULL;
    return a;
}

vector<string> unionVars(const ASTNode& x, const ASTNode& y) {
    TruthTableEngine e;
    set<string> s;
    for (auto& v : e.collect_variables(x)) s.insert(v);
    for (auto& v : e.collect_variables(y)) s.insert(v);
    return vector<string>(s.begin(), s.end());
}

bool oracleEquivalent(const ASTNode& x, const ASTNode& y) {
    const auto vars = unionVars(x, y);
    for (uint64_t row = 0; row < (1ULL << vars.size()); ++row) {
        const Assignment a = assignmentFor(vars, row);
        Oracle o1(a), o2(a);
        if (o1.run(x) != o2.run(y)) return false;
    }
    return true;
}

// Returns "" when the tree is a legal NNF, otherwise a description of the problem.
class NNFChecker : public Visitor {
public:
    string problem;
    void visit(const VarNode&) override {}
    void visit(const NotNode& n) override {
        if (!n.left || n.right || !dynamic_cast<const VarNode*>(n.left.get()))
            problem = "NOT applied to something other than a variable";
    }
    void visit(const AndNode& n) override { kids(n); }
    void visit(const OrNode& n) override { kids(n); }
    void visit(const ImpliesNode&) override { problem = "implication left in NNF"; }
    void visit(const IffNode&) override { problem = "biconditional left in NNF"; }
private:
    void kids(const ASTNode& n) {
        if (!n.left || !n.right) { problem = "binary node missing a child"; return; }
        n.left->accept(*this);
        if (problem.empty()) n.right->accept(*this);
    }
};

string nnfProblem(const ASTNode& n) {
    NNFChecker c;
    n.accept(c);
    return c.problem;
}

string tableBits(const TruthTable& t) {
    string s;
    for (uint64_t r = 0; r < t.num_rows(); ++r) s += t.value(r) ? 'T' : 'F';
    return s;
}

vector<TokenType> typesOf(const string& f) {
    vector<TokenType> t;
    for (const Token& k : tokenize(f)) t.push_back(k.type);
    return t;
}

struct Caught {
    bool threw = false;
    ParseError err{ErrorCategory::LEXICAL, ErrorKind::INVALID_CHARACTER, "", 0, 0, 0};
};

Caught parseExpectingError(const string& formula) {
    Caught c;
    try {
        (void)parseFormula(formula);
    } catch (const ParseError& e) {
        c.threw = true;
        c.err = e;
    } catch (...) {
    }
    return c;
}

// Random formula generator (fixed seed => reproducible)
NodePtr randomFormula(mt19937& rng, int depth, int numVars) {
    if (depth == 0 || rng() % 5 == 0)
        return mkVar(string(1, static_cast<char>('A' + rng() % numVars)));
    switch (rng() % 5) {
        case 0: return mkNot(randomFormula(rng, depth - 1, numVars));
        case 1: return mkAnd(randomFormula(rng, depth - 1, numVars), randomFormula(rng, depth - 1, numVars));
        case 2: return mkOr(randomFormula(rng, depth - 1, numVars), randomFormula(rng, depth - 1, numVars));
        case 3: return mkImplies(randomFormula(rng, depth - 1, numVars), randomFormula(rng, depth - 1, numVars));
        default: return mkIff(randomFormula(rng, depth - 1, numVars), randomFormula(rng, depth - 1, numVars));
    }
}

// ============================================================================
//  1. LEXER
// ============================================================================
void validate_lexer() {
    section("LEX", "1. LEXER  (text -> tokens)");

    check("every operator symbol is recognised: & | ~ ( ) -> <->",
          typesOf("& | ~ ( ) -> <->") ==
              vector<TokenType>{AND, OR, NOT, LPAREN, RPAREN, IMPLIES, IFF});

    check("spaces are optional: A&B|~C",
          typesOf("A&B|~C") == vector<TokenType>{VARIABLE, AND, VARIABLE, OR, NOT, VARIABLE});

    check("spaces are optional for arrows: A->B<->C",
          typesOf("A->B<->C") == vector<TokenType>{VARIABLE, IMPLIES, VARIABLE, IFF, VARIABLE});

    check("empty string gives zero tokens", tokenize("").empty());
    check("whitespace-only (spaces, tab, newline) gives zero tokens", tokenize("  \t\n ").empty());

    {
        const auto t = tokenize("x1 7up a_b_c Z9");
        check("variable names may contain digits/underscores and may start with a digit",
              t.size() == 4 && t[0].value == "x1" && t[1].value == "7up" &&
                  t[2].value == "a_b_c" && t[3].value == "Z9");
    }

    check("word operators are case-insensitive: and/OR/Not/iMpLiEs/IFF",
          typesOf("A and B OR C Not D iMpLiEs E IFF F") ==
              vector<TokenType>{VARIABLE, AND, VARIABLE, OR, VARIABLE, NOT, VARIABLE,
                                IMPLIES, VARIABLE, IFF, VARIABLE});

    {
        bool ok = true;
        string bad;
        for (const string w : {"android", "order", "notice", "iffy", "impliesX", "band", "or_", "and1"}) {
            const auto t = tokenize(w);
            if (!(t.size() == 1 && t[0].type == VARIABLE)) { ok = false; bad = w; }
        }
        check("words that merely CONTAIN an operator word stay variables (android, order, notice, iffy...)",
              ok, "wrongly split: " + bad);
    }

    {
        const auto t = tokenize("A & B");
        check("token offsets/columns are correct for 'A & B'  (0,2,4 / 1,3,5)",
              t.size() == 3 && t[0].offset == 0 && t[1].offset == 2 && t[2].offset == 4 &&
                  t[0].column == 1 && t[1].column == 3 && t[2].column == 5 &&
                  t[0].line == 1 && t[2].line == 1);
    }

    {
        const auto t = tokenize("A\n&\nB");
        check("line numbers advance on '\\n'  (& is on line 2, B on line 3, both column 1)",
              t.size() == 3 && t[1].line == 2 && t[1].column == 1 && t[2].line == 3 && t[2].column == 1);
    }

    {
        const auto t = tokenize("A\r\n& B");
        check("Windows line endings (\\r\\n) count as ONE line break",
              t.size() == 3 && t[1].line == 2 && t[1].column == 1);
    }

    {
        const auto t = tokenize("A <-> B iff C");
        check("lexeme keeps the original text; value is normalised ('iff' -> '<->')",
              t.size() == 5 && t[1].lexeme == "<->" && t[3].lexeme == "iff" && t[3].value == "<->");
    }

    {
        struct Row { const char* in; ErrorKind kind; size_t offset; const char* found; };
        const Row rows[] = {
            {"_A",     ErrorKind::INVALID_CHARACTER, 0, "_"},
            {"A $ B",  ErrorKind::INVALID_CHARACTER, 2, "$"},
            {"A => B", ErrorKind::INVALID_CHARACTER, 2, "="},
            {"A & \xC3\xA9", ErrorKind::INVALID_CHARACTER, 4, "\xC3\xA9"},  // 'é' (2 bytes)
            {"A - B",  ErrorKind::INVALID_TOKEN,     2, "-"},
            {"A -",    ErrorKind::INVALID_TOKEN,     2, "-"},
            {"A <- B", ErrorKind::INVALID_TOKEN,     2, "<-"},
            {"A <> B", ErrorKind::INVALID_TOKEN,     2, "<"},
            {"A < - > B", ErrorKind::INVALID_TOKEN,  2, "<"},
        };
        for (const Row& r : rows) {
            const Caught c = parseExpectingError(r.in);
            check(string("lexical error for [") + r.in + "]: right kind, offset and offending text",
                  c.threw && c.err.category == ErrorCategory::LEXICAL && c.err.kind == r.kind &&
                      c.err.offset == r.offset && c.err.found == r.found,
                  c.threw ? "kind/offset/found differ: offset=" + to_string(c.err.offset) +
                                " found=[" + c.err.found + "]"
                          : "no ParseError thrown");
        }
    }

    {
        const Caught c = parseExpectingError("A\n& @");
        check("lexical error position is reported as line 2, column 3 for 'A\\n& @'",
              c.threw && c.err.line == 2 && c.err.column == 3);
    }

    {
        bool threw = false;
        size_t n = 0;
        try { n = tokenize("& & )").size(); } catch (...) { threw = true; }
        check("lexer only checks SPELLING, not grammar: '& & )' still tokenises to 3 tokens",
              !threw && n == 3);
    }
}

// ============================================================================
//  2. PARSER (precedence, associativity, AST shape, postfix)
// ============================================================================
void validate_parser() {
    section("PAR", "2. PARSER  (tokens -> postfix -> AST)");

    struct Row { const char* in; const char* expected; };
    const Row shapes[] = {
        // precedence  (~  >  &  >  |  >  ->  >  <->)
        {"A | B & C",        "(A | (B & C))"},
        {"A & B | C",        "((A & B) | C)"},
        {"~A & B",           "(~A & B)"},
        {"~A | B",           "(~A | B)"},
        {"~(A & B)",         "~(A & B)"},
        {"A -> B | C",       "(A -> (B | C))"},
        {"A | B -> C",       "((A | B) -> C)"},
        {"A & ~B -> C",      "((A & ~B) -> C)"},
        {"A <-> B -> C",     "(A <-> (B -> C))"},
        {"A -> B <-> C",     "((A -> B) <-> C)"},
        // associativity
        {"A -> B -> C",      "(A -> (B -> C))"},       // right
        {"A & B & C",        "((A & B) & C)"},         // left
        {"A | B | C",        "((A | B) | C)"},         // left
        {"A <-> B <-> C",    "((A <-> B) <-> C)"},     // left
        // parentheses and negation
        {"(A | B) & C",      "((A | B) & C)"},
        {"A & (B | C)",      "(A & (B | C))"},
        {"(A)",              "A"},
        {"((A))",            "A"},
        {"~~A",              "~~A"},
        {"~~~A",             "~~~A"},
        {"~(~A)",            "~~A"},
        {"A",                "A"},
        // word-form operators mixed in
        {"not A and B or C", "((~A & B) | C)"},
    };
    for (const Row& r : shapes)
        check_eq(string("[") + r.in + "] parses as " + r.expected, showFormula(r.in), r.expected);

    {
        const pair<const char*, const char*> same[] = {
            {"A and B", "A & B"}, {"A or B", "A | B"}, {"not A", "~A"},
            {"A implies B", "A -> B"}, {"A iff B", "A <-> B"},
            {"not A and B or C implies D iff E", "~A & B | C -> D <-> E"},
        };
        for (const auto& p : same)
            check_eq(string("word form [") + p.first + "] == symbol form [" + p.second + "]",
                     showFormula(p.first), showFormula(p.second));
    }

    check_eq("whitespace does not change the parse (tabs, newlines, no spaces)",
             showFormula("  A\t&\n B |C "), showFormula("A&B|C"));

    check_eq("parsing is deterministic / has no hidden state (same input twice)",
             showFormula("(A -> B) <-> ~C"), showFormula("(A -> B) <-> ~C"));

    {
        NodePtr v = parseFormula("A");
        NodePtr n = parseFormula("~A");
        NodePtr a = parseFormula("A & B");
        check("root node types: 'A' is VarNode, '~A' is NotNode(left=Var, right=null), 'A&B' is AndNode",
              dynamic_cast<VarNode*>(v.get()) && dynamic_cast<NotNode*>(n.get()) &&
                  n->left && !n->right && dynamic_cast<VarNode*>(n->left.get()) &&
                  dynamic_cast<AndNode*>(a.get()) && a->left && a->right);
    }

    {
        const auto toValues = [](const string& f) {
            vector<string> v;
            for (const Token& t : infixToPostfix(tokenize(f))) v.push_back(t.value);
            return v;
        };
        check("Shunting-Yard: 'A | B & C' -> postfix  A B C & |",
              toValues("A | B & C") == vector<string>({"A", "B", "C", "&", "|"}));
        check("Shunting-Yard: '(A | B) & C' -> postfix  A B | C &",
              toValues("(A | B) & C") == vector<string>({"A", "B", "|", "C", "&"}));
        check("Shunting-Yard: '~A & B' -> postfix  A ~ B &",
              toValues("~A & B") == vector<string>({"A", "~", "B", "&"}));
        check("Shunting-Yard: parentheses never appear in the postfix output",
              toValues("((A | B) & (C | D))") == vector<string>({"A", "B", "|", "C", "D", "|", "&"}));
    }

    {
        const string deepParens = string(500, '(') + "A" + string(500, ')');
        check_eq("500 nested parentheses parse to just 'A'", showFormula(deepParens), "A");

        string andChain = "A";
        for (int i = 0; i < 1500; ++i) andChain += " & A";
        const string s = showFormula(andChain);
        check("a 1,500-term conjunction chain parses without crashing",
              s.rfind("<exception", 0) != 0 && s.size() > 1500);

        string impChain = "A0";
        for (int i = 1; i < 1000; ++i) impChain += " -> A" + to_string(i);
        check("a 1,000-term implication chain (right-assoc, deep tree) parses without crashing",
              nodeCount(parseFormula(impChain)) == 1999);

        check_eq("2,000 stacked negations give 2,000 '~'",
                 to_string(showFormula(string(2000, '~') + "A").size()), "2001");
    }
}

// ============================================================================
//  3. SYNTAX ERRORS  (parser error reporting)
// ============================================================================
void validate_errors() {
    section("ERR", "3. SYNTAX-ERROR REPORTING  (invalid input must be rejected, precisely)");

    struct Row { const char* in; ErrorKind kind; size_t offset; };
    const Row rows[] = {
        {"",           ErrorKind::UNEXPECTED_END_OF_INPUT, 0},
        {"()",         ErrorKind::MISSING_OPERAND,         1},
        {"( )",        ErrorKind::MISSING_OPERAND,         2},
        {"& A",        ErrorKind::MISSING_OPERAND,         0},
        {"and A",      ErrorKind::MISSING_OPERAND,         0},
        {"A & & B",    ErrorKind::MISSING_OPERAND,         4},
        {"A -> -> B",  ErrorKind::MISSING_OPERAND,         5},
        {")(",         ErrorKind::MISSING_OPERAND,         0},
        {"A &",        ErrorKind::UNEXPECTED_END_OF_INPUT, 3},
        {"~",          ErrorKind::UNEXPECTED_END_OF_INPUT, 1},
        {"(",          ErrorKind::UNEXPECTED_END_OF_INPUT, 1},
        {"A implies",  ErrorKind::UNEXPECTED_END_OF_INPUT, 9},
        {"A B",        ErrorKind::MISSING_OPERATOR,        2},
        {"(A)B",       ErrorKind::MISSING_OPERATOR,        3},
        {"A~",         ErrorKind::MISSING_OPERATOR,        1},
        {"A ( B )",    ErrorKind::MISSING_OPERATOR,        2},
        {"(A | B",     ErrorKind::MISSING_CLOSING_PAREN,   6},
        {"A | B)",     ErrorKind::UNEXPECTED_CLOSING_PAREN,5},
        {"A ) B (",    ErrorKind::UNEXPECTED_CLOSING_PAREN,2},
    };
    for (const Row& r : rows) {
        const Caught c = parseExpectingError(r.in);
        check(string("invalid [") + r.in + "] -> SYNTAX error, correct kind and offset",
              c.threw && c.err.category == ErrorCategory::SYNTAX && c.err.kind == r.kind &&
                  c.err.offset == r.offset,
              c.threw ? "got kind=" + to_string(static_cast<int>(c.err.kind)) +
                            " offset=" + to_string(c.err.offset)
                      : "no ParseError thrown (formula was accepted!)");
    }

    {
        bool allGood = true;
        string bad;
        for (const Row& r : rows) {
            const Caught c = parseExpectingError(r.in);
            const bool ok = c.threw && c.err.line >= 1 && c.err.column >= 1 &&
                            !c.err.message.empty() && string(c.err.what()) == c.err.message;
            if (!ok) { allGood = false; bad = r.in; }
        }
        check("every error has line>=1, column>=1, a non-empty message, and what()==message",
              allGood, "problem with: " + bad);
    }

    {
        const Caught c = parseExpectingError("A $ B )");
        check("when a formula has several problems the LEXICAL one (found first) is reported",
              c.threw && c.err.category == ErrorCategory::LEXICAL && c.err.offset == 2);
    }

    {
        const Caught c = parseExpectingError("(A | B");
        check("missing ')' tells the user which token was expected",
              c.threw && c.err.expected == vector<string>{")"} && c.err.found.empty());
    }

    {
        bool allValid = true;
        string bad;
        for (const string f : {"A", "~A", "(A)", "A & B", "A|B", "A->B", "A<->B", "~(A&B)|C",
                               "((A))", "A and B", "not not A", "1 & 2 | 3", "x_1 -> y_2"}) {
            try { (void)parseFormula(f); } catch (...) { allValid = false; bad = f; }
        }
        check("a list of 13 VALID formulas is all accepted (no false rejections)", allValid, "rejected: " + bad);
    }
}

// ============================================================================
//  4. TRUTH TABLE ENGINE
// ============================================================================
void validate_truth_table() {
    section("TT", "4. TRUTH TABLE ENGINE  (generation, classification, equivalence)");
    TruthTableEngine engine;

    {
        const auto v = engine.collect_variables(*parseFormula("B & A | C & A"));
        check("variables are listed in order of FIRST APPEARANCE with no duplicates (B,A,C)",
              v == vector<string>({"B", "A", "C"}));
    }

    for (unsigned n = 1; n <= 8; ++n) {
        string f = "v1";
        for (unsigned i = 2; i <= n; ++i) f += " & v" + to_string(i);
        const TruthTable t = engine.truth_table(*parseFormula(f));
        check("a formula with " + to_string(n) + " variable(s) has exactly 2^" + to_string(n) + " = " +
                  to_string(1u << n) + " rows",
              t.variables.size() == n && t.num_rows() == (1ULL << n));
    }

    {
        struct Row { const char* f; const char* bits; };   // bits listed row 0 first
        const Row known[] = {
            {"A",           "FT"},
            {"~A",          "TF"},
            {"A & B",       "FFFT"},
            {"A | B",       "FTTT"},
            {"A -> B",      "TTFT"},
            {"A <-> B",     "TFFT"},
            {"A & ~B",      "FFTF"},
            {"B & A",       "FFFT"},
            {"A | ~A",      "TT"},
            {"A & ~A",      "FF"},
            {"(A -> B) & (B -> C)", "TTFTFFFT"},
            {"A -> B -> C", "TTTTTTFT"},   // false ONLY for A=T,B=T,C=F  (right associativity!)
        };
        for (const Row& r : known)
            check_eq(string("hand-computed truth table of [") + r.f + "] = " + r.bits,
                     tableBits(engine.truth_table(*parseFormula(r.f))), r.bits);
    }

    {
        const TruthTable t = engine.truth_table(*parseFormula("A & B & C"));
        const Assignment first = t.assignment(0);
        const Assignment last = t.assignment(7);
        const Assignment mid = t.assignment(5);   // binary 101
        const Assignment r6 = t.assignment(6);    // binary 110 (NOT a palindrome, so bit order matters)
        check("row order: row 0 is all FALSE, last row is all TRUE",
              !first.at("A") && !first.at("B") && !first.at("C") &&
                  last.at("A") && last.at("B") && last.at("C"));
        check("row 5 = binary 101 => A=T, B=F, C=T  (first variable is the most significant bit)",
              mid.at("A") && !mid.at("B") && mid.at("C"));
        check("row 6 = binary 110 => A=T, B=T, C=F  (proves MSB-first; 101 alone cannot, it is a palindrome)",
              r6.at("A") && r6.at("B") && !r6.at("C"));

        bool agree = true;
        for (const char* f : {"A & ~B & C", "(A -> B) <-> C", "A | (B & ~C)"}) {
            const NodePtr ast = parseFormula(f);
            const TruthTable tt = engine.truth_table(*ast);
            for (uint64_t r = 0; r < tt.num_rows(); ++r) {
                const Assignment a = tt.assignment(r);
                const Assignment mine = assignmentFor(tt.variables, r);
                Oracle o(mine);
                if (a != mine || engine.evaluate(*ast, a) != tt.value(r) || o.run(*ast) != tt.value(r))
                    agree = false;
            }
        }
        check("for every row: assignment(row) matches the documented bit order AND evaluate() matches the stored value",
              agree);
    }

    {
        const NodePtr f = parseFormula("A & ~B");
        Assignment a = {{"A", true}, {"B", false}};
        check("evaluate(): A & ~B with A=T,B=F is TRUE", engine.evaluate(*f, a));
        a["B"] = true;
        check("evaluate(): A & ~B with A=T,B=T is FALSE", !engine.evaluate(*f, a));

        bool threw = false;
        try { (void)engine.evaluate(*parseFormula("A & B"), {{"A", true}}); }
        catch (const runtime_error&) { threw = true; }
        check("evaluate(): an unbound variable throws std::runtime_error", threw);

        bool noThrow = true;
        try { (void)engine.evaluate(*parseFormula("A"), {{"A", true}, {"Z", false}}); }
        catch (...) { noThrow = false; }
        check("evaluate(): extra, unused variables in the assignment are harmless", noThrow);
    }

    {
        TruthTableEngine small(3);
        bool throwsAt4 = false, okAt3 = true;
        try { (void)small.truth_table(*parseFormula("A & B & C & D")); } catch (const length_error&) { throwsAt4 = true; }
        try { (void)small.truth_table(*parseFormula("A & B & C")); } catch (...) { okAt3 = false; }
        check("size guard: max_vars=3 rejects 4 variables (length_error) but accepts exactly 3",
              throwsAt4 && okAt3);

        string f = "v1";
        for (int i = 2; i <= 25; ++i) f += " & v" + to_string(i);
        bool throws25 = false;
        try { (void)engine.truth_table(*parseFormula(f)); } catch (const length_error&) { throws25 = true; }
        check("size guard: the DEFAULT limit (24) rejects a 25-variable formula instead of allocating 2^25 rows",
              throws25);
    }

    {
        const NodePtr t1 = parseFormula("A | ~A");
        const NodePtr t2 = parseFormula("A | B | C");
        const NodePtr t3 = parseFormula("A & ~A");
        check("BitVector padding bits stay 0: tautology over 1 var has exactly 2 ones (not 64)",
              engine.truth_table(*t1).results.count_ones() == 2);
        check("BitVector count: 'A | B | C' has 7 true rows out of 8",
              engine.truth_table(*t2).results.count_ones() == 7);
        check("BitVector count: contradiction has 0 true rows",
              engine.truth_table(*t3).results.count_ones() == 0);
    }

    {
        struct Row { const char* f; Classification c; };
        const Row rows[] = {
            {"A | ~A",                         Classification::TAUTOLOGY},
            {"A -> A",                         Classification::TAUTOLOGY},
            {"(A & (A -> B)) -> B",            Classification::TAUTOLOGY},   // modus ponens
            {"~(A & B) <-> (~A | ~B)",         Classification::TAUTOLOGY},   // De Morgan
            {"((A -> B) & (B -> C)) -> (A -> C)", Classification::TAUTOLOGY},// hypothetical syllogism
            {"A & ~A",                         Classification::CONTRADICTION},
            {"(A -> B) & A & ~B",              Classification::CONTRADICTION},
            {"A <-> ~A",                       Classification::CONTRADICTION},
            {"A",                              Classification::CONTINGENCY},
            {"A -> B",                         Classification::CONTINGENCY},
            {"A <-> B",                        Classification::CONTINGENCY},
        };
        for (const Row& r : rows) {
            const Classification got = engine.classify(engine.truth_table(*parseFormula(r.f)));
            check(string("classify [") + r.f + "] = " + to_string(r.c), got == r.c,
                  "got " + to_string(got));
        }
    }

    {
        const pair<const char*, const char*> equiv[] = {
            {"~(A & B)", "~A | ~B"},                       // De Morgan 1
            {"~(A | B)", "~A & ~B"},                       // De Morgan 2
            {"A -> B", "~A | B"},                          // material implication
            {"A -> B", "~B -> ~A"},                        // contrapositive
            {"A <-> B", "(A -> B) & (B -> A)"},            // iff definition
            {"A & (B | C)", "(A & B) | (A & C)"},          // distributivity
            {"A | (A & B)", "A"},                          // absorption (different variable sets!)
            {"A", "A & (B | ~B)"},                         // extra variable that cancels out
            {"~~A", "A"},                                  // double negation
            {"A & B", "B & A"},                            // commutativity
        };
        for (const auto& p : equiv) {
            const NodePtr x = parseFormula(p.first), y = parseFormula(p.second);
            check(string("EQUIVALENT: [") + p.first + "]  ==  [" + p.second + "]",
                  engine.are_equivalent(*x, *y) && engine.are_equivalent(*y, *x));
        }
        const pair<const char*, const char*> notEquiv[] = {
            {"A -> B", "B -> A"},                          // converse is NOT the same
            {"A & B", "A | B"},
            {"A", "B"},
            {"A -> B", "~A -> ~B"},                        // inverse is NOT the same
            {"A | B", "A <-> B"},
        };
        for (const auto& p : notEquiv) {
            const NodePtr x = parseFormula(p.first), y = parseFormula(p.second);
            check(string("NOT equivalent: [") + p.first + "]  !=  [" + p.second + "]",
                  !engine.are_equivalent(*x, *y));
        }
    }

    {
        // Cross-validation on >64 rows: exercises the multi-word bit-parallel path.
        const char* big[] = {
            "(A & B) | (C & D) | (E & F) | G",
            "A <-> B <-> C <-> D <-> E <-> F <-> G",
            "((A -> B) & (C -> D) & (E -> F)) -> (G | ~A)",
            "~(A | B | C | D | E | F | G | H)",
            "A & B & C & D & E & F & G & H",
            "(A | B) & (C | D) & (E | F) & (G | H) & ~(A & C & E & G)",
        };
        for (const char* f : big) {
            const NodePtr ast = parseFormula(f);
            const TruthTable t = engine.truth_table(*ast);
            bool allMatch = true;
            uint64_t trueRows = 0;
            for (uint64_t r = 0; r < t.num_rows(); ++r) {
                const Assignment a = assignmentFor(t.variables, r);
                Oracle o(a);
                const bool expected = o.run(*ast);
                if (t.value(r) != expected || engine.evaluate(*ast, a) != expected) allMatch = false;
                if (expected) ++trueRows;
            }
            check(string("bit-parallel table == independent oracle on all ") + to_string(t.num_rows()) +
                      " rows: [" + f + "]",
                  allMatch && t.results.count_ones() == trueRows);
        }
    }

    {
        ostringstream os;
        print_truth_table(os, engine.truth_table(*parseFormula("A & ~B")));
        const string out = os.str();
        const auto lines = static_cast<size_t>(count(out.begin(), out.end(), '\n'));
        check("print_truth_table: header + separator + 4 rows, mentions the variables",
              lines == 6 && out.find('A') != string::npos && out.find('B') != string::npos &&
                  out.find('T') != string::npos && out.find('F') != string::npos);
    }
}

// ============================================================================
//  5. NNF CONVERTER
// ============================================================================
void validate_nnf() {
    section("NNF", "5. NNF CONVERTER  (no ->, no <->, ~ only on variables, meaning preserved)");
    NNFConverter conv;
    TruthTableEngine engine;

    {
        struct Row { const char* in; const char* expected; };
        const Row rows[] = {
            {"~(A & B)",    "(~A | ~B)"},                 // De Morgan
            {"~(A | B)",    "(~A & ~B)"},                 // De Morgan
            {"A -> B",      "(~A | B)"},                  // implication elimination
            {"~(A -> B)",   "(A & ~B)"},
            {"~~A",         "A"},                         // double negation
            {"~~~A",        "~A"},
            {"~(~A & ~B)",  "(A | B)"},
            {"A <-> B",     "((A & B) | (~A & ~B))"},     // iff expansion
            {"~(A <-> B)",  "((A & ~B) | (~A & B))"},
            {"A",           "A"},
            {"~A",          "~A"},
        };
        for (const Row& r : rows)
            check_eq(string("NNF of [") + r.in + "] is " + r.expected,
                     show(conv.convert(*parseFormula(r.in))), r.expected);
    }

    {
        const char* formulas[] = {
            "~(A & B)", "~(A | B)", "A -> B", "~(A -> B)", "A <-> B", "~(A <-> B)",
            "~~(A | B)", "(A -> B) & (C <-> ~D)", "~((A -> B) -> C)", "~(A <-> (B <-> C))",
            "A -> (B -> (C -> D))", "((A -> B) -> C) -> D", "~(~(A & B) | ~(C | D))",
            "(A <-> B) <-> (C <-> D)", "~(A & (B | (C & ~D)))", "A", "~A",
        };
        bool shapeOk = true, equivOk = true, oracleOk = true;
        string badShape, badEq;
        for (const char* f : formulas) {
            const NodePtr orig = parseFormula(f);
            const NodePtr nnf = conv.convert(*orig);
            const string p = nnfProblem(*nnf);
            if (!p.empty()) { shapeOk = false; badShape = string(f) + " (" + p + ")"; }
            if (!engine.are_equivalent(*orig, *nnf)) { equivOk = false; badEq = f; }
            if (!oracleEquivalent(*orig, *nnf)) { oracleOk = false; badEq = f; }
        }
        check("17 hand-picked formulas: every NNF output has legal NNF shape", shapeOk, badShape);
        check("17 hand-picked formulas: NNF is logically equivalent (engine.are_equivalent)", equivOk, badEq);
        check("17 hand-picked formulas: NNF is logically equivalent (independent oracle)", oracleOk, badEq);
    }

    {
        bool idem = true;
        for (const char* f : {"~(A & B)", "A <-> B", "~((A -> B) -> C)", "(A | B) & ~(C <-> D)"}) {
            const NodePtr once = conv.convert(*parseFormula(f));
            const NodePtr twice = conv.convert(*once);
            if (show(once) != show(twice)) idem = false;
        }
        check("idempotent: converting an NNF again changes nothing", idem);
    }

    {
        bool unchanged = true;
        for (const char* f : {"~(A & B)", "A <-> B", "~((A -> B) -> C)"}) {
            const NodePtr orig = parseFormula(f);
            const string before = show(orig);
            (void)conv.convert(*orig);
            if (show(orig) != before) unchanged = false;
        }
        check("convert() builds a NEW tree: the original AST is not modified", unchanged);
    }

    {
        mt19937 rng(20240601);
        int shapeBad = 0, equivBad = 0, oracleBad = 0, roundTripBad = 0, idemBad = 0, engineVsOracleBad = 0;
        const int N = 400;
        for (int i = 0; i < N; ++i) {
            const NodePtr f = randomFormula(rng, 5, 4);
            const NodePtr n = conv.convert(*f);
            if (!nnfProblem(*n).empty()) ++shapeBad;
            if (!engine.are_equivalent(*f, *n)) ++equivBad;
            if (!oracleEquivalent(*f, *n)) ++oracleBad;
            if (show(conv.convert(*n)) != show(n)) ++idemBad;
            // print -> parse -> print must be the identity
            if (showFormula(show(f)) != show(f)) ++roundTripBad;
            // second opinion: engine's equivalence verdict must match the oracle for an unrelated pair
            const NodePtr g = randomFormula(rng, 3, 3);
            if (engine.are_equivalent(*f, *g) != oracleEquivalent(*f, *g)) ++engineVsOracleBad;
        }
        check("RANDOM (400 formulas, seed fixed): NNF shape always legal", shapeBad == 0,
              to_string(shapeBad) + " bad");
        check("RANDOM (400): NNF always equivalent per the engine", equivBad == 0, to_string(equivBad) + " bad");
        check("RANDOM (400): NNF always equivalent per the independent oracle", oracleBad == 0,
              to_string(oracleBad) + " bad");
        check("RANDOM (400): NNF is idempotent", idemBad == 0, to_string(idemBad) + " bad");
        check("RANDOM (400): print -> parse -> print round-trips (parser agrees with AST builder)",
              roundTripBad == 0, to_string(roundTripBad) + " bad");
        check("RANDOM (400 pairs): engine.are_equivalent agrees with the oracle", engineVsOracleBad == 0,
              to_string(engineVsOracleBad) + " bad");
    }

    {
        string chain = "A0";
        for (int i = 1; i <= 8; ++i) chain += " <-> A" + to_string(i);
        const NodePtr orig = parseFormula(chain);
        const NodePtr nnf = conv.convert(*orig);
        info("KNOWN LIMITATION: an 8-link '<->' chain grows from " + to_string(nodeCount(orig)) +
             " AST nodes to " + to_string(nodeCount(nnf)) +
             " nodes in NNF (exponential). This is why the Tseitin encoding exists.");
        check("even the blown-up NNF of the iff-chain is still correct (shape + equivalence)",
              nnfProblem(*nnf).empty() && engine.are_equivalent(*orig, *nnf));
    }
}

// ============================================================================
//  6. END-TO-END PIPELINE (text -> AST -> table -> NNF -> table)
// ============================================================================
void validate_pipeline() {
    section("E2E", "6. WORKLET-1 PIPELINE  (text -> AST -> truth table -> NNF -> truth table)");
    TruthTableEngine engine;
    NNFConverter conv;

    const char* formulas[] = {
        "(A & ~B) -> C", "A <-> (B | C)", "~(A -> B) | (C & D)",
        "(p implies q) and (q implies r) implies (p implies r)",
        "not (x1 and x2) iff (not x1 or not x2)",
    };
    for (const char* f : formulas) {
        const NodePtr ast = parseFormula(f);
        const TruthTable before = engine.truth_table(*ast);
        const NodePtr nnf = conv.convert(*ast);
        const TruthTable after = engine.truth_table(*nnf);
        // The NNF may mention the same variables in a different first-appearance order,
        // so compare semantically through the oracle rather than raw bit strings.
        check(string("pipeline keeps the meaning of [") + f + "]",
              oracleEquivalent(*ast, *nnf) &&
                  engine.classify(before) == engine.classify(after));
    }
}

}  // namespace

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i)
        if (string(argv[i]) == "--quiet" || string(argv[i]) == "-q") g_quiet = true;

    const pair<const char*, void (*)()> sections[] = {
        {"LEXER", validate_lexer},       {"PARSER", validate_parser},
        {"ERRORS", validate_errors},     {"TRUTH TABLE", validate_truth_table},
        {"NNF", validate_nnf},           {"PIPELINE", validate_pipeline},
    };
    for (const auto& sec : sections) {
        try {
            sec.second();
        } catch (const exception& e) {
            ++g_failed;
            g_failures.push_back(string(sec.first) + " section aborted by an unexpected exception: " + e.what());
            cout << "  [FAIL] " << sec.first << " section aborted: " << e.what() << "\n";
        }
    }

    cout << "\n------------------------------------------------------------\n";
    cout << "WORKLET 1 VALIDATION SUMMARY\n";
    cout << "  checks run : " << (g_passed + g_failed) << "\n";
    cout << "  passed     : " << g_passed << "\n";
    cout << "  failed     : " << g_failed << "\n";
    for (const string& f : g_failures) cout << "    - " << f << "\n";
    cout << "------------------------------------------------------------\n";
    cout << (g_failed == 0 ? "RESULT: ALL VALIDATION CHECKS PASSED\n" : "RESULT: SOME CHECKS FAILED\n");
    return g_failed == 0 ? 0 : 1;
}
