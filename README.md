# SAT-SOLVER — Worklets 1–3

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

The output includes the formula in NNF, its SAT/UNSAT result, a satisfying
assignment when one exists, and solver statistics.

Generate and classify a truth table as well:

```sh
./sat_solver --truth-table "(A & ~B) -> C"
```

Truth-table generation is opt-in; the engine guards against formulas with more
than 24 distinct variables by default.

## Graph-coloring GUI

The Qt Widgets GUI lets you choose a vertex and colour count, add graph edges by
clicking two vertices, and ask the SAT solver for a valid colouring. A
satisfiable result is shown directly on the graph; unsatisfiable graphs are
reported in the status area. Right-click to cancel an in-progress edge, or use
**Clear edges** to start over.

Install Qt 6 Widgets and `pkg-config`, then build and launch with:

```sh
make gui
./sat_solver_gui
```

At launch, the GUI clears inherited `GTK_PATH` and `GIO_MODULE_DIR` values to
avoid incompatible GTK modules in environments such as the VS Code Snap
terminal.

The default Qt package is `Qt6Widgets`. To build with a Qt 5 installation,
specify `make gui QT_PACKAGE=Qt5Widgets`.

## Encoder benchmark

Build the graph-colouring encoder and DPLL comparison program with:

```sh
make benchmark
./performance
```

It reports CNF and solver statistics for the supplied graph. It is a
standalone benchmark, separate from the interactive GUI.

## Use from another Worklet 1 component

```cpp
#include "nnf.h"
#include "parser.h"
#include "truth_table.h"

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
#include "parse_error.h"
#include "parser.h"

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
