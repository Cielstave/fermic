#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
work=build/tests/strings
if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then work=build/tests/strings-sanitize; fi
mkdir -p "$work"
for mode in O0 O2; do
    "$fermi" test tests/cases/strings.fe "-$mode" > "$work/tests"
    grep -q '^test result: ok. 12 passed; 0 failed$' "$work/tests"
    "$fermi" build --library tests/cases/strings.fe --out-dir "$work" "-$mode"
    if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then
        "${CC:-clang}" -std=c11 "-$mode" -fsanitize=address,undefined -I"$work" tests/integration/string_index_input.c -o "$work/index"
    else
        "${CC:-clang}" -std=c11 "-$mode" -Wall -Wextra -Werror -Wno-unused-function -Wno-unused-but-set-variable -I"$work" tests/integration/string_index_input.c -o "$work/index"
    fi
    "$work/index"
    "$fermi" test examples/features/strings/main.fe "-$mode" > "$work/example-tests"
    grep -q '^test result: ok. 2 passed; 0 failed$' "$work/example-tests"
    "$fermi" run examples/features/strings/main.fe --no-cache "-$mode" > "$work/output"
    printf 'Hello, Fermi!\n' > "$work/expected"
    cmp "$work/expected" "$work/output"
    printf 'PASS: UTF-8 strings %s, content helpers and immutable byte access\n' "$mode"
done
