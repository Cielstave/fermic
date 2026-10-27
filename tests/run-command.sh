#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
cc=${CC:-clang}
root=$(pwd)
case "${FERMI_CC:-}" in
    ""|/*) ;;
    */*) FERMI_CC="$root/$FERMI_CC"; export FERMI_CC ;;
esac
case "$fermi" in /*) ;; *) fermi="$root/$fermi" ;; esac
work="$root/build/tests/run-command"
mkdir -p "$work/tmp ; literal" "$work/project"
export TMPDIR="$work/tmp ; literal"

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
    test -z "$(ls -A "$TMPDIR")"
}

for arguments in '' '--help' '-h' 'help'; do
    if [ -z "$arguments" ]; then expect_exit 0 "$fermi"; else expect_exit 0 "$fermi" "$arguments"; fi
    grep -q 'Fermi native compiler' "$work/stdout"
    grep -q 'run, r' "$work/stdout"
    test ! -s "$work/stderr"
done
expect_exit 0 "$fermi" help run
grep -q 'default: main.fe' "$work/stdout"
expect_exit 0 "$fermi" r --help
grep -q 'Build and run' "$work/stdout"
expect_exit 0 "$fermi" -V
grep -q 'fermi 0.2.0-pilot' "$work/stdout"
expect_exit 2 "$fermi" help unknown
expect_exit 2 "$fermi" run --no-cache --library --diagnostic-format=json
grep -q '"code":"E_USAGE"' "$work/stderr"
expect_exit 2 "$fermi" run --no-cache --out-dir "$work/output"
expect_exit 2 "$fermi" check tests/cases/args_echo.fe -- argument
expect_exit 2 "$fermi" run --no-cache tests/cases/args_echo.fe unexpected

cat > "$work/status.fe" <<'SOURCE'
func main() -> i32 { return 37; }
SOURCE
cat > "$work/fault.fe" <<'SOURCE'
func divide(value: i32) -> i32 { return 1 / value; }
func main() -> i32 { return divide(0); }
SOURCE
for mode in O0 O2; do
    expect_exit 37 "$fermi" run --no-cache "$work/status.fe" "-$mode"
    test ! -s "$work/stdout"
    test ! -s "$work/stderr"
    expect_exit 1 "$fermi" run --no-cache "$work/fault.fe" "-$mode"
    grep -q 'runtime fault\[3\]: division by zero' "$work/stderr"
    expect_exit 0 "$fermi" r --no-cache tests/cases/args_echo.fe "-$mode" -- \
        first 'two words' '' --help '-O0' 'semi;colon' '$HOME'
    printf 'first\ntwo words\n\n--help\n-O0\nsemi;colon\n$HOME\n' > "$work/expected"
    cmp "$work/expected" "$work/stdout"
    test ! -s "$work/stderr"
    expect_exit 37 "$fermi" run --no-cache "$work/status.fe" -o "$work/program ; literal" "-$mode"
    test -x "$work/program ; literal"
    cp "$work/program ; literal" "$work/previous"
    expect_exit 1 env FERMI_CC=/nonexistent/fermi-cc "$fermi" run --no-cache \
        "$work/status.fe" -o "$work/program ; literal" --diagnostic-format=json
    cmp "$work/previous" "$work/program ; literal"
    grep -q '"code":"E_TOOLCHAIN"' "$work/stderr"
    expect_exit 1 env FERMI_CC=/nonexistent/fermi-cc "$fermi" run --no-cache "$work/status.fe"
done

expect_exit 0 "$fermi" run --no-cache examples/tools/bytecount/main.fe -- 100 <<'INPUT'
Fermi
INPUT
printf '6\n' > "$work/expected"
cmp "$work/expected" "$work/stdout"
expect_exit 64 "$fermi" run --no-cache examples/tools/bytecount/main.fe
grep -q 'usage:' "$work/stderr"

cp tests/cases/args_echo.fe "$work/project/main.fe"
(
    cd "$work/project"
    expect_exit 0 "$fermi" run --no-cache -- default
    printf 'default\n' > "$work/expected"
    cmp "$work/expected" "$work/stdout"
    printf 'relative file access\n' > input
    expect_exit 0 "$fermi" run --no-cache "$root/examples/tools/filecat/main.fe" -- input
    cmp input "$work/stdout"
)
expect_exit 1 "$fermi" run --no-cache "$work/absent.fe" -- --diagnostic-format=json
grep -q 'error\[E_IO\]' "$work/stderr"
expect_exit 1 "$fermi" run --no-cache examples/telemetry/telemetry.fe --diagnostic-format=json
grep -q '"code":"E_ENTRY_MISSING"' "$work/stderr"
expect_exit 1 env TMPDIR="$work/status.fe" "$fermi" run --no-cache "$work/status.fe"
grep -q E_RUN "$work/stderr"

# Substitute only the native backend output to exercise OS-level runner failures.
"$cc" -std=c11 -Wall -Wextra -Werror tests/integration/run_process.c -o "$work/process"
cat > "$work/backend" <<'BACKEND'
#!/bin/sh
set -eu
output=
while [ "$#" -gt 0 ]; do
    if [ "$1" = -o ]; then shift; output=$1; fi
    shift
done
test -n "$output"
cp "$FERMI_RUN_FIXTURE" "$output"
BACKEND
chmod +x "$work/backend"
for pair in 'interrupt 130' 'terminate 143' 'exit127 127'; do
    set -- $pair
    expect_exit "$2" env FERMI_CC="$work/backend" FERMI_RUN_FIXTURE="$work/process" \
        "$fermi" run --no-cache "$work/status.fe" -- "$1"
    test ! -s "$work/stdout"
    test ! -s "$work/stderr"
done
printf 'exit 0\n' > "$work/plain"
expect_exit 1 env FERMI_CC="$work/backend" FERMI_RUN_FIXTURE="$work/plain" \
    "$fermi" run --no-cache "$work/status.fe" --diagnostic-format=json
grep -q '"code":"E_RUN"' "$work/stderr"
test ! -s "$work/stdout"
chmod +x "$work/plain"
expect_exit 1 env FERMI_CC="$work/backend" FERMI_RUN_FIXTURE="$work/plain" \
    "$fermi" run --no-cache "$work/status.fe" --diagnostic-format=json
grep -q '"code":"E_RUN"' "$work/stderr"
test ! -s "$work/stdout"
echo 'PASS: run command, help, literal arguments, streams, statuses, signals and temporary cleanup'
