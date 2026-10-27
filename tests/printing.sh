#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
cc=${CC:-clang}
work=build/tests/printing
mkdir -p "$work"
for mode in O0 O2; do
    "$fermi" build --library tests/cases/printing.fe --out-dir "$work" "-$mode"
    if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then
        "$cc" -std=c11 "-$mode" -fsanitize=address,undefined -I"$work" \
            tests/integration/printing_io.c -o "$work/run"
    else
        "$cc" -std=c11 "-$mode" -Wall -Wextra -Werror -Wno-unused-function \
            -Wno-unused-but-set-variable -I"$work" tests/integration/printing_io.c -o "$work/run"
    fi
    "$work/run"
    "$fermi" build examples/cli/hello.fe -o "$work/hello" "-$mode"
    "$work/hello" > "$work/actual"
    printf 'Hello, world!\n' > "$work/expected"
    cmp "$work/expected" "$work/actual"
    status=0
    "$work/hello" 1<"$work/expected" 2>"$work/error" || status=$?
    test "$status" -eq 1
    grep -q 'runtime fault\[5\]: standard output write failed' "$work/error"
done
cat > "$work/empty.fe" <<'SOURCE'
func main() -> void {}
SOURCE
"$fermi" run --no-cache "$work/empty.fe"
cat > "$work/explicit.fe" <<'SOURCE'
func main() -> void { return; }
SOURCE
"$fermi" run --no-cache "$work/explicit.fe"
cat > "$work/arguments.fe" <<'SOURCE'
use std.args;
use std.io;
func main() -> void {
    match (args.count()) {
        case Result.ok(let count: usize) {
            if (count == 1) { io.println("argument received"); }
        }
        case Result.err(let error: args.Error) { io.println("argument failure"); }
    }
}
SOURCE
"$fermi" run --no-cache "$work/arguments.fe" -- sample > "$work/actual"
printf 'argument received\n' > "$work/expected"
cmp "$work/expected" "$work/actual"
cat > "$work/rejected.fe" <<'SOURCE'
use std.io;
@budget(work = 1000, heap = 0)
func main() -> void { io.println("effect"); }
SOURCE
if "$fermi" check "$work/rejected.fe" > "$work/out" 2> "$work/error"; then
    echo 'FAIL: printing was accepted as pure budgeted code' >&2
    exit 1
fi
grep -q E_EFFECT "$work/error"
echo 'PASS: void entry, concise Hello World and explicit output faults'
