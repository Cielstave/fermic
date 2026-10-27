#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
work=build/tests/array-repeat
if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then
    work=build/tests/array-repeat-sanitize
fi
mkdir -p "$work"
cat > "$work/effects.fe" <<'SOURCE'
use std.io;
func seed() -> u8 { io.println(read "seed"); return 7; }
func main() -> void {
    let values: u8[8] = [seed(); 8];
    if (values[7] == 7) { io.println(read "ready"); }
}
SOURCE
cat > "$work/rejected-effects.fe" <<'SOURCE'
use std.io;
func seed() -> u8 { io.println(read "seed"); return 7; }
@effects()
func f() -> void { let values: u8[8] = [seed(); 8]; }
SOURCE
for mode in O0 O2; do
    "$fermi" test tests/cases/array_repeat.fe "-$mode" > "$work/tests"
    grep -q '^test result: ok. 12 passed; 0 failed$' "$work/tests"
    "$fermi" build --library tests/cases/array_repeat_owners.fe --out-dir "$work" "-$mode"
    if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then
        "${CC:-clang}" -std=c11 "-$mode" -fsanitize=address,undefined -I"$work" \
            tests/integration/array_repeat_owners.c -o "$work/owners"
    else
        "${CC:-clang}" -std=c11 "-$mode" -Wall -Wextra -Werror -Wno-unused-function \
            -Wno-unused-but-set-variable -I"$work" tests/integration/array_repeat_owners.c -o "$work/owners"
    fi
    "$work/owners"
    "$fermi" test examples/features/array_initializers/main.fe "-$mode" > "$work/example-tests"
    grep -q '^test result: ok. 2 passed; 0 failed$' "$work/example-tests"
    "$fermi" run examples/features/array_initializers/main.fe --no-cache "-$mode" > "$work/output"
    printf '10\n' > "$work/expected"
    cmp "$work/expected" "$work/output"
    "$fermi" run "$work/effects.fe" --no-cache "-$mode" > "$work/output"
    printf 'seed\nready\n' > "$work/expected"
    cmp "$work/expected" "$work/output"
done
cat > "$work/mismatch.fe" <<'SOURCE'
func f() -> void { let values: u8[3] = [0; 2]; }
SOURCE
status=0
"$fermi" check "$work/mismatch.fe" > "$work/output" 2> "$work/error" || status=$?
test "$status" -eq 1
grep -q 'array repeat count 2 does not match expected length 3' "$work/error"
status=0
"$fermi" check "$work/rejected-effects.fe" > "$work/output" 2> "$work/error" || status=$?
test "$status" -eq 1
grep -q 'error\[E_EFFECT\]' "$work/error"
"$fermi" explain tests/cases/array_repeat.fe > "$work/explain"
grep -q '^bounded: static upper bound' "$work/explain"
grep -q '^once_bounded: static upper bound' "$work/explain"
printf 'PASS: array repetition O0/O2, values, effects, ownership and bounded work\n'
