#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
work=build/tests/driver
mkdir -p "$work"

# The directory name contains shell metacharacters on purpose. The backend must
# receive one path argument, never a shell program assembled from that path.
output="$work/artifacts ; literal"
"$fermi" build --library examples/telemetry/telemetry.fe --out-dir "$output" >"$work/build.log"
test -s "$output/telemetry.o"
cp "$output/telemetry.contract.json" "$work/previous.json"
if FERMI_CC=/nonexistent/fermi-test-compiler "$fermi" build --library \
    examples/telemetry/telemetry.fe --out-dir "$output" >"$work/failure.log" 2>&1; then
    echo 'FAIL: unavailable native backend accepted' >&2
    exit 1
fi
cmp "$output/telemetry.contract.json" "$work/previous.json"

cat >"$work/rejected.fe" <<'SOURCE'
@budget(work = 2, heap = 0)
export func expensive(value: i64) -> i64 { return value + 1; }
SOURCE
if "$fermi" check "$work/rejected.fe" >"$work/rejected.log" 2>&1; then
    echo 'FAIL: tiny resource contract accepted' >&2
    exit 1
fi
grep -q E_BUDGET "$work/rejected.log"

cat >"$work/export_collision.fe" <<'SOURCE'
use std.fs;
export func file_read() -> void {}
SOURCE
cat >"$work/export_pair.fe" <<'SOURCE'
export func inspect() -> u8 { return 1; }
export func inspect_result() -> u8 { return 2; }
SOURCE
for source in export_collision export_pair; do
    for command in check build; do
        if [ "$command" = build ]; then
            set -- build --library "$work/$source.fe" --out-dir "$work"
        else
            set -- check "$work/$source.fe"
        fi
        if "$fermi" "$@" >"$work/$source.$command.log" 2>&1; then
            echo "FAIL: conflicting export accepted by $command" >&2
            exit 1
        fi
        grep -q 'error\[E_ABI\]' "$work/$source.$command.log"
        if grep -q 'E_TOOLCHAIN' "$work/$source.$command.log"; then
            echo 'FAIL: conflicting export reached the C backend' >&2
            exit 1
        fi
    done
done

mkdir -p "$work/export_modules"
cat >"$work/export_modules/other.fe" <<'SOURCE'
export func inspect_result() -> u8 { return 2; }
SOURCE
cat >"$work/export_modules/main.fe" <<'SOURCE'
use other;

export func inspect() -> u8 { return 1; }
SOURCE
if "$fermi" check "$work/export_modules/main.fe" >"$work/export_modules.log" 2>&1; then
    echo 'FAIL: cross-module export collision accepted' >&2
    exit 1
fi
grep -q 'main.fe:3:.*error\[E_ABI\]' "$work/export_modules.log"

mkfifo "$work/fifo.fe"
for source in "$work/fifo.fe" "$work/import_fifo.fe"; do
    if [ "$source" = "$work/import_fifo.fe" ]; then
        printf 'use fifo;\nfunc main() -> void {}\n' > "$source"
    fi
    if timeout 5 "$fermi" check "$source" >"$work/fifo.log" 2>&1; then
        echo 'FAIL: FIFO source accepted' >&2
        exit 1
    fi
    grep -q 'error\[E_IO\]' "$work/fifo.log"
    grep -q 'source must be a regular file' "$work/fifo.log"
done
rm "$work/fifo.fe"

"$fermi" check examples/design/filter_above.fe >"$work/filter.log"
"$fermi" --version >"$work/version.log"
if "$fermi" build --library examples/telemetry/telemetry.fe --bad-option >"$work/usage.log" 2>&1; then
    echo 'FAIL: invalid CLI options accepted' >&2
    exit 1
fi
printf '%s\n' 'PASS: CLI, literal paths, failed-build preservation, budget, ABI and nonregular-source rejection'
