#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
cc=${CC:-clang}
work=build/tests/function-exits
mkdir -p "$work"
for mode in O0 O2; do
    "$fermi" build --library tests/cases/function_exits.fe --out-dir "$work" "-$mode"
    sed '/^fe_exit:/a\
    ++exit_count;
' "$work/function_exits.c" > "$work/function_exits.instrumented.c"
    if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then
        "$cc" -std=c11 "-$mode" -fsanitize=address,undefined -I"$work" \
            tests/integration/function_exits.c -o "$work/run"
    else
        "$cc" -std=c11 "-$mode" -Wall -Wextra -Werror -Wno-unused-function \
            -Wno-unused-but-set-variable -I"$work" tests/integration/function_exits.c -o "$work/run"
    fi
    "$work/run"
done
