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

- Variables begin with a letter and may contain letters, digits, and underscores.
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

## Tests

Run focused lexer, parser, truth-table, and NNF tests with:

```sh
make test
```
