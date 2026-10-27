# Source naming

New Fermi code uses the following conventions. Bootstrap sources are the first
complete application of these rules; existing public library names remain usable.

| Element | Convention | Examples |
| --- | --- | --- |
| Types | PascalCase | `Node`, `Tree`, `ParseError` |
| Functions and methods | camelCase | `parseExpr`, `nodeCount`, `errorOffset` |
| Variables and fields | camelCase | `errorToken`, `valueLength`, `firstNode` |
| Enum variants | camelCase | `invalidTokens`, `returnStmt` |
| Multiword source files | camelCase | `parserTests.fe`, `parserOracle.fe` |
| Directories and single words | lowercase | `bootstrap/frontend`, `lexer.fe` |
| Test functions | test + PascalCase suffix | `testTreeStructure` |

Use the shortest name that retains the role. `id`, `op`, `lhs`, `rhs`, `line` and
`column` are appropriate in their local contexts. Prefer `parseExpr` to a long
sentence-like function name. Avoid abbreviations that require a separate glossary.
Use `Id` in type names and `id` within camelCase identifiers.

The native test runner recognizes `test` followed by an ASCII uppercase letter,
as well as existing `test_*` functions. A bare `test` or `testhelper` is a helper.
Imported tests are not run; generic test declarations are rejected in either form.

Preserve external names required by an existing API: standard-library calls,
the compiler's C interfaces, generated ABI prefixes and platform functions.
Rename such interfaces only with a compatibility/migration contract. This naming
convention does not change Fermi identifier syntax or keyword spelling.
