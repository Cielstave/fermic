#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
work=build/tests/executable
mkdir -p "$work"

expect_exit() {
    expected=$1
    shift
    actual=0
    "$@" >"$work/stdout" 2>"$work/stderr" || actual=$?
    if [ "$actual" -ne "$expected" ]; then
        cat "$work/stderr" >&2
        echo "FAIL: expected exit $expected, received $actual" >&2
        exit 1
    fi
}

for mode in O0 O2; do
    "$fermi" build examples/cli/exit_status.fe -o "$work/program ; literal" "-$mode"
    expect_exit 0 "$work/program ; literal"
    cp "$work/program ; literal" "$work/previous"
    expect_exit 1 env FERMI_CC=/nonexistent/fermi-cc "$fermi" build \
        examples/cli/exit_status.fe -o "$work/program ; literal" --diagnostic-format=json
    cmp "$work/program ; literal" "$work/previous"
    grep -q '"code":"E_TOOLCHAIN"' "$work/stderr"

    cat >"$work/return.fe" <<'SOURCE'
func main() -> i32 { return 37; }
SOURCE
    "$fermi" build "$work/return.fe" -o "$work/return" "-$mode"
    expect_exit 37 "$work/return"

    cat >"$work/fault.fe" <<'SOURCE'
func divide(value: i32) -> i32 { return 1 / value; }
func main() -> i32 { return divide(0); }
SOURCE
    "$fermi" build "$work/fault.fe" -o "$work/fault" "-$mode"
    expect_exit 1 "$work/fault"
    grep -q 'division by zero' "$work/stderr"

    cat >"$work/range.fe" <<'SOURCE'
func main() -> i32 { return 256; }
SOURCE
    "$fermi" build "$work/range.fe" -o "$work/range" "-$mode"
    expect_exit 1 "$work/range"
    grep -q 'exit status must be between 0 and 255' "$work/stderr"
done

cat >"$work/wrong.fe" <<'SOURCE'
func main() -> i64 { return 0; }
SOURCE
expect_exit 1 "$fermi" build "$work/wrong.fe" -o "$work/wrong" --diagnostic-format=json
grep -q '"code":"E_ENTRY_RETURN_TYPE"' "$work/stderr"
test ! -e "$work/wrong"
expect_exit 1 "$fermi" check "$work/absent.fe" --diagnostic-format=json
grep -q '"code":"E_IO"' "$work/stderr"
test ! -s "$work/stdout"
expect_exit 2 "$fermi" check --wrong --diagnostic-format=json
grep -q '"code":"E_USAGE"' "$work/stderr"
expect_exit 1 "$fermi" build --library examples/cli/exit_status.fe --out-dir "$work/exports" --diagnostic-format=json
grep -q '"code":"E_EXPORT"' "$work/stderr"

cat >"$work/too_large.fe" <<'SOURCE'
func main() -> i32 { return 2147483648; }
SOURCE
expect_exit 1 "$fermi" check "$work/too_large.fe" --diagnostic-format=json
grep -q '"code":"E_RANGE"' "$work/stderr"
echo 'PASS: executable entry, exit status, runtime faults, JSON errors, failed-build preservation'
