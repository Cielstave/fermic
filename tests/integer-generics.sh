#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
work=build/tests/integer-generics
if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then work=build/tests/integer-generics-sanitize; fi
mkdir -p "$work"
for mode in O0 O2; do
    "$fermi" test tests/cases/integer_generics.fe "-$mode" > "$work/results"
    grep -q '^test result: ok. 8 passed; 0 failed$' "$work/results"
    "$fermi" test examples/features/integer_generics/main.fe "-$mode" > "$work/example-tests"
    grep -q '^test result: ok. 3 passed; 0 failed$' "$work/example-tests"
    "$fermi" run examples/features/integer_generics/main.fe --no-cache "-$mode" > "$work/out"
    printf 'integer-generics: checks passed\n' > "$work/expected"
    cmp "$work/expected" "$work/out"
done
"$fermi" explain tests/cases/integer_generics.fe > "$work/explain"
grep -q '^larger<u8>: static upper bound' "$work/explain"
grep -q '^larger<i64>: static upper bound' "$work/explain"
grep -q '^consume<u32>:.*effects: \["memory"\]' "$work/explain"
cat > "$work/private.fe" <<'SOURCE'
func identity<T: integer>(value: T) -> T { return value; }
SOURCE
cat > "$work/rejected.fe" <<'SOURCE'
use private;
func main() -> i32 { return private.identity<i32>(7); }
SOURCE
if "$fermi" check "$work/rejected.fe" > "$work/out" 2> "$work/err"; then exit 1; fi
grep -q 'error\[E_PRIVATE\]' "$work/err"
cat > "$work/fault.fe" <<'SOURCE'
func add<T: integer>(left: T, right: T) -> T { return left + right; }
func main() -> i32 {
    let value: u8 = add<u8>(255, 1);
    return 0;
}
SOURCE
status=0
"$fermi" run "$work/fault.fe" --no-cache > "$work/out" 2> "$work/err" || status=$?
test "$status" -eq 1
grep -q 'runtime fault\[2\]: integer overflow' "$work/err"
# Distinct modules may use the same generic name without sharing specializations.
printf 'pub func identity<T: integer>(value: T) -> T { return value; }\n' > "$work/first.fe"
printf 'pub func identity<T: integer>(value: T) -> T { return value + 1; }\n' > "$work/second.fe"
cat > "$work/modules.fe" <<'SOURCE'
use first;
use second;
func test_modules() -> bool {
    return first.identity<u16>(7) == 7 && second.identity<u16>(7) == 8;
}
SOURCE
"$fermi" test "$work/modules.fe" > "$work/results"
grep -q '^test result: ok. 1 passed; 0 failed$' "$work/results"
# Unrequested bodies are syntax-checked, but do not create concrete functions.
cat > "$work/unused.fe" <<'SOURCE'
func invalid<T: integer>(value: T) -> T { return -value; }
func main() -> void {}
SOURCE
"$fermi" check "$work/unused.fe" > "$work/out"
"$fermi" explain "$work/unused.fe" > "$work/explain"
if grep -q '^invalid<' "$work/explain"; then exit 1; fi
cat > "$work/rejected.fe" <<'SOURCE'
func invalid<T: integer>(value: T) -> T { return -value; }
func main() -> void { let value: u8 = invalid<u8>(1); }
SOURCE
if "$fermi" check "$work/rejected.fe" > "$work/out" 2> "$work/err"; then exit 1; fi
grep -q 'error\[E_TYPE\]' "$work/err"
node - "$work" <<'JS'
const fs = require('fs');
const directory = process.argv[2];
let declarations = '';
for (let index = 0; index < 128; index++) {
    declarations += `func identity_${index}<T: integer>(value: T) -> T { return value; }\n`;
}
fs.writeFileSync(`${directory}/unused-limit.fe`, declarations);
fs.writeFileSync(`${directory}/declaration-limit.fe`, declarations + 'func extra<T: integer>(value: T) -> T { return value; }\n');
let calls = '';
for (let index = 0; index < 127; index++) {
    calls += `let value_${index}: u8 = identity_${index}<u8>(1);\n`;
}
fs.writeFileSync(`${directory}/limit.fe`, declarations + `func main() -> void { ${calls} }\n`);
fs.writeFileSync(`${directory}/instance-limit.fe`, declarations + `func main() -> void { ${calls} let extra: u8 = identity_127<u8>(1); }\n`);
JS
"$fermi" check "$work/unused-limit.fe" > "$work/out"
"$fermi" check "$work/limit.fe" > "$work/out"
if "$fermi" check "$work/declaration-limit.fe" > "$work/out" 2> "$work/err"; then exit 1; fi
grep -q 'too many generic declarations' "$work/err"
if "$fermi" check "$work/instance-limit.fe" > "$work/out" 2> "$work/err"; then exit 1; fi
grep -q 'too many functions, including requested generic specializations' "$work/err"
echo 'PASS: requested integer specializations, modules, checked faults, resources and limits'
