#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
cc=${CC:-clang}
work=build/tests/arguments
mkdir -p "$work/std"
for module in args fmt; do
    printf 'invalid standard-library shadow\n' > "$work/std/$module.fe"
done
cp tests/cases/arguments.fe "$work/arguments.fe"
cp tests/cases/args_echo.fe "$work/echo.fe"
expect_exit() {
    expected=$1
    shift
    actual=0
    "$@" > "$work/out" 2> "$work/err" || actual=$?
    if [ "$actual" -ne "$expected" ]; then
        cat "$work/err" >&2
        echo "FAIL: expected exit $expected, received $actual" >&2
        exit 1
    fi
}
printf 'Fermi\n' > "$work/input"
dd if=/dev/zero of="$work/large" bs=4096 count=3 2>/dev/null
printf x >> "$work/large"
for mode in O0 O2; do
    "$fermi" build --library "$work/arguments.fe" --out-dir "$work" "-$mode"
    if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then
        "$cc" -std=c11 "-$mode" -fsanitize=address,undefined -I"$work" \
            tests/integration/arguments_input.c -o "$work/run"
    else
        "$cc" -std=c11 "-$mode" -Wall -Wextra -Werror -Wno-unused-function \
            -Wno-unused-but-set-variable -I"$work" tests/integration/arguments_input.c -o "$work/run"
    fi
    "$work/run"
    "$fermi" build "$work/echo.fe" -o "$work/echo ; literal" "-$mode"
    raw_arg=$(printf '\377')
    expect_exit 0 "$work/echo ; literal" '' 'two words' '*' '-flag' 'semi;literal' '$value' "$raw_arg"
    printf '%s\n' '' 'two words' '*' '-flag' 'semi;literal' '$value' "$raw_arg" > "$work/expected"
    cmp "$work/expected" "$work/out"
    test ! -s "$work/err"
    expect_exit 0 "$work/echo ; literal"
    test ! -s "$work/out"
    expect_exit 65 "$work/echo ; literal" '12345678901234567'
    test ! -s "$work/out"
    "$fermi" build examples/tools/bytecount/main.fe -o "$work/bytecount" "-$mode"
    expect_exit 0 "$work/bytecount" 6 < "$work/input"
    printf '6\n' > "$work/expected"
    cmp "$work/expected" "$work/out"
    test ! -s "$work/err"
    expect_exit 0 "$work/bytecount" 18446744073709551615 < "$work/input"
    cmp "$work/expected" "$work/out"
    expect_exit 0 "$work/bytecount" 0 < /dev/null
    printf '0\n' > "$work/expected"
    cmp "$work/expected" "$work/out"
    expect_exit 0 "$work/bytecount" 12289 < "$work/large"
    printf '12289\n' > "$work/expected"
    cmp "$work/expected" "$work/out"
    expect_exit 65 "$work/bytecount" 12288 < "$work/large"
    test ! -s "$work/out"
    grep -q 'input exceeds configured byte limit' "$work/err"
    expect_exit 65 "$work/bytecount" 5 < "$work/input"
    test ! -s "$work/out"
    expect_exit 64 "$work/bytecount" < /dev/null
    expect_exit 64 "$work/bytecount" 1 2 < /dev/null
    for value in '' -1 '+1' ' 6' 18446744073709551616 000000000000000000001 secret-value; do
        expect_exit 64 "$work/bytecount" "$value" < /dev/null
        test ! -s "$work/out"
        grep -q '^usage: bytecount MAX_BYTES$' "$work/err"
        if grep -q 'secret-value' "$work/err"; then
            echo 'FAIL: raw argument entered diagnostics' >&2
            exit 1
        fi
    done
    expect_exit 74 "$work/bytecount" 6 0> "$work/unreadable"
    grep -q 'cannot read standard input' "$work/err"
    status=0
    "$work/bytecount" 6 < "$work/input" 1< "$work/input" 2> "$work/err" || status=$?
    test "$status" -eq 74
    status=0
    "$work/bytecount" 2< "$work/input" > "$work/out" || status=$?
    test "$status" -eq 74
    test ! -s "$work/out"
done
cat > "$work/fail.h" <<'SOURCE'
#include <stddef.h>
#include <stdlib.h>
static void *failed_allocate(size_t size) { (void)size; return NULL; }
#define malloc failed_allocate
SOURCE
cat > "$work/fail-cc.sh" <<'SOURCE'
#!/bin/sh
if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then
    exec "${CC:-clang}" -fsanitize=address,undefined -include build/tests/arguments/fail.h "$@"
fi
exec "${CC:-clang}" -include build/tests/arguments/fail.h "$@"
SOURCE
chmod +x "$work/fail-cc.sh"
FERMI_CC="$work/fail-cc.sh" "$fermi" build examples/tools/bytecount/main.fe -o "$work/no-memory" -O2
expect_exit 71 "$work/no-memory" 6 < "$work/input"
test ! -s "$work/out"
grep -q 'cannot allocate input buffer' "$work/err"
reject() {
    if "$fermi" check "$work/rejected.fe" > "$work/rejected.out" 2> "$work/rejected.err"; then
        echo 'FAIL: invalid argument access accepted' >&2
        exit 1
    fi
    grep -q "$1" "$work/rejected.err"
}
cat > "$work/rejected.fe" <<'SOURCE'
use std.args;
func main() -> i32 { args.count(); return 0; }
SOURCE
reject E_RESULT
cat > "$work/rejected.fe" <<'SOURCE'
use std.args;
func main() -> i32 { let output: u8[1] = [0]; args.read_into(0, edit output); return 0; }
SOURCE
reject E_MUT
cat > "$work/rejected.fe" <<'SOURCE'
use std.args;
func main() -> i32 { var output: u8[1]; args.read_into(0, edit output); return 0; }
SOURCE
reject E_INIT
cat > "$work/rejected.fe" <<'SOURCE'
use std.args;
@budget(work = 1000, heap = 0)
func main() -> i32 {
    match (args.count()) {
        case Result.ok(let count: usize) { return 0; }
        case Result.err(let error: args.Error) { return 1; }
    }
}
SOURCE
reject E_EFFECT
grep -q 'process argument access' "$work/rejected.err"
grep -q '"name": "format_value".*"proof": "static".*"heap_bytes": 0, "effects": \[\]' "$work/arguments.contract.json"
grep -q '"process.args"' "$work/arguments.contract.json"
echo 'PASS: native CLI arguments, numeric output, bounded streaming and application failures'
