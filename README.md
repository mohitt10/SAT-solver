# SAT-SOLVER — Worklet 1

Worklet 1 provides the propositional-logic front end:

```text
formula → lexer → parser → shared AST
                           ├─ optional truth table / classification
                           └─ NNF converter → NNF AST
```

It does not implement CNF conversion or SAT solving. The NNF AST is the
handoff point for Worklet 2.

## Formula syntax

- Variables begin with a letter or digit and may contain letters, digits, and underscores.
- Operators: `~` (NOT), `&` (AND), `|` (OR), `->` (implication), `<->` (iff).
- Parentheses may be used to group expressions.
- Precedence from highest to lowest: `~`, `&`, `|`, `->`, `<->`.
- Implication is right-associative; the other binary operators are left-associative.

## Build and run

Build the command-line demo:

```sh
make
./sat_solver "(A & ~B) -> C"
```

Generate and classify a truth table as well:

```sh
./sat_solver --truth-table "(A & ~B) -> C"
```

The demo always prints the formula in NNF. Truth-table generation is opt-in;
the engine guards against formulas with more than 24 distinct variables by
default.

## Graph-coloring GUI

The Qt Widgets GUI lets you choose a vertex and colour count, add graph edges by
clicking two vertices, and ask the SAT solver for a valid coloring. A
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

## Formula validation errors

`parseFormula` stops at the first lexical or syntax error and throws
`ParseError` from `parse_error.h`. The exception includes its category and kind,
a human-readable message, the byte offset and 1-based line/column, the offending
lexeme (empty at end of input), and expected tokens when applicable. A
formula-validation UI can display the first error like this:

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

Run the lexer, parser, truth-table, NNF, SAT, and graph-coloring tests with:

```sh
make test
```
