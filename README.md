# SAT-SOLVER — Worklets 1–3

## Project layout

```text
apps/                 Command-line, GUI backend, and benchmark entry points
docs/                 Project documentation
include/sat_solver/   Public headers
src/frontend/         Lexer, parser, truth-table, and NNF implementation
src/graph/            Graph-colouring integration
src/sat/              CNF encoders and DPLL solver
tests/                Test programs
build/                Generated objects, libraries, and test executables
```

Worklet 1 provides the propositional-logic front end:

```text
formula → lexer → parser → shared AST
                           ├─ optional truth table / classification
                           └─ NNF converter → CNF converter → DPLL solver
```

The command-line application solves propositional formulas and can optionally
print their truth table. The Tkinter GUI calls a C++ backend that uses the same
CNF converter and DPLL solver for formula solving and graph colouring.

## Formula syntax

- Variables begin with a letter or digit and may contain letters, digits, and underscores.
- Operators: `~` (NOT), `&` (AND), `|` (OR), `->` (implication), `<->` (iff).
- Parentheses may be used to group expressions.
- Precedence from highest to lowest: `~`, `&`, `|`, `->`, `<->`.
- Implication is right-associative; the other binary operators are left-associative.

## Build and run

Build and run the command-line SAT solver by feeding the command through stdin:

```sh
make
printf '%s\n' '--truth-table --pipeline nnf "(A & ~B) -> C"' | ./sat_solver
```

On Windows with MinGW, use `mingw32-make` to build and run the generated
executable from PowerShell:

```powershell
mingw32-make
"--truth-table --pipeline nnf (A & ~B) -> C" | .\sat_solver.exe
```

The output includes the formula in NNF, its SAT/UNSAT result, a satisfying
assignment when one exists, and solver statistics.

Choose direct Tseitin encoding instead of NNF conversion and distribution
with:

```sh
printf '%s\n' '--pipeline tseitin "(A <-> B) -> (C | ~A)"' | ./sat_solver
```

The default is `--pipeline nnf`. The `--truth-table` option can be combined
with either pipeline. Tseitin auxiliary variables are omitted from the printed
satisfying assignment.

Generate and classify a truth table as well:

```sh
printf '%s\n' '--truth-table "(A & ~B) -> C"' | ./sat_solver
```

Truth-table generation is opt-in; the engine guards against formulas with more
than 24 distinct variables by default.

## GUI

The Tkinter GUI supports three problem modes:

- **Graph colouring**: click two circular nodes to add an edge, then solve to
  see the colouring. Graphs support up to 100 nodes. Graph and Sudoku use the
  shared **Solver results** tab for solutions, encoding, DPLL trace, metrics,
  and heuristic performance. Graph mode compares all four heuristic
  combinations; Sudoku reports the selected combination.
- **Formula SAT**: enter any propositional formula using `~`, `&`, `|`, `->`,
  and `<->`. Syntax is checked as you type; select a CNF pipeline and solve to
  see SAT/UNSAT, a satisfying assignment, and solver statistics.
- **Sudoku**: enter clues in a 9×9 grid or generate a randomized puzzle with a
  chosen clue count, then solve to fill the remaining cells. Unit propagation
  and pure-literal options are available for the selected trace. Generated
  puzzles are solvable but are not checked for uniqueness.

Formula solving supports NNF-to-CNF and Tseitin pipelines.

For graph colouring, a satisfying result is shown directly on the graph;
unsatisfiable graphs are reported in the status area. Select an edge and use
**Remove selected** to delete it, or use **Clear graph** to start over.

Build its C++ backend and launch the Python interface with:

```sh
make gui-backend
python gui/main.py
```

On Windows with MinGW, build with `mingw32-make gui-backend`, then launch with
`py gui\main.py`. The GUI requires Python with Tkinter available.

## Encoder benchmark

Build the graph-colouring encoder and DPLL comparison program with:

```sh
make benchmark
./performance
```

It reports CNF and solver statistics for the supplied graph, comparing both
pipelines. It is a standalone benchmark, separate from the interactive GUI.
On Windows with MinGW, use `mingw32-make benchmark` and run
`.\performance.exe`.

## Use from another Worklet 1 component

```cpp
#include <sat_solver/nnf.h>
#include <sat_solver/parser.h>
#include <sat_solver/truth_table.h>

NodePtr parsed = parseFormula("(A & ~B) -> C");
NNFConverter converter;
NodePtr nnf = converter.convert(*parsed);

TruthTableEngine engine;
TruthTable table = engine.truth_table(*parsed); // optional
Classification classification = engine.classify(table); // optional
```

`parseFormula` returns the shared visitor-based AST declared in `ast.h`.
`NNFConverter::convert` creates a new AST with only variables, negated
variables, AND, and OR; it does not modify the parsed tree.

## Formula errors

`parseFormula` stops at the first lexical or syntax error and throws
`ParseError` from `parse_error.h`. The exception includes its category and kind,
a human-readable message, the byte offset and 1-based line/column, the offending
lexeme (empty at end of input), and expected tokens when applicable. The
command-line solver reports these errors and exits unsuccessfully. A caller
can inspect the error details like this:

```cpp
#include <sat_solver/parse_error.h>
#include <sat_solver/parser.h>

try {
    (void)parseFormula(user_input);
    // The formula is syntactically valid.
} catch (const ParseError& error) {
    // Show error.message and highlight error.line / error.column.
}
```

The AST structure is unchanged. Multi-error recovery is not implemented.

## Tests

Run the lexer, parser, truth-table, validation, NNF/CNF, SAT, graph-colouring,
and formula-to-SAT end-to-end tests with:

```sh
make test
```

Run only the formula-to-SAT end-to-end tests with `make e2e-test`.

The default `make` target builds the SAT solver, its library, and the Sudoku
program.

On Windows with MinGW, run `mingw32-make test`.
