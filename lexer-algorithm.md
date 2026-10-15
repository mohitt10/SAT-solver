# Lexer Algorithm

> **Note:** This document outlines my understanding of the lexer algorithm and serves as a reference for implementation and further development.

## Aim
To convert a propositional logic formula into a sequence of tokens.

## Input
A propositional logic formula containing:
- Variables: Strings starting with a letter (`a-z` or `A-Z`), followed by zero or more letters, digits or underscores
- AND: `&`
- OR: `|`
- NOT: `~`
- IMPLIES: `->`
- IFF: `<->`
- Parentheses: `(` and `)`

## Algorithm
1. Start from the first character of the formula.
2. Read the formula one character at a time.
3. Ignore whitespace.
4. If the character is a letter, collect consecutive letters, digits and underscores to form a complete variable name.
5. Create a `VARIABLE` token containing the complete variable name.
6. If the character is `&`, `|`, `~`, `(` or `)`, create the corresponding token.
7. If the current characters are `->`, create an `IMPLIES` token and skip both characters.
8. If the current characters are `<->`, create an `IFF` token and skip all three characters.
9. If the character does not match any valid token, report an invalid character.
10. Continue until the end of the formula is reached.
11. Return the list of tokens.

## Example

**Input:**
```text
(red & blue) -> green
```

**Output:**
```text
LPAREN VARIABLE(red) AND VARIABLE(blue) RPAREN IMPLIES VARIABLE(green)
```

## Complexity
- **Time Complexity:** O(n)
- **Space Complexity:** O(n)

Where `n` is the length of the input formula.
