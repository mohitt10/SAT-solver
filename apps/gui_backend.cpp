#include "graph_coloring.h"
#include "nnf.h"
#include "parse_error.h"
#include "parser.h"
#include "sat_types.hpp"
#include "truth_table.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

std::string json_string(const std::string& value) {
    std::ostringstream out;
    out << '"';
    for (unsigned char ch : value) {
        switch (ch) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (ch < 0x20) {
                    const char digits[] = "0123456789abcdef";
                    out << "\\u00" << digits[ch >> 4] << digits[ch & 0x0f];
                } else {
                    out << static_cast<char>(ch);
                }
        }
    }
    out << '"';
    return out.str();
}

std::string event_name(SolverEventType type) {
    switch (type) {
        case SolverEventType::Decision: return "decision";
        case SolverEventType::UnitPropagation: return "unit_propagation";
        case SolverEventType::PureLiteral: return "pure_literal";
        case SolverEventType::Conflict: return "conflict";
        case SolverEventType::Backtrack: return "backtrack";
        case SolverEventType::Result: return "result";
    }
    return "result";
}

class AstPrinter : public Visitor {
public:
    std::string print(const ASTNode& root) {
        text_.clear();
        prefix_.clear();
        last_child_ = true;
        is_root_ = true;
        root.accept(*this);
        return text_;
    }

    void visit(const VarNode& node) override {
        line(node.name);
    }
    void visit(const NotNode& node) override {
        line("NOT");
        child(node.left, true);
    }
    void visit(const AndNode& node) override { binary(node, "AND"); }
    void visit(const OrNode& node) override { binary(node, "OR"); }
    void visit(const ImpliesNode& node) override { binary(node, "IMPLIES"); }
    void visit(const IffNode& node) override { binary(node, "IFF"); }

private:
    std::string text_;
    std::string prefix_;
    bool last_child_ = true;
    bool is_root_ = true;

    void line(const std::string& value) {
        text_ += prefix_;
        if (!is_root_) {
            text_ += last_child_ ? "`-- " : "|-- ";
        }
        text_ += value + '\n';
    }
    void child(const NodePtr& node, bool is_last) {
        if (!node) throw std::invalid_argument("Malformed AST: missing child.");
        const std::string previous_prefix = prefix_;
        const bool previous_last_child = last_child_;
        const bool previous_is_root = is_root_;
        if (!is_root_) {
            prefix_ += last_child_ ? "    " : "|   ";
        }
        last_child_ = is_last;
        is_root_ = false;
        node->accept(*this);
        prefix_ = previous_prefix;
        last_child_ = previous_last_child;
        is_root_ = previous_is_root;
    }
    void binary(const ASTNode& node, const std::string& name) {
        line(name);
        child(node.left, false);
        child(node.right, true);
    }
};

std::string literal_json(const Literal& literal) {
    std::ostringstream out;
    out << "{\"variable\":" << json_string(literal.getVariable())
        << ",\"negated\":" << (literal.isNegated() ? "true" : "false") << '}';
    return out.str();
}

std::string clause_json(const std::vector<Literal>& clause) {
    std::ostringstream out;
    out << '[';
    for (std::size_t i = 0; i < clause.size(); ++i) {
        if (i) out << ',';
        out << literal_json(clause[i]);
    }
    out << ']';
    return out.str();
}

int literal_count(const CNF& cnf) {
    int count = 0;
    for (const Clause& clause : cnf.getClauses()) count += clause.size();
    return count;
}

std::string clauses_json(const CNF& cnf) {
    std::ostringstream out;
    out << '[';
    const auto& clauses = cnf.getClauses();
    for (std::size_t i = 0; i < clauses.size(); ++i) {
        if (i) out << ',';
        out << clause_json(clauses[i].getLiterals());
    }
    out << ']';
    return out.str();
}

std::string trace_json(const SolverResult& result) {
    std::ostringstream out;
    out << "{\"trace_truncated\":"
        << (result.trace_truncated ? "true" : "false") << ",\"events\":[";
    for (std::size_t i = 0; i < result.trace.size(); ++i) {
        if (i) out << ',';
        const SolverTraceEvent& event = result.trace[i];
        out << "{\"type\":" << json_string(event_name(event.type))
            << ",\"variable\":" << json_string(event.variable)
            << ",\"value\":" << (event.value ? "true" : "false")
            << ",\"level\":" << event.decision_level
            << ",\"clause\":" << clause_json(event.clause)
            << ",\"undone\":[";
        for (std::size_t j = 0; j < event.undone_assignments.size(); ++j) {
            if (j) out << ',';
            out << "{\"variable\":"
                << json_string(event.undone_assignments[j].first)
                << ",\"value\":"
                << (event.undone_assignments[j].second ? "true" : "false")
                << '}';
        }
        out << "]}";
    }
    out << "]}";
    return out.str();
}

std::string parse_error_json(const ParseError& error) {
    const char* kinds[] = {
        "invalid_character", "invalid_token", "unexpected_token",
        "missing_operand", "missing_operator", "missing_closing_parenthesis",
        "unexpected_closing_parenthesis", "unexpected_end_of_input"
    };
    std::ostringstream out;
    out << "{\"ok\":false,\"parse_error\":{\"category\":"
        << json_string(error.category == ErrorCategory::LEXICAL ? "lexical" : "syntax")
        << ",\"kind\":"
        << json_string(kinds[static_cast<std::size_t>(error.kind)])
        << ",\"message\":" << json_string(error.message)
        << ",\"offset\":" << error.offset << ",\"line\":" << error.line
        << ",\"column\":" << error.column
        << ",\"found\":" << json_string(error.found) << ",\"expected\":[";
    for (std::size_t i = 0; i < error.expected.size(); ++i) {
        if (i) out << ',';
        out << json_string(error.expected[i]);
    }
    out << "]}}";
    return out.str();
}

std::string error_json(const std::string& message) {
    return "{\"ok\":false,\"error\":" + json_string(message) + '}';
}

struct Options {
    std::string operation;
    std::string unit_prop = "on";
    std::string pure_literal = "on";
    std::string trace_unit = "on";
    std::string trace_pure = "on";
    std::size_t trace_limit = 10000;
    std::uint64_t start = 0;
    std::uint64_t count = 100;
    int nodes = 0;
    int colours = 0;
};

std::string option_value(int& index, int argc, char* argv[]) {
    if (index + 1 >= argc) throw std::invalid_argument("Missing option value.");
    return argv[++index];
}

Options parse_options(int argc, char* argv[]) {
    if (argc < 3 || std::string(argv[1]) != "--gui") {
        throw std::invalid_argument("Expected --gui followed by an operation.");
    }
    Options options;
    options.operation = argv[2];
    for (int i = 3; i < argc; ++i) {
        const std::string flag = argv[i];
        if (flag == "--unit-prop") options.unit_prop = option_value(i, argc, argv);
        else if (flag == "--pure-literal") options.pure_literal = option_value(i, argc, argv);
        else if (flag == "--trace-unit") options.trace_unit = option_value(i, argc, argv);
        else if (flag == "--trace-pure") options.trace_pure = option_value(i, argc, argv);
        else if (flag == "--trace-limit") options.trace_limit = std::stoull(option_value(i, argc, argv));
        else if (flag == "--start") options.start = std::stoull(option_value(i, argc, argv));
        else if (flag == "--count") options.count = std::stoull(option_value(i, argc, argv));
        else if (flag == "--nodes") options.nodes = std::stoi(option_value(i, argc, argv));
        else if (flag == "--colours") options.colours = std::stoi(option_value(i, argc, argv));
        else throw std::invalid_argument("Unknown option: " + flag);
    }
    return options;
}

bool enabled(const std::string& value) {
    if (value == "on") return true;
    if (value == "off") return false;
    throw std::invalid_argument("Boolean options must be 'on' or 'off'.");
}

std::string analysis_json(const NodePtr& formula) {
    std::ostringstream out;
    const std::vector<std::string> variables =
        TruthTableEngine().collect_variables(*formula);
    out << "{\"ok\":true,\"variables\":[";
    for (std::size_t i = 0; i < variables.size(); ++i) {
        if (i) out << ',';
        out << json_string(variables[i]);
    }
    out << "],\"variable_count\":" << variables.size()
        << ",\"ast\":" << json_string(AstPrinter().print(*formula));
    if (variables.size() > 24) {
        out << ",\"classification\":null,\"classification_message\":"
            << json_string("Unavailable: truth tables are limited to 24 variables.")
            << ",\"truth_table_rows\":null}";
    } else {
        const TruthTable table = TruthTableEngine().truth_table(*formula);
        out << ",\"classification\":"
            << json_string(to_string(TruthTableEngine().classify(table)))
            << ",\"classification_message\":null,\"truth_table_rows\":"
            << table.num_rows() << '}';
    }
    return out.str();
}

std::string truth_table_json(
    const NodePtr& formula,
    std::uint64_t start,
    std::uint64_t count) {
    const TruthTableEngine engine;
    const std::vector<std::string> variables = engine.collect_variables(*formula);
    if (variables.size() > 24) {
        return error_json("Truth tables are limited to 24 variables.");
    }
    const std::uint64_t total_rows = 1ULL << variables.size();
    if (count == 0 || count > 500 || start > total_rows) {
        return error_json("Invalid truth-table page; count must be 1-500.");
    }
    const std::uint64_t end = std::min(total_rows, start + count);
    std::ostringstream out;
    out << "{\"ok\":true,\"variables\":[";
    for (std::size_t i = 0; i < variables.size(); ++i) {
        if (i) out << ',';
        out << json_string(variables[i]);
    }
    out << "],\"total_rows\":" << total_rows << ",\"start\":" << start
        << ",\"rows\":[";
    for (std::uint64_t row = start; row < end; ++row) {
        if (row != start) out << ',';
        const Assignment assignment = [&]() {
            Assignment values;
            for (std::size_t i = 0; i < variables.size(); ++i) {
                values[variables[i]] =
                    ((row >> (variables.size() - i - 1)) & 1ULL) != 0;
            }
            return values;
        }();
        out << "{\"values\":[";
        for (std::size_t i = 0; i < variables.size(); ++i) {
            if (i) out << ',';
            out << (assignment.at(variables[i]) ? "true" : "false");
        }
        out << "],\"result\":"
            << (engine.evaluate(*formula, assignment) ? "true" : "false") << '}';
    }
    out << "]}";
    return out.str();
}

CNF formula_cnf(const NodePtr& formula) {
    const NodePtr nnf = NNFConverter().convert(*formula);
    return convert_nnf_to_cnf(nnf);
}

std::string cnf_json(const CNF& cnf) {
    std::ostringstream out;
    out << "{\"ok\":true,\"clauses\":" << clauses_json(cnf)
        << ",\"clause_count\":" << cnf.size()
        << ",\"literal_count\":" << literal_count(cnf) << '}';
    return out.str();
}

std::string solve_json(
    const NodePtr& formula,
    const Options& options,
    bool include_trace) {
    const CNF cnf = formula_cnf(formula);
    SolverConfig config;
    config.use_unit_prop = enabled(options.unit_prop);
    config.use_pure_literal = enabled(options.pure_literal);
    config.record_trace = include_trace;
    config.trace_limit = options.trace_limit;
    const SolverResult result = solve(cnf, config);
    const std::vector<std::string> variables =
        TruthTableEngine().collect_variables(*formula);

    std::ostringstream out;
    out << "{\"ok\":true,\"satisfiable\":"
        << (result.is_sat ? "true" : "false") << ",\"assignment\":{";
    bool first = true;
    for (const std::string& variable : variables) {
        const auto value = result.final_assignments.find(variable);
        if (value == result.final_assignments.end()) continue;
        if (!first) out << ',';
        first = false;
        out << json_string(variable) << ':'
            << (value->second ? "true" : "false");
    }
    out << "},\"variable_count\":" << variables.size()
        << ",\"clause_count\":" << cnf.size()
        << ",\"literal_count\":" << literal_count(cnf)
        << ",\"solver_time_ms\":" << result.total_execution_time_ms
        << ",\"decisions\":" << result.decision_count
        << ",\"backtracks\":" << result.backtrack_count;
    if (include_trace) out << ",\"trace\":" << trace_json(result);
    out << '}';
    return out.str();
}

std::string puzzle_json(const Options& options, std::istream& input) {
    std::vector<std::pair<int, int>> edges;
    int first = 0;
    int second = 0;
    while (input >> first) {
        if (!(input >> second)) {
            throw std::invalid_argument("Each edge must contain two vertex numbers.");
        }
        edges.emplace_back(first, second);
    }
    if (!input.eof()) throw std::invalid_argument("Invalid edge list.");

    const auto encode_start = std::chrono::steady_clock::now();
    Encoder encoder(options.nodes, options.colours, edges);
    encoder.build_initial_string();
    encoder.addedgeconstraint();
    const auto encode_end = std::chrono::steady_clock::now();
    const std::string source = encoder.getencodedstring();

    const auto parse_start = std::chrono::steady_clock::now();
    const NodePtr formula = parseFormula(source);
    const auto parse_end = std::chrono::steady_clock::now();
    const auto cnf_start = std::chrono::steady_clock::now();
    const CNF cnf = formula_cnf(formula);
    const auto cnf_end = std::chrono::steady_clock::now();

    const SolverConfig configs[] = {
        {false, false}, {false, true}, {true, false}, {true, true}
    };
    std::vector<SolverResult> results;
    for (const SolverConfig& config : configs) results.push_back(solve(cnf, config));

    SolverConfig trace_config;
    trace_config.use_unit_prop = enabled(options.trace_unit);
    trace_config.use_pure_literal = enabled(options.trace_pure);
    trace_config.record_trace = true;
    trace_config.trace_limit = options.trace_limit;
    const SolverResult traced = solve(cnf, trace_config);

    std::vector<int> colouring;
    bool valid = false;
    if (results.back().is_sat) {
        std::vector<std::pair<std::string, int>> values;
        for (const auto& value : results.back().final_assignments) {
            values.emplace_back(value.first, value.second ? 1 : 0);
        }
        colouring = encoder.decoderesult(values);
        valid = encoder.checkresult(values);
        if (!valid) throw std::runtime_error("Solver returned an invalid colouring.");
    }

    std::ostringstream out;
    out << "{\"ok\":true,\"satisfiable\":"
        << (results.back().is_sat ? "true" : "false")
        << ",\"valid_coloring\":" << (valid ? "true" : "false")
        << ",\"nodes\":" << options.nodes << ",\"colours\":" << options.colours
        << ",\"edges\":[";
    for (std::size_t i = 0; i < edges.size(); ++i) {
        if (i) out << ',';
        out << '[' << edges[i].first << ',' << edges[i].second << ']';
    }
    out << "],\"colouring\":[";
    for (std::size_t i = 0; i < colouring.size(); ++i) {
        if (i) out << ',';
        out << colouring[i];
    }
    const auto milliseconds = [](auto start, auto end) {
        return std::chrono::duration<double, std::milli>(end - start).count();
    };
    out << "],\"variable_count\":" << options.nodes * options.colours
        << ",\"edge_count\":" << edges.size()
        << ",\"formula_length\":" << source.size()
        << ",\"clause_count\":" << cnf.size()
        << ",\"literal_count\":" << literal_count(cnf)
        << ",\"encoding_time_ms\":" << milliseconds(encode_start, encode_end)
        << ",\"parsing_time_ms\":" << milliseconds(parse_start, parse_end)
        << ",\"cnf_time_ms\":" << milliseconds(cnf_start, cnf_end)
        << ",\"comparisons\":[";
    for (std::size_t i = 0; i < results.size(); ++i) {
        if (i) out << ',';
        out << "{\"unit_propagation\":"
            << (configs[i].use_unit_prop ? "true" : "false")
            << ",\"pure_literal\":"
            << (configs[i].use_pure_literal ? "true" : "false")
            << ",\"satisfiable\":" << (results[i].is_sat ? "true" : "false")
            << ",\"solver_time_ms\":" << results[i].total_execution_time_ms
            << ",\"decisions\":" << results[i].decision_count
            << ",\"backtracks\":" << results[i].backtrack_count << '}';
    }
    out << "],\"trace\":" << trace_json(traced) << '}';
    return out.str();
}

std::string run(const Options& options, std::istream& input) {
    if (options.operation == "puzzle") return puzzle_json(options, input);
    const std::string source((std::istreambuf_iterator<char>(input)),
                             std::istreambuf_iterator<char>());
    const NodePtr formula = parseFormula(source);
    if (options.operation == "analyze") return analysis_json(formula);
    if (options.operation == "truth-table") {
        return truth_table_json(formula, options.start, options.count);
    }
    if (options.operation == "cnf") return cnf_json(formula_cnf(formula));
    if (options.operation == "solve") return solve_json(formula, options, false);
    if (options.operation == "trace") return solve_json(formula, options, true);
    throw std::invalid_argument("Unknown operation: " + options.operation);
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        const Options options = parse_options(argc, argv);
        try {
            std::cout << run(options, std::cin) << '\n';
        } catch (const ParseError& error) {
            std::cout << parse_error_json(error) << '\n';
        } catch (const std::exception& error) {
            std::cout << error_json(error.what()) << '\n';
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
