#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
cc=${CC:-clang}
work=build/tests/ranges-native
mkdir -p "$work"
for mode in O0 O2; do
    "$fermi" build --library tests/cases/ranges.fe --out-dir "$work" "-$mode"
    test "$(grep -c 'i16 sum: at most 4096 additions from zero' "$work/ranges.c")" -eq 1
    if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then
        "$cc" -std=c11 "-$mode" -fsanitize=address,undefined -I"$work" \
            tests/integration/ranges.c "$work/ranges.c" -o "$work/run"
    else
        "$cc" -std=c11 "-$mode" -Wall -Wextra -Werror -I"$work" \
            tests/integration/ranges.c "$work/ranges.o" -o "$work/run"
    fi
    "$work/run"
done
