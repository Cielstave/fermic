#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
case "$fermi" in /*) ;; *) fermi="$(pwd)/$fermi" ;; esac
# Keep an explicitly selected backend usable in the default-source directory.
case "${FERMI_CC:-}" in
    /*|"") ;;
    */*) FERMI_CC="$(pwd)/$FERMI_CC"; export FERMI_CC ;;
esac
work=build/tests/test-command
if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then work=build/tests/test-command-sanitize; fi
mkdir -p "$work/tmp"
cat > "$work/helper.fe" <<'SOURCE'
pub func value() -> u8 { return 7; }
func test_imported() -> bool { return false; }
func testImportedCamel() -> bool { return false; }
SOURCE
cat > "$work/tests.fe" <<'SOURCE'
use helper;
use std.args;
use std.mem;
func test_value() -> bool { return helper.value() == 7; }
func testCamelCase() -> bool { return true; }
func testhelper() -> u8 { return 7; }
func test() -> u8 { return 7; }
func test_arguments() -> bool {
    match (args.count()) {
        case Result.ok(let count: usize) { return count == 0; }
        case Result.err(let error: args.Error) { return false; }
    }
}
func test_owner() -> bool {
    match (mem.from_bytes(read "Fe", 2)) {
        case Result.ok(let buffer: Buffer<u8>) { return buffer.len == 2 && buffer[1] == 101; }
        case Result.err(let error: mem.Error) { return false; }
    }
}
func main() -> i32 { return 99; }
SOURCE
for mode in O0 O2; do
    TMPDIR="$(pwd)/$work/tmp" "$fermi" test "$work/tests.fe" "-$mode" > "$work/pass"
    grep -q '^test result: ok. 4 passed; 0 failed$' "$work/pass"
    if grep -q 'test_imported\|testImportedCamel' "$work/pass"; then exit 1; fi
    "$fermi" test examples/testing/codec/tests.fe "-$mode" > "$work/codec"
    grep -q '^test result: ok. 3 passed; 0 failed$' "$work/codec"
done
(cd "$work" && "$fermi" t > default)
grep -q '^running 4 tests$' "$work/default"
cat > "$work/fail.fe" <<'SOURCE'
use std.mem;
func divide(value: usize) -> usize { return 1 / value; }
func test_false() -> bool { return false; }
func test_fault() -> bool {
    match (mem.bytes(8, 8)) {
        case Result.ok(let buffer: Buffer<u8>) { return divide(0) == 1; }
        case Result.err(let error: mem.Error) { return false; }
    }
}
func testAfterFault() -> bool { return true; }
SOURCE
status=0
TMPDIR="$(pwd)/$work/tmp" "$fermi" test "$work/fail.fe" > "$work/fail" || status=$?
test "$status" -eq 1
grep -q '^test test_false ... FAILED$' "$work/fail"
grep -q '^test test_fault ... FAILED (runtime fault 3)$' "$work/fail"
grep -q '^test testAfterFault ... ok$' "$work/fail"
grep -q '^test result: FAILED. 1 passed; 2 failed$' "$work/fail"
reject() {
    printf '%s\n' "$2" > "$work/rejected.fe"
    if "$fermi" test "$work/rejected.fe" > "$work/out" 2> "$work/err"; then exit 1; fi
    grep -q "error\[$1\]" "$work/err"
}
reject E_TEST 'func testBad(value: u8) -> bool { return true; }'
reject E_TEST 'func testBad() -> void {}'
reject E_TEST 'func helper() -> bool { return true; }'
reject E_TYPE 'func test_bad() -> bool { return 7; }'
reject E_PARSE 'func testGeneric<T: integer>() -> bool { return true; }'
"$fermi" help test > "$work/help"
grep -q 'Usage: fermi test' "$work/help"
for option in --library --no-cache -- --out-dir -o; do
    status=0
    "$fermi" test "$work/tests.fe" "$option" > "$work/out" 2> "$work/err" || status=$?
    test "$status" -eq 2
done
status=0
FERMI_CC=/nonexistent/fermi-test-compiler TMPDIR="$(pwd)/$work/tmp" \
    "$fermi" test "$work/tests.fe" > "$work/out" 2> "$work/err" || status=$?
test "$status" -eq 1
grep -q 'E_TOOLCHAIN' "$work/err"
test -z "$(ls -A "$work/tmp")"
echo 'PASS: native test discovery, camelCase names, defaults, faults, imports, CLI and cleanup'
