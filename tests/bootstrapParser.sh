#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
work=build/tests/bootstrap-parser
if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then work=build/tests/bootstrap-parser-sanitize; fi
mkdir -p "$work"
run=$(mktemp -d "$work/run-XXXXXX")
node tests/bootstrapParserFixtures.js "$run"

reject() {
    file=$1
    message=$2
    status=0
    "$run/fermi-parse" "$run/$file.fe" > "$run/out" 2> "$run/err" || status=$?
    test "$status" = 65
    test ! -s "$run/out"
    grep -Fq "$message" "$run/err"
}

for mode in O0 O2; do
    "$fermi" test tests/cases/unaryPrecedence.fe "-$mode" > "$run/unary"
    grep -q '^test result: ok. 6 passed; 0 failed$' "$run/unary"
    "$fermi" test bootstrap/parserTests.fe "-$mode" > "$run/tests"
    grep -q '^test result: ok. 5 passed; 0 failed$' "$run/tests"
    "$fermi" build --library bootstrap/parserOracle.fe --out-dir "$run" "-$mode"
    if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then
        "${CC:-clang}" -std=c11 "-$mode" -fsanitize=address,undefined -Icompiler/include -I"$run" \
            tests/integration/bootstrapParser.c compiler/support/context.c compiler/frontend/lex.c \
            compiler/frontend/types.c compiler/frontend/parse.c compiler/analysis/effects.c -o "$run/compare"
    else
        "${CC:-clang}" -std=c11 "-$mode" -Wall -Wextra -Werror -Wno-unused-function \
            -Wno-unused-but-set-variable -Icompiler/include -I"$run" \
            tests/integration/bootstrapParser.c compiler/support/context.c compiler/frontend/lex.c \
            compiler/frontend/types.c compiler/frontend/parse.c compiler/analysis/effects.c -o "$run/compare"
    fi
    "$run/compare"
    "$fermi" build bootstrap/parse.fe "-$mode" -o "$run/fermi-parse"
    "$run/fermi-parse" bootstrap/examples/arithmetic.fe > "$run/out" 2> "$run/err"
    printf 'AST nodes: 35\n' > "$run/expected"
    cmp "$run/expected" "$run/out"
    test ! -s "$run/err"
    "$run/fermi-parse" "$run/large.fe" > "$run/out"
    grep -q '^AST nodes: 2003$' "$run/out"
    reject missing-type "requires explicit type at 1:25"
    reject missing-value "let requires an initializer at 1:29"
    reject unsupported 'syntax is outside the bootstrap parser subset'
    reject generic 'syntax is outside the bootstrap parser subset'
    reject unterminated 'unterminated block'
    reject empty 'source must contain a function'
    reject depth-limit 'syntax nesting limit exceeded'
    reject chain-limit 'expression chain exceeds 64 operators'
    reject param-limit 'function exceeds 16 parameters'
    reject func-limit 'module exceeds 128 functions'
    reject node-limit 'syntax node limit exceeded'
    "$fermi" build bootstrap/examples/arithmetic.fe "-$mode" -o "$run/arithmetic"
    status=0
    "$run/arithmetic" || status=$?
    test "$status" = 39
    "$run/fermi-parse" bootstrap/examples/controlFlow.fe > "$run/control-out"
    printf 'AST nodes: 49\n' > "$run/control-expected"
    cmp "$run/control-expected" "$run/control-out"
    "$fermi" build bootstrap/examples/controlFlow.fe "-$mode" -o "$run/control"
    status=0
    "$run/control" || status=$?
    test "$status" = 19
    printf 'PASS: bootstrap parser %s, AST structure, limits, diagnostics and native arithmetic\n' "$mode"
done
