#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
work=build/tests/input
mkdir -p "$work"
: >"$work/empty"
: >"$work/data"
i=0
while [ "$i" -lt 600 ]; do
    printf 'Fermi\000\377 input\r\n' >>"$work/data"
    i=$((i + 1))
done
for mode in O0 O2; do
    "$fermi" build examples/cli/copy_stdin.fe -o "$work/copy" "-$mode"
    "$work/copy" <"$work/data" >"$work/actual"
    cmp "$work/data" "$work/actual"
    cat "$work/data" | "$work/copy" >"$work/actual"
    cmp "$work/data" "$work/actual"
    "$work/copy" <"$work/empty" >"$work/actual"
    test ! -s "$work/actual"
    status=0
    "$work/copy" 0>"$work/unreadable" >"$work/actual" || status=$?
    test "$status" -eq 1
    test ! -s "$work/actual"
done
reject() {
    if "$fermi" check "$work/rejected.fe" >"$work/out" 2>"$work/error"; then
        echo 'FAIL: invalid input program accepted' >&2
        exit 1
    fi
    grep -q "$1" "$work/error"
}
cat >"$work/rejected.fe" <<'SOURCE'
use std.io;
func main() -> i32 {
    let buffer: u8[1] = [0];
    io.read_into(edit buffer);
    return 0;
}
SOURCE
reject E_MUT
cat >"$work/rejected.fe" <<'SOURCE'
use std.io;
func main() -> i32 {
    var buffer: u8[1];
    io.read_into(edit buffer);
    return 0;
}
SOURCE
reject E_INIT
cat >"$work/rejected.fe" <<'SOURCE'
use std.io;
func main() -> i32 {
    var buffer: u8[1] = [0];
    io.read_into(read buffer);
    return 0;
}
SOURCE
reject E_ACCESS
cat >"$work/rejected.fe" <<'SOURCE'
use std.io;
func main() -> i32 {
    var buffer: u8[1] = [0];
    io.read_into(edit buffer);
    return 0;
}
SOURCE
reject E_RESULT
cat >"$work/rejected.fe" <<'SOURCE'
use std.io;
@budget(work = 100, heap = 0)
func bounded(bytes: edit u8[]) -> Result<usize, io.Error> {
    return helper(edit bytes);
}
func helper(bytes: edit u8[]) -> Result<usize, io.Error> {
    return io.read_into(edit bytes);
}
SOURCE
reject E_EFFECT
cat >"$work/receive.fe" <<'SOURCE'
use std.io;
export func receive(bytes: edit u8[]) -> Result<usize, io.Error> {
    return io.read_into(edit bytes);
}
export func recurse(bytes: edit u8[], again: bool) -> Result<usize, io.Error> {
    if (again) { return bridge(edit bytes); }
    return io.read_into(edit bytes);
}
func bridge(bytes: edit u8[]) -> Result<usize, io.Error> {
    match (io.write(read "")) {
        case Result.ok(let count: usize) { return recurse(edit bytes, false); }
        case Result.err(let error: io.Error) { return Result.err(error); }
    }
}
SOURCE
"$fermi" build --library "$work/receive.fe" --out-dir "$work/library"
grep -q '"effects": \["io.read"\]' "$work/library/receive.contract.json"
grep -q '"effects": \["io.read", "io.write"\]' "$work/library/receive.contract.json"
cc=${FERMI_CC:-clang}
for mode in O0 O2; do
    "$cc" -std=c11 "-$mode" -Wall -Wextra -Werror -Wno-unused-function \
        -Wno-unused-but-set-variable -Dread=fermi_test_read -I"$work/library" \
        tests/integration/input.c "$work/library/receive.c" -o "$work/runtime"
    "$work/runtime"
done
echo 'PASS: stdin copying, EOF, read errors, mutable access, input effects and recursive propagation'
