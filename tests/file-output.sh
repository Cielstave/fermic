#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
cc=${CC:-clang}
work=build/tests/file-output
mkdir -p "$work"
for mode in O0 O2; do
    "$fermi" build --library tests/cases/file_output.fe --out-dir "$work" "-$mode"
    if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then
        "$cc" -std=c11 "-$mode" -fsanitize=address,undefined -I"$work" \
            tests/integration/file_output.c -o "$work/owners"
    else
        "$cc" -std=c11 "-$mode" -Wall -Wextra -Werror -Wno-unused-function \
            -Wno-unused-but-set-variable -I"$work" tests/integration/file_output.c -o "$work/owners"
    fi
    "$work/owners"
    "$fermi" build examples/tools/filecopy/main.fe -o "$work/filecopy" "-$mode"
    sandbox=$(mktemp -d "$work/copy-XXXXXX")
    printf 'Fermi\000\377\n' > "$sandbox/input"
    dd if=/dev/zero bs=4096 count=3 >> "$sandbox/input" 2>/dev/null
    "$work/filecopy" "$sandbox/input" "$sandbox/output"
    cmp "$sandbox/input" "$sandbox/output"
    status=0
    "$work/filecopy" "$sandbox/input" "$sandbox/output" 2> "$work/err" || status=$?
    test "$status" -eq 74
    cmp "$sandbox/input" "$sandbox/output"
    ln -s input "$sandbox/link"
    status=0
    "$work/filecopy" "$sandbox/input" "$sandbox/link" 2> "$work/err" || status=$?
    test "$status" -eq 74
    cmp "$sandbox/input" "$sandbox/output"
    : > "$sandbox/empty"
    "$work/filecopy" "$sandbox/empty" "$sandbox/empty-copy"
    test ! -s "$sandbox/empty-copy"
    dd if=/dev/zero of="$sandbox/limit" bs=4096 count=2048 2>/dev/null
    "$work/filecopy" "$sandbox/limit" "$sandbox/limit-copy"
    cmp "$sandbox/limit" "$sandbox/limit-copy"
    printf x >> "$sandbox/limit"
    status=0
    "$work/filecopy" "$sandbox/limit" "$sandbox/over-limit" 2> "$work/err" || status=$?
    test "$status" -eq 65
    test "$(wc -c < "$sandbox/over-limit")" -eq 8388608
    grep -q 'exceeds the 8 MiB limit' "$work/err"
    status=0
    "$work/filecopy" 2> "$work/err" || status=$?
    test "$status" -eq 64
    grep -q '^usage: filecopy SOURCE DESTINATION$' "$work/err"
done
reject() {
    printf 'use std.fs;\n%s\n' "$2" > "$work/rejected.fe"
    if "$fermi" check "$work/rejected.fe" > "$work/out" 2> "$work/err"; then
        echo "FAIL: accepted file output rejection: $2" >&2
        exit 1
    fi
    grep -q "error\[$1\]" "$work/err"
}
reject E_MUT 'func bad(file: fs.File) -> Result<void, fs.Error> { return fs.write_all(edit file, read "x"); }'
reject E_RESULT 'func bad(file: edit fs.File) -> void { fs.write(edit file, read "x"); }'
reject E_INIT 'func bad(file: fs.File) -> Result<void, fs.Error> { try(fs.close(move(file))); return fs.sync(edit file); }'
reject E_EFFECT '@budget(work = 1000, heap = 0) func bad(file: edit fs.File) -> Result<void, fs.Error> { return fs.sync(edit file); }'
reject E_ABI 'export func file_write() -> void {}'
reject E_ABI 'export func file_create_new() -> void {}'
reject E_ABI 'export func file_sync() -> void {}'
"$fermi" explain tests/cases/file_output.fe > "$work/effects"
grep -q 'save:.*"fs.open".*"fs.close".*"fs.write".*"fs.sync"' "$work/effects"
echo 'PASS: filecopy O0/O2, exact limit, overwrite refusal, output effects and semantic guards'
