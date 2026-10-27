#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
work=build/tests/signed-decimal
if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then
    work=build/tests/signed-decimal-sanitize
fi
mkdir -p "$work"
for mode in O0 O2; do
    "$fermi" build --library tests/cases/signed_decimal.fe --out-dir "$work" "-$mode"
    if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then
        "${CC:-clang}" -std=c11 "-$mode" -fsanitize=address,undefined -I"$work" \
            tests/integration/signed_decimal_input.c -o "$work/run"
    else
        "${CC:-clang}" -std=c11 "-$mode" -Wall -Wextra -Werror -Wno-unused-function \
            -Wno-unused-but-set-variable -I"$work" tests/integration/signed_decimal_input.c -o "$work/run"
    fi
    "$work/run"
    "$fermi" test examples/features/signed_numbers/main.fe "-$mode" > "$work/example-tests"
    grep -q '^test result: ok. 2 passed; 0 failed$' "$work/example-tests"
    "$fermi" run examples/features/signed_numbers/main.fe --no-cache "-$mode" > "$work/output"
    printf '%s\n' -1250 > "$work/expected"
    cmp "$work/expected" "$work/output"
done
