#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
work=build/tests/io
mkdir -p "$work/std"
# A project module must not replace the installed standard library.
printf 'invalid project shadow\n' >"$work/std/io.fe"
cp examples/cli/hello.fe "$work/hello.fe"
printf 'Hello, world!\n' >"$work/expected"
for mode in O0 O2; do
    "$fermi" build "$work/hello.fe" -o "$work/hello" "-$mode"
    "$work/hello" >"$work/actual"
    cmp "$work/expected" "$work/actual"
    status=0
    "$work/hello" 1<"$work/expected" || status=$?
    test "$status" -eq 1

done
cat >"$work/bytes.fe" <<'SOURCE'
use std.io;
func send(bytes: read u8[]) -> i32 {
    match (io.write(read bytes)) {
        case Result.ok(let count: usize) { return 0; }
        case Result.err(let error: io.Error) { return 1; }
    }
}
func main() -> i32 {
    if (send(read "") != 0) { return 1; }
    if (send(read "quote: \"; slash: \\; tab:\t; cr:\r; newline:\n") != 0) { return 2; }
    let bytes: u8[3] = [65, 0, 66];
    return send(read bytes);
}
SOURCE
printf 'quote: "; slash: \\; tab:\t; cr:\r; newline:\nA\000B' >"$work/bytes.expected"
for mode in O0 O2; do
    "$fermi" build "$work/bytes.fe" -o "$work/bytes" "-$mode"
    "$work/bytes" >"$work/bytes.actual"
    cmp "$work/bytes.expected" "$work/bytes.actual"
done
reject() {
    if "$fermi" check "$work/rejected.fe" >"$work/out" 2>"$work/error"; then
        echo 'FAIL: invalid I/O program accepted' >&2
        exit 1
    fi
    grep -q "$1" "$work/error"
}
cat >"$work/rejected.fe" <<'SOURCE'
use std.io;
func main() -> i32 { io.write(read "discarded"); return 0; }
SOURCE
reject E_RESULT
cat >"$work/rejected.fe" <<'SOURCE'
func change(bytes: edit u8[]) -> void { bytes[0] = 65; }
func main() -> i32 { change(edit "immutable"); return 0; }
SOURCE
reject E_ACCESS
cat >"$work/rejected.fe" <<'SOURCE'
native func write(bytes: read u8[]) -> usize;
SOURCE
reject 'restricted to the standard library'
cat >"$work/rejected.fe" <<'SOURCE'
use std.io;
@budget(work = 1000, heap = 0)
func main() -> i32 { return helper(); }
func helper() -> i32 {
    match (io.write(read "effect")) {
        case Result.ok(let count: usize) { return 0; }
        case Result.err(let error: io.Error) { return 1; }
    }
}
SOURCE
reject E_EFFECT
cat >"$work/send.fe" <<'SOURCE'
use std.io;
export func send(bytes: read u8[]) -> Result<usize, io.Error> {
    return io.write(read bytes);
}
SOURCE
"$fermi" build --library "$work/send.fe" --out-dir "$work/library"
grep -q '"heap_bytes": null, "effects": \["io.write"\]' "$work/library/send.contract.json"
cc=${FERMI_CC:-clang}
"$cc" -std=c11 -Wall -Wextra -Werror -Wno-unused-function -Dwrite=fermi_test_write \
    -I"$work/library" tests/integration/io.c "$work/library/send.c" -o "$work/runtime"
"$work/runtime"
echo 'PASS: stdout, empty strings, escaped bytes, unwritable stdout, I/O effects, native isolation'
