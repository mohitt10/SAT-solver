# Lexer Algorithm

## Aim
To convert a propositional logic formula into a sequence of tokens.

## Input
A propositional logic formula containing:
- Variables: `a` to `z`
- AND: `&`
- OR: `|`
- NOT: `~`
- IMPLIES: `->`
- IFF: `<->`
- Parentheses: `(` and `)`

## Algorithm
1. Start from the first character of the formula.
2. Read the formula one character at a time.
3. Ignore spaces.
4. If the character is between `a` and `z`, create a `VARIABLE` token.
5. If the character is `&`, `|`, `~`, `(`, or `)`, create the corresponding token.
6. If the current characters are `->`, create an `IMPLIES` token and skip both characters.
7. If the current characters are `<->`, create an `IFF` token and skip all three characters.
8. If the character does not match any valid token, report an invalid character.
9. Continue until the end of the formula is reached.
10. Return the list of tokens.

## Example
Input:
```text
(a & b) -> c

Output:
LPAREN VARIABLE AND VARIABLE RPAREN IMPLIES VARIABLE

Complexity
Time Complexity: O(n)
Space Complexity: O(n)
,where n is the length of the input formula.