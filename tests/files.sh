#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
cc=${CC:-clang}
work=build/tests/files
mkdir -p "$work/std"
printf 'invalid standard library shadow\n' > "$work/std/fs.fe"
cp tests/cases/files.fe "$work/files.fe"
printf 'Fermi\n' > "$work/input"
dd if=/dev/zero of="$work/large" bs=4096 count=3 2>/dev/null
printf 'tail' >> "$work/large"
for mode in O0 O2; do
    "$fermi" build --library "$work/files.fe" --out-dir "$work" "-$mode"
    "$fermi" build --library tests/cases/files_second.fe --out-dir "$work" "-$mode"
    if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then
        "$cc" -std=c11 "-$mode" -fsanitize=address,undefined -iquote"$work" \
            tests/integration/file_ownership.c -o "$work/run"
        "$cc" -std=c11 "-$mode" -fsanitize=address,undefined -iquote"$work" \
            tests/integration/file_abi.c "$work/files.o" "$work/files_second.o" -o "$work/abi"
    else
        "$cc" -std=c11 "-$mode" -Wall -Wextra -Werror -Wno-unused-function \
            -Wno-unused-but-set-variable -iquote"$work" tests/integration/file_ownership.c -o "$work/run"
        "$cc" -std=c11 "-$mode" -Wall -Wextra -Werror -iquote"$work" \
            tests/integration/file_abi.c "$work/files.o" "$work/files_second.o" -o "$work/abi"
    fi
    "$work/run"
    "$work/abi" "$work/input"
    "$fermi" build examples/tools/filecat/main.fe -o "$work/filecat" "-$mode"
    for file in "$work/input" "$work/large" /dev/null; do
        if [ "$file" = /dev/null ]; then
            : > "$work/empty"
            file="$work/empty"
        fi
        "$work/filecat" "$file" > "$work/out" 2> "$work/err"
        cmp "$file" "$work/out"
        test ! -s "$work/err"
    done
    for path in "$work/absent" "$work" /dev/null; do
        status=0
        "$work/filecat" "$path" > "$work/out" 2> "$work/err" || status=$?
        test "$status" -eq 66
        test ! -s "$work/out"
        grep -q 'cannot open a regular input file' "$work/err"
    done
    status=0
    "$work/filecat" > "$work/out" 2> "$work/err" || status=$?
    test "$status" -eq 64
    grep -q '^usage: filecat PATH$' "$work/err"
    status=0
    "$work/filecat" "$work/input" 1< "$work/input" 2> "$work/err" || status=$?
    test "$status" -eq 74
    grep -q 'cannot write standard output' "$work/err"
done
reject() {
    printf 'use std.fs;\n%s\n' "$2" > "$work/rejected.fe"
    if "$fermi" check "$work/rejected.fe" > "$work/out" 2> "$work/err"; then
        echo 'FAIL: invalid file ownership accepted' >&2
        exit 1
    fi
    grep -q "$1" "$work/err"
}
reject E_MOVE 'func bad(file: fs.File) -> void { copy(file); }'
reject E_TYPE 'func bad() -> void { fs.File(); }'
reject E_TYPE 'func bad(file: fs.File) -> i32 { return file.descriptor; }'
reject E_MOVE 'struct Holder { file: fs.File; } func bad(holder: Holder) -> fs.File { return move(holder.file); }'
reject E_PARSE 'func bad(files: fs.File[2]) -> void {}'
reject E_PARSE 'func bad(file: read fs.File) -> void {}'
reject E_PARSE 'func bad(file: edit fs.File) -> edit fs.File { return file; }'
reject E_PARSE 'func bad(file: Result<edit fs.File, fs.Error>) -> void {}'
reject E_PARSE 'func bad(file: Result<Result<fs.File, fs.Error>, fs.Error>) -> void {}'
reject E_TYPE 'func bad() -> void { var file: edit fs.File; }'
reject E_ABI 'export func bad(file: fs.File) -> void {}'
reject E_ABI 'export func bad(file: edit fs.File) -> void {}'
reject E_ABI 'export func bad(path: read u8[]) -> Result<fs.File, fs.Error> { return fs.open_read(read path); }'
reject E_MUT 'func bad(file: fs.File, output: edit u8[]) -> Result<usize, fs.Error> {
    return fs.read_into(edit file, edit output);
}'
reject E_RESULT 'func bad(file: fs.File) -> void { fs.close(move(file)); }'
reject E_INIT 'func bad(file: fs.File) -> void {
    match (fs.close(move(file))) {
        case Result.ok() {}
        case Result.err(let error: fs.Error) { fs.close(move(file)); }
    }
}'
reject E_MOVE 'func consume(file: fs.File) -> void {}
func bad(file: fs.File) -> void { consume(file); }'
reject E_MUT 'func bad(file: edit fs.File, other: fs.File) -> void { file = move(other); }'
reject E_ALIAS 'func pair(first: edit fs.File, second: edit fs.File) -> void {}
func bad(file: fs.File) -> void { var active: fs.File = move(file); pair(edit active, edit active); }'
reject E_ALIAS 'func count(file: fs.File) -> usize { return 0; }
func use_file(file: edit fs.File, length: usize) -> void {}
func bad(file: fs.File) -> void {
    var active: fs.File = move(file); use_file(edit active, count(move(active)));
}'
reject E_EFFECT '@budget(work = 1000, heap = 0)
func bad(file: fs.File) -> void {}'
grep -q 'file access or cleanup' "$work/err"
reject E_EFFECT '@budget(work = 1000, heap = 0)
func bad(file: edit fs.File, output: edit u8[]) -> Result<usize, fs.Error> {
    return fs.read_into(edit file, edit output);
}'
"$fermi" explain "$work/files.fe" > "$work/effects"
grep -q 'read_once:.*effects: \["fs.read"\]' "$work/effects"
grep -q 'forwarded:.*effects: \["fs.close"\]' "$work/effects"
grep -q 'first:.*effects: \["fs.open", "fs.read", "fs.close"\]' "$work/effects"
grep -q '"name": "first".*"heap_bytes": null.*"effects": \["fs.open", "fs.read", "fs.close"\]' "$work/files.contract.json"
echo 'PASS: file owner isolation, source rejection, effect propagation and native filecat'

# Exercise explicit close errors in the actual Fermi application as well.
cat > "$work/fail-close.h" <<'SOURCE'
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <unistd.h>
static int failed_close(int descriptor) {
    int result = close(descriptor);
    if (result < 0) { return result; }
    errno = EIO;
    return -1;
}
#define close(...) failed_close(__VA_ARGS__)
SOURCE
cat > "$work/fail-close-cc.sh" <<'SOURCE'
#!/bin/sh
if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then
    exec "${CC:-clang}" -fsanitize=address,undefined -include build/tests/files/fail-close.h "$@"
fi
exec "${CC:-clang}" -include build/tests/files/fail-close.h "$@"
SOURCE
chmod +x "$work/fail-close-cc.sh"
FERMI_CC="$work/fail-close-cc.sh" "$fermi" build examples/tools/filecat/main.fe -o "$work/fail-close" -O2
status=0
"$work/fail-close" "$work/input" > "$work/out" 2> "$work/err" || status=$?
test "$status" -eq 74
cmp "$work/input" "$work/out"
grep -q 'cannot close input file' "$work/err"
status=0
"$work/fail-close" "$work/input" 1< "$work/input" 2> "$work/err" || status=$?
test "$status" -eq 74
grep -q 'cannot write standard output' "$work/err"
if grep -q 'cannot close input file' "$work/err"; then
    echo 'FAIL: close error replaced the primary I/O failure' >&2
    exit 1
fi
echo 'PASS: explicit close failure and primary-error preservation in filecat'
