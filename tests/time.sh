#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
cc=${CC:-clang}
work=build/tests/time
mkdir -p "$work/std"
printf 'invalid standard module shadow\n' > "$work/std/time.fe"
cp tests/cases/time.fe "$work/time.fe"
for mode in O0 O2; do
    "$fermi" build --library "$work/time.fe" --out-dir "$work" "-$mode"
    if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then
        "$cc" -std=c11 "-$mode" -fsanitize=address,undefined -iquote"$work" tests/integration/clock_input.c -o "$work/run"
    else
        "$cc" -std=c11 "-$mode" -Wall -Wextra -Werror -Wno-unused-function \
            -Wno-unused-but-set-variable -iquote"$work" tests/integration/clock_input.c -o "$work/run"
    fi
    "$work/run"
    if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then
        "$cc" -std=c11 "-$mode" -fsanitize=address,undefined -iquote"$work" tests/integration/clock_native.c "$work/time.o" -o "$work/native"
    else
        "$cc" -std=c11 "-$mode" -Wall -Wextra -Werror -iquote"$work" tests/integration/clock_native.c "$work/time.o" -o "$work/native"
    fi
    "$work/native"
    "$fermi" build examples/cli/deadline.fe -o "$work/deadline" "-$mode"
    "$work/deadline" > "$work/out"
    printf 'monotonic deadline initialized\n' > "$work/expected"
    cmp "$work/expected" "$work/out"
done
cat > "$work/rejected.fe" <<'SOURCE'
use std.time;
@budget(work = 1000, heap = 0)
func main() -> i32 {
    match (time.monotonic_ns()) {
        case Result.ok(let now: usize) { return 0; }
        case Result.err(let error: time.Error) { return 1; }
    }
}
SOURCE
if "$fermi" check "$work/rejected.fe" > "$work/out" 2> "$work/err"; then
    echo 'FAIL: clock access accepted in budgeted function' >&2
    exit 1
fi
grep -q 'E_EFFECT' "$work/err"
grep -q 'clock access' "$work/err"
grep -q '"clock.read"' "$work/time.contract.json"
test "$(grep -c '"proof": "static".*"heap_bytes": 0, "effects": \[\]' "$work/time.contract.json")" -eq 2
echo 'PASS: clock library isolation, native executable and effect separation'
