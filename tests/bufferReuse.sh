#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
work=build/tests/buffer-reuse
if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then work=build/tests/buffer-reuse-sanitize; fi
mkdir -p "$work"
for mode in O0 O2; do
    "$fermi" test tests/cases/bufferReuse.fe "-$mode" > "$work/$mode.log"
    grep -q '^test result: ok. 3 passed; 0 failed$' "$work/$mode.log"
    "$fermi" build --library tests/cases/bufferReuse.fe --out-dir "$work" "-$mode"
    extra=
    if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then extra=-fsanitize=address,undefined; fi
    "${CC:-clang}" -std=c11 "-$mode" -Wall -Wextra -Werror -Wno-unused-function -Wno-unused-but-set-variable $extra -I"$work" tests/integration/bufferReuse.c -o "$work/run"
    "$work/run"
    "$fermi" run examples/features/bufferReuse/main.fe --no-cache "-$mode" > "$work/example"
    printf 'Processed 128 records using reusable storage.\n' > "$work/expected"
    cmp "$work/example" "$work/expected"
done
printf '%s\n' 'PASS: record/scalar buffer reuse, zero capacity, invalid growth and reinitialization'
