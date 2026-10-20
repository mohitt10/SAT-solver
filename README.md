# SAT-SOLVER — Worklets 1–3

## Project layout

```text
apps/                 Command-line, GUI, and benchmark entry points
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
print their truth table. The Qt GUI uses the same CNF converter and DPLL solver
to solve graph-colouring instances.

## Formula syntax

- Variables begin with a letter or digit and may contain letters, digits, and underscores.
- Operators: `~` (NOT), `&` (AND), `|` (OR), `->` (implication), `<->` (iff).
- Parentheses may be used to group expressions.
- Precedence from highest to lowest: `~`, `&`, `|`, `->`, `<->`.
- Implication is right-associative; the other binary operators are left-associative.

## Build and run

Build and run the command-line SAT solver:

```sh
make
./sat_solver "(A & ~B) -> C"
```

On Windows with MinGW, use `mingw32-make` to build and run the generated
executable from PowerShell:

```powershell
mingw32-make
.\sat_solver.exe "(A & ~B) -> C"
```

The output includes the formula in NNF, its SAT/UNSAT result, a satisfying
assignment when one exists, and solver statistics.

Choose direct Tseitin encoding instead of NNF conversion and distribution
with:

```sh
./sat_solver --pipeline tseitin "(A <-> B) -> (C | ~A)"
```

The default is `--pipeline nnf`. The `--truth-table` option can be combined
with either pipeline. Tseitin auxiliary variables are omitted from the printed
satisfying assignment.

Generate and classify a truth table as well:

```sh
./sat_solver --truth-table "(A & ~B) -> C"
```

Truth-table generation is opt-in; the engine guards against formulas with more
than 24 distinct variables by default.

## GUI

The GUI supports two problem modes:

- **Graph coloring**: choose vertices and colours, then add edges on the canvas.
- **Formula SAT**: enter any propositional formula using `~`, `&`, `|`, `->`,
  and `<->`. Syntax is checked as you type; select a CNF pipeline and solve to
  see SAT/UNSAT, a satisfying assignment, and solver statistics.

Both modes let you choose the NNF-to-CNF or Tseitin pipeline.

For graph coloring, a satisfying result is shown directly on the graph;
unsatisfiable graphs are reported in the status area. Right-click to cancel an
in-progress edge, or use **Clear edges** to start over.

Install Qt 6 Widgets and `pkg-config`, then build and launch with:

```sh
make gui
./sat_solver_gui
```

On Windows with MinGW and a Qt installation configured for `pkg-config`, run
`mingw32-make gui` and then `.\sat_solver_gui.exe`.

At launch, the GUI clears inherited `GTK_PATH` and `GIO_MODULE_DIR` values to
avoid incompatible GTK modules in environments such as the VS Code Snap
terminal.

The default Qt package is `Qt6Widgets`. To build with a Qt 5 installation,
specify `make gui QT_PACKAGE=Qt5Widgets` (or
`mingw32-make gui QT_PACKAGE=Qt5Widgets` on Windows).

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

Run the lexer, parser, truth-table, NNF/CNF, SAT, and graph-colouring tests with:

```sh
make test
```

On Windows with MinGW, run `mingw32-make test`.
