#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
root=$(pwd)
case "$fermi" in /*) ;; *) fermi="$root/$fermi" ;; esac
mkdir -p build/tests
work=$(mktemp -d "$root/build/tests/run-cache-XXXXXX")
# The cache deliberately bypasses compiler wrappers and custom header search paths.
unset FERMI_CC CPATH C_INCLUDE_PATH CPLUS_INCLUDE_PATH OBJC_INCLUDE_PATH LIBRARY_PATH SDKROOT
unset COMPILER_PATH GCC_EXEC_PREFIX CCC_OVERRIDE_OPTIONS CLANG_CONFIG_FILE_SYSTEM_DIR
unset CLANG_CONFIG_FILE_USER_DIR SOURCE_DATE_EPOCH
cd "$work"
cat > main.fe <<'SOURCE'
use helper;
func main() -> i32 { return helper.status(); }
SOURCE
cat > helper.fe <<'SOURCE'
pub func status() -> i32 { return 37; }
SOURCE
expect() {
    expected=$1
    shift
    actual=0
    "$@" > stdout 2> stderr || actual=$?
    if [ "$actual" -ne "$expected" ]; then
        cat stderr >&2
        echo "FAIL: expected $expected, received $actual" >&2
        exit 1
    fi
}
entries() {
    find build/.fermi-run -mindepth 1 -maxdepth 1 -type d ! -name '.stage-*' | wc -l
}
expect 37 "$fermi" run
count=$(entries)
test "$count" -eq 1
entry=$(find build/.fermi-run -mindepth 1 -maxdepth 1 -type d)
cp "$entry/program" previous
# Read-only artifacts prove a warm run does not rewrite the executable.
chmod 500 "$entry/program"
chmod 400 "$entry/sha256"
expect 37 "$fermi" run
test "$(entries)" -eq 1
cmp previous "$entry/program"
test ! -s stdout
test ! -s stderr
expect 37 "$fermi" run -O0
test "$(entries)" -eq 2
# An imported module change invalidates native output.
cat > helper.fe <<'SOURCE'
pub func status() -> i32 { return 38; }
SOURCE
expect 38 "$fermi" run
test "$(entries)" -eq 3
# Even a previously built program must be checked again before cache lookup.
printf 'invalid source\n' > helper.fe
expect 1 "$fermi" run
grep -q E_PARSE stderr
test "$(entries)" -eq 3
cat > helper.fe <<'SOURCE'
pub func status() -> i32 { return 37; }
SOURCE
# Corrupt or symlinked cached executables are not executed.
chmod 700 "$entry/program"
printf 'corrupt\n' > "$entry/program"
expect 37 "$fermi" run
cmp previous "$entry/program"
rm "$entry/program"
ln -s "$work/previous" "$entry/program"
expect 37 "$fermi" run
test ! -L "$entry/program"
cmp previous "$entry/program"
# Native compiler configuration changes bypass the cache, including warm entries.
expect 1 env FERMI_CC=/nonexistent/fermi-cc "$fermi" run
grep -q E_TOOLCHAIN stderr
expect 37 env CPATH= "$fermi" run
test "$(entries)" -eq 3
expect 37 "$fermi" run --no-cache
test "$(entries)" -eq 3
expect 2 "$fermi" check main.fe --no-cache
grep -q E_USAGE stderr
# Explicit output uses ordinary publication rather than substituting a cached path.
expect 37 "$fermi" run -o program
test -x program
test "$(entries)" -eq 3
# Concurrent cold runs publish complete entries and leave no staging directories.
cat > helper.fe <<'SOURCE'
pub func status() -> i32 { return 0; }
SOURCE
"$fermi" run > first.out 2> first.err &
first=$!
"$fermi" run > second.out 2> second.err &
second=$!
wait "$first"
wait "$second"
test ! -s first.err
test ! -s second.err
test "$(entries)" -eq 4
test -z "$(find build/.fermi-run -mindepth 1 -maxdepth 1 -name '.stage-*' -print)"
# An unsafe cache root is ignored; source still runs through private temporary output.
chmod 777 build/.fermi-run
expect 0 "$fermi" run
chmod 700 build/.fermi-run
# Cached programs still receive current process arguments, not arguments from build time.
expect 0 "$fermi" run "$root/tests/cases/args_echo.fe" -- first
printf 'first\n' > expected
cmp expected stdout
count=$(entries)
expect 0 "$fermi" run "$root/tests/cases/args_echo.fe" -- second ''
printf 'second\n\n' > expected
cmp expected stdout
test "$(entries)" -eq "$count"
# Loader directory names can contain spaces; their metadata must still invalidate reuse.
mkdir "$work/loader path"
loader="${LD_LIBRARY_PATH:+$LD_LIBRARY_PATH:}$work/loader path"
count=$(entries)
expect 0 env LD_LIBRARY_PATH="$loader" "$fermi" run
count=$((count + 1))
test "$(entries)" -eq "$count"
printf 'changed\n' > "$work/loader path/marker"
expect 0 env LD_LIBRARY_PATH="$loader" "$fermi" run
if [ "$(entries)" -ne "$((count + 1))" ]; then
    echo 'FAIL: changed loader directory with spaces did not invalidate the cache' >&2
    exit 1
fi
echo 'PASS: warm run reuse, source/module invalidation, integrity, concurrency and cache bypass'
