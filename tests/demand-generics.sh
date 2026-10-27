#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
work=build/tests/demand-generics
if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then work=build/tests/demand-generics-sanitize; fi
mkdir -p "$work"
cat > "$work/model.fe" <<'SOURCE'
pub type Number = i16;
pub func apply<T: integer>(value: T) -> T { return helper<T>(value); }
func helper<T: integer>(value: T) -> T { return value + 1; }
pub func number<T: integer>(value: T) -> Number { return 17; }
pub func recurse<T: integer>(value: T) -> T {
    if (value == 0) { return value; }
    return recurse<T>(value - 1);
}
func unused<T: integer>(value: T) -> T { return -value; }
SOURCE
cat > "$work/main.fe" <<'SOURCE'
use "model.fe";
type Number = u64;
func helper<T: integer>(value: T) -> T { return value + 100; }
func test_module_instances() -> bool {
    let first: i16 = model.number<u8>(1);
    return model.apply<u32>(7) == 8 && model.apply<u32>(9) == 10 &&
           model.apply<i16>(-2) == -1 && first == 17 &&
           model.recurse<u8>(10) == 0;
}
SOURCE
for mode in O0 O2; do
    "$fermi" test "$work/main.fe" "-$mode" > "$work/results"
    grep -q '^test result: ok. 1 passed; 0 failed$' "$work/results"
done
"$fermi" explain "$work/main.fe" > "$work/explain"
test "$(grep -c '^local[.]model[.]fe[.]apply<' "$work/explain")" -eq 2
test "$(grep -c '^local[.]model[.]fe[.]helper<' "$work/explain")" -eq 2
test "$(grep -c '^local[.]model[.]fe[.]recurse<' "$work/explain")" -eq 1
if grep -q '^helper<\|[.]unused<' "$work/explain"; then exit 1; fi
cat > "$work/broken.fe" <<'SOURCE'
pub func broken<T: integer>(value: T) -> T { return true; }
SOURCE
cat > "$work/rejected.fe" <<'SOURCE'
use "broken.fe";
func f() -> u8 { return broken.broken<u8>(1); }
SOURCE
if "$fermi" check "$work/rejected.fe" > "$work/out" 2> "$work/err"; then exit 1; fi
grep -q 'broken.fe:1:' "$work/err"
grep -q 'error\[E_TYPE\]' "$work/err"
cat > "$work/rejected.fe" <<'SOURCE'
func unused<T: integer>(value: T) -> T { return value + ; }
SOURCE
if "$fermi" check "$work/rejected.fe" > "$work/out" 2> "$work/err"; then exit 1; fi
grep -q 'error\[E_PARSE\]' "$work/err"
printf 'PASS: demand specialization, deduplication, nested calls, source modules and diagnostics\n'
