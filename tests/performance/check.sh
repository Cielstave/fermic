#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
cc=${CC:-clang}
work=build/tests/performance
mkdir -p "$work"
for mode in O0 O2; do
    "$fermi" build --library examples/design/filter_above.fe --out-dir "$work" "-$mode"
    "$fermi" build --library examples/design/sum_samples.fe --out-dir "$work" "-$mode"
    if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then
        "$cc" -std=c11 "-$mode" -fsanitize=address,undefined -I"$work" \
            tests/performance/kernels.c tests/performance/reference.c tests/performance/validate.c \
            "$work/filter_above.c" "$work/sum_samples.c" -o "$work/check"
    else
        "$cc" -std=c11 "-$mode" -Wall -Wextra -Werror -I"$work" \
            tests/performance/kernels.c tests/performance/reference.c tests/performance/validate.c \
            "$work/filter_above.o" "$work/sum_samples.o" -o "$work/check"
    fi
    "$work/check" --validate
done
