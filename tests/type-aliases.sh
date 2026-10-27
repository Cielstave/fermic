#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
work=build/tests/type-aliases
mkdir -p "$work"
for mode in O0 O2; do
    "$fermi" test tests/cases/type_aliases.fe "-$mode" > "$work/tests"
    grep -q '^test result: ok. 6 passed; 0 failed$' "$work/tests"
    "$fermi" test examples/features/type_aliases/main.fe "-$mode" > "$work/example-tests"
    grep -q '^test result: ok. 1 passed; 0 failed$' "$work/example-tests"
    "$fermi" run examples/features/type_aliases/main.fe --no-cache "-$mode" > "$work/output"
    printf 'type-aliases: ready\n' > "$work/expected"
    cmp "$work/expected" "$work/output"
done
cat > "$work/types.fe" <<'SOURCE'
type Private = u32;
pub type Count = usize;
SOURCE
cat > "$work/private.fe" <<'SOURCE'
use "types.fe";
func f(value: types.Private) -> void {}
SOURCE
if "$fermi" check "$work/private.fe" > "$work/out" 2> "$work/err"; then
    echo 'FAIL: private alias was accessible' >&2
    exit 1
fi
grep -q 'E_PARSE' "$work/err"
cat > "$work/conflict.fe" <<'SOURCE'
use "types.fe";
type types = usize;
SOURCE
if "$fermi" check "$work/conflict.fe" > "$work/out" 2> "$work/err"; then
    echo 'FAIL: alias shadowed module import' >&2
    exit 1
fi
grep -q 'E_MODULE' "$work/err"
index=0
previous=usize
while [ "$index" -lt 1024 ]; do
    printf 'type Alias%s = %s;\n' "$index" "$previous"
    previous=Alias$index
    index=$((index + 1))
done > "$work/limit.fe"
"$fermi" check "$work/limit.fe" > "$work/out"
printf 'type Excess = usize;\n' >> "$work/limit.fe"
if "$fermi" check "$work/limit.fe" > "$work/out" 2> "$work/err"; then
    echo 'FAIL: type alias limit was not enforced' >&2
    exit 1
fi
grep -q 'type alias limit exceeded' "$work/err"
echo 'PASS: transparent type aliases, modules, visibility, resources, limits and native execution'
