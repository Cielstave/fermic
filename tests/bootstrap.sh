#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
work=build/tests/bootstrap
if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then work=build/tests/bootstrap-sanitize; fi
mkdir -p "$work"
run=$(mktemp -d "$work/run-XXXXXX")
node tests/bootstrapFixtures.js "$run"

expect_failure() {
    expected_status=$1
    expected_message=$2
    shift 2
    status=0
    "$run/fermi-bootstrap" "$@" > "$run/out" 2> "$run/err" || status=$?
    test "$status" = "$expected_status"
    test ! -s "$run/out"
    printf '%s\n' "$expected_message" > "$run/expected-error"
    cmp "$run/expected-error" "$run/err"
}

expect_count() {
    expected_count=$1
    source=$2
    "$run/fermi-bootstrap" "$source" > "$run/out" 2> "$run/err"
    test ! -s "$run/err"
    printf 'tokens (including EOF): %s\n' "$expected_count" > "$run/expected-out"
    cmp "$run/expected-out" "$run/out"
}

for mode in O0 O2; do
    "$fermi" test bootstrap/tests.fe "-$mode" > "$run/tests"
    grep -q '^test result: ok. 5 passed; 0 failed$' "$run/tests"
    "$fermi" build --library bootstrap/oracle.fe --out-dir "$run" "-$mode"
    if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then
        "${CC:-clang}" -std=c11 "-$mode" -fsanitize=address,undefined -Icompiler/include -I"$run" \
            tests/integration/bootstrapLexer.c compiler/support/context.c compiler/frontend/lex.c -o "$run/compare"
    else
        "${CC:-clang}" -std=c11 "-$mode" -Wall -Wextra -Werror -Wno-unused-function \
            -Wno-unused-but-set-variable -Icompiler/include -I"$run" \
            tests/integration/bootstrapLexer.c compiler/support/context.c compiler/frontend/lex.c -o "$run/compare"
    fi
    "$run/compare" bootstrap/*.fe bootstrap/frontend/*.fe library/std/*.fe tests/cases/*.fe examples/cli/*.fe
    "$fermi" build bootstrap/main.fe "-$mode" -o "$run/fermi-bootstrap"
    expect_count 1 "$run/empty.fe"
    expect_count 12 "$run/valid.fe"
    expect_count 1 "$run/source-limit.fe"
    expect_count 131072 "$run/token-limit.fe"
    for source in "$run"/chunk-*.fe; do expect_count 3 "$source"; done
    expect_failure 64 'fermi-bootstrap: usage: fermi-bootstrap SOURCE'
    expect_failure 64 'fermi-bootstrap: usage: fermi-bootstrap SOURCE' "$run/valid.fe" extra
    expect_failure 66 'fermi-bootstrap: cannot open source file' "$run/missing.fe"
    expect_failure 66 'fermi-bootstrap: cannot open source file' "$run"
    expect_failure 65 'fermi-bootstrap: unsupported character at 2:1' "$run/invalid.fe"
    expect_failure 65 'fermi-bootstrap: NUL byte in comment at 1:1' "$run/nul.fe"
    expect_failure 65 'fermi-bootstrap: source exceeds 1 MiB' "$run/source-overflow.fe"
    expect_failure 65 'fermi-bootstrap: too many tokens at 1:131072' "$run/token-overflow.fe"
    "$run/fermi-bootstrap" bootstrap/main.fe > "$run/self-out"
    grep -q '^tokens (including EOF): [0-9][0-9]*$' "$run/self-out"
    printf 'PASS: bootstrap %s, source I/O, token ownership, chunk boundaries and CLI failures\n' "$mode"
done
