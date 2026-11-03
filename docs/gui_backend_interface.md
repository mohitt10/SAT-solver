# Python GUI backend interface

The Python desktop GUI launches `sat_solver_backend` once per request. It must
pass an argument list to the process API (never a shell command), send request
data on standard input, and read one JSON object from standard output. Errors
written by the backend go to standard error. Parse and input errors are returned
as JSON with `ok: false`; process failures and timeouts are handled by Python.

Build the executable with `make gui-backend` on Linux or
`mingw32-make gui-backend` on Windows.

## Formula requests

Formula requests receive the complete formula text on standard input:

```text
sat_solver_backend --gui analyze
sat_solver_backend --gui truth-table --start 0 --count 100
sat_solver_backend --gui cnf
sat_solver_backend --gui cnf --pipeline tseitin
sat_solver_backend --gui solve --unit-prop on --pure-literal on
sat_solver_backend --gui trace --pipeline tseitin --unit-prop on --pure-literal on --trace-limit 10000
```

`analyze` returns the variable list, an indented AST string, and a
tautology/contradiction/contingency classification. Classification is omitted
with an explanatory message above the 24-variable truth-table limit.

`truth-table` returns at most 500 rows, beginning at `start`. Rows contain a
Boolean value for each listed variable and the formula result. Requests over
the truth-table variable limit return an error response.

`cnf`, `solve`, and `trace` accept `--pipeline nnf|tseitin` (default `nnf`).
`cnf` returns the selected pipeline, clauses as arrays of
`{ "variable": "...", "negated": false }` literal objects, and clause and
literal counts. NNF conversion uses distribution, which can grow exponentially;
Tseitin conversion introduces auxiliary variables to avoid that growth.

`solve` returns the selected pipeline, `satisfiable`, the original-variable
assignment when SAT, and variable, clause, literal, time, decision, and
backtrack metrics. `trace` returns the same result plus a bounded ordered event
list and `trace_truncated`. The Formula tab exposes this pipeline choice for
both CNF analysis and solving.

## Puzzle request

Graph-colouring data is sent one edge per input line:

```text
sat_solver_backend --gui puzzle --nodes 3 --colours 3 --trace-unit on --trace-pure on
```

Example standard input:

```text
1 2
2 3
3 1
```

The response includes the decoded colouring and validity when SAT, graph and
formula sizes, encoding/parsing/CNF times, four DPLL comparisons (unit
propagation on/off crossed with pure-literal elimination on/off), and the
selected comparison's bounded trace. The GUI lets users choose which of the
two heuristics are enabled in the trace; the performance comparison always
runs all four combinations. The default trace configuration enables both
optimizations. Puzzle truth tables are not generated.

## Sudoku request

Sudoku requests send 81 space-separated digits on standard input, in row-major
order. Use `0` for an empty cell:

```text
sat_solver_backend --gui sudoku --trace-unit on --trace-pure on
```

The backend validates the input, encodes the cell, row, column, and 3×3-box
constraints as CNF, and returns `satisfiable`, a 9×9 solved grid when SAT,
encoding statistics, the selected solver configuration's metrics, and a
bounded trace using the selected `--trace-unit` and `--trace-pure` settings.
Unlike graph-colouring requests, Sudoku runs only the selected configuration
so the optional performance comparison does not multiply solve time.

## Trace events

Each event includes `type`, `variable`, `value`, `level`, `clause`, and
`undone`. Event types are `decision`, `unit_propagation`, `pure_literal`,
`conflict`, `backtrack`, and `result`. Propagation events include their
forcing unit clause. Backtracks include assignments being undone. Unused
fields are empty/default values. A trace limit of zero records no events and
sets `trace_truncated` if the solver attempts to record one.

## Parse errors

Parse errors return `ok: false` and a `parse_error` object containing
`category`, `kind`, `message`, `offset`, `line`, `column`, `found`, and
`expected`. The current parser reports the first error only.
