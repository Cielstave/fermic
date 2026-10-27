#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
cc=${CC:-clang}
work=build/tests/try-result
mkdir -p "$work"
for mode in O0 O2; do
    "$fermi" build --library tests/cases/try_result.fe --out-dir "$work" "-$mode"
    "$fermi" build --library tests/cases/try_owners.fe --out-dir "$work" "-$mode"
    if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then
        "$cc" -std=c11 "-$mode" -fsanitize=address,undefined -I"$work" \
            tests/integration/try_result.c "$work/try_result.o" -o "$work/values"
        "$cc" -std=c11 "-$mode" -fsanitize=address,undefined -I"$work" \
            tests/integration/try_ownership.c -o "$work/owners"
    else
        "$cc" -std=c11 "-$mode" -Wall -Wextra -Werror -I"$work" \
            tests/integration/try_result.c "$work/try_result.o" -o "$work/values"
        "$cc" -std=c11 "-$mode" -Wall -Wextra -Werror -Wno-unused-function \
            -Wno-unused-but-set-variable -I"$work" tests/integration/try_ownership.c -o "$work/owners"
    fi
    "$work/values"
    "$work/owners"
    "$fermi" build examples/cli/try_result.fe -o "$work/example" "-$mode"
    "$work/example" > "$work/actual"
    printf 'Worker limit accepted.\n' > "$work/expected"
    cmp "$work/expected" "$work/actual"
done
cat > "$work/effects.fe" <<'SOURCE'
use std.io;
@budget(work = 1000, heap = 0)
func output() -> Result<usize, io.Error> {
    return Result.ok(try(io.write("output")));
}
SOURCE
if "$fermi" check "$work/effects.fe" > "$work/out" 2> "$work/err"; then
    echo 'FAIL: try concealed an output effect' >&2
    exit 1
fi
grep -q E_EFFECT "$work/err"
cat > "$work/mismatch.fe" <<'SOURCE'
use std.io;
use std.num;
func output() -> Result<usize, io.Error> {
    return Result.ok(try(num.parse_usize("8")));
}
SOURCE
if "$fermi" check "$work/mismatch.fe" --diagnostic-format=json > "$work/out" 2> "$work/err"; then
    echo 'FAIL: try converted an incompatible error type' >&2
    exit 1
fi
grep -q '"code":"E_TRY"' "$work/err"
grep -q 'match the Result and convert its error explicitly' "$work/err"
echo 'PASS: try example, native effect propagation and explicit error conversion diagnostics'
